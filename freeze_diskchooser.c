/*
  Disk chooser for freeze menu.

  It is displayed over the top of the normal freeze menu,
  and so we use that screen mode.

  We get our list of disknames and put them at $40000.
  As we only care about their names, and file names are
  limited to 64 characters, we can fit ~1000.
  In fact, we can only safely mount images with names <32
  characters.

  We return the disk image name or a NULL pointer if the
  selection has failed and $FFFF if the user cancels selection
  of a disk.
*/

#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "freezer_common.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

extern unsigned short slot_number;

short file_count = 0, min_dir_entry = 0;
short selection_number = 0;
short display_offset = 0;

char* reading_disk_list_message = "SCANNING DIRECTORY ...";

char* diskchooser_instructions = "  SELECT DISK IMAGE, THEN PRESS RETURN  "
                                 "  OR PRESS RUN/STOP TO LEAVE UNCHANGED  "
                                 "UNMOUNT CURRENT  ";

// use DMA lcopy overlap trick to save space!
unsigned char normal_row[4] = { 0, 1, 0, 1 };
unsigned char error_row[4] = { 0, 2, 0, 2 };
unsigned char highlight_row[4] = { 0, 0x21, 0, 0x21 };
unsigned char dir_line_colour[4] = { 0, 0xe, 0, 0xe };

char disk_name_return[32];

unsigned char joy_to_key_disk[32] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0d,         // With fire pressed
  0, 0, 0, 0, 0, 0, 0, 0x9d, 0, 0, 0, 0x1d, 0, 0x11, 0x91, 0 // without fire
};

static char default_error[] = "ERROR CODE XX";
char* hyppoerror_to_screen(unsigned char error)
{
  // don't add to many errors, this is lot of space!
  switch (error) {
  /*
    case 0x07:
      return "READ TIMEOUT";
    case 0x11:
      return "ILLEGAL VALUE";
  */
    case 0x20:
      return "READ ERROR";
  /*
    case 0x21:
      return "WRITE ERROR";
    case 0x80:
      return "NO SUCH DRIVE";
    case 0x81:
      return "NAME TO LONG";
    case 0x82:
      return "NOT IMPLEMENTED";
    case 0x83:
      return "FILE TO LONG";
    case 0x84:
      return "TO MANY OPEN FILES";
  */
    case 0x85:
      return "INVALID CLUSTER";
  /*
    case 0x86:
      return "IS A DIRECTORY";
    case 0x87:
      return "NOT A DIRECTORY";
  */
    case 0x88:
      return "FILE NOT FOUND";
  /*
    case 0x89:
      return "INVALID FILE DESCR";
  */
    case 0x8a:
      return "WRONG IMAGE LENGTH";
    case 0x8b:
      return "IMAGE FRAGMENTED";
  /*
    case 0x8c:
      return "NO SPACE LEFT";
    case 0x8d:
      return "FILE EXISTS";
    case 0x8e:
      return "DIRECTORY FULL";
    case 0xff:
      return "NO SUCH TRAP / EOF";
  */
  }
  default_error[11] = (error >> 4) + (((error >> 4) < 10) ? 0x30 : 0x37);
  default_error[12] = (error & 0xf) + (((error & 0xf) < 10) ? 0x30 : 0x37);
  return default_error;
}

#define DISK_TYPE_D81 0
#define DISK_TYPE_D64 1
#define DISK_TYPE_D65 2
#define DISK_TYPE_D71 3
static unsigned char disk_type, current_sector, dir_track, entries, cur_row, next_sector, messed_up = 0;
static unsigned char current_side = 0, entry_buffer[18] = "\"                 ";

void display_error(unsigned char error)
{
  unsigned char i, *errstr;

  POKE(0xD020U, 2);
  errstr = hyppoerror_to_screen(error);
  for (i = 0; i < 19 && errstr[i]; i++) {
    POKE(SCREEN_ADDRESS + (21 * 2) + (i * 2), petscii_to_screen(errstr[i]));
    lpoke(COLOUR_RAM_ADDRESS + (21 * 2) + 1 + (i * 2), 0x02); // errors are red
  }
  lcopy((long)error_row, COLOUR_RAM_ADDRESS + (21 * 2), 4);
  lcopy(COLOUR_RAM_ADDRESS + (21 * 2), COLOUR_RAM_ADDRESS + (21 * 2) + 4, 19 * 2 - 4);
}

void draw_directory_entry(unsigned char screen_row)
{
  unsigned char i;

  for (i = 0; i < 18; i++)
    POKE(SCREEN_ADDRESS + (screen_row * 80) + (21 * 2) + (i * 2), entry_buffer[i]);

  lcopy((unsigned long)dir_line_colour, COLOUR_RAM_ADDRESS + (screen_row * 80) + (21 * 2), 4);
  lcopy(COLOUR_RAM_ADDRESS + (screen_row * 80) + (21 * 2), COLOUR_RAM_ADDRESS + (screen_row * 80) + (21 * 2) + 4, (19 * 2 - 4));
}

unsigned char next_directory_entry(void)
{
  unsigned char c, i, type = 0;

  if (disk_type == DISK_TYPE_D81 || disk_type == DISK_TYPE_D64) {
    // D81 || D64
    i = PEEK(0xD087U); // track next dir
    c = PEEK(0xD087U); // sector next dir
    if (next_sector == 255) { // only first two bytes of sector count!
      if (i > 0 && c > 1 && c < 41) // track 0 means end of dir
        next_sector = c;
      else
        next_sector = 254; // first two bytes of sector read
    }
    type = PEEK(0xD087U); // file type
    c = PEEK(0xD087U); // track file
    c = PEEK(0xD087U); // sector file
    // now 16 char filename
    if (type) { // valid
      entry_buffer[17] = ' ';
      for (i = 1; i < 17; i++)
        entry_buffer[i] = petscii_to_screen(PEEK(0xD087U));
      for (; (entry_buffer[i] & 0xbf) == ' ' && i > 1; i--); // this might be 0x20 or 0x60
      entry_buffer[i+1] = '"';

      // skip rest up to 32 bytes
      for (i = 0; i < 11; i++)
        c = PEEK(0xD087U);
    }
    else
      for (i = 0; i < 27; i++)
        c = PEEK(0xD087U);
  }

  return type;
}

void draw_entries(void)
{
  unsigned char i;

  // next_sector = 255 -> first entry of sector to be read
  // next_sector = 254 -> first already read, no valid dir pointer
  // next_sector < 41  -> next dir sector 
  next_sector = 255;
  for (i = 0; i < entries; i++) {
    if (next_directory_entry()) {
      draw_directory_entry(cur_row);
      cur_row++;
    }
    if (cur_row >= 23)
      break;
    if (i == 8) // D81 two sectors at once, so read next pointer
      next_sector = 255;
  }
}

int read_sector_with_cancel(void)
{
  POKE(0xD084U, dir_track);
  POKE(0xD085U, current_sector);
  POKE(0xD086U, current_side);
  while (PEEK(0xD082U) & 0x80) {
    // Exit if a key has been pressed
    if (PEEK(0xD610U)) {
      POKE(0xD080U, 0);
      return 0;
    }
  }
  POKE(0xD081U, 0x41); // Read sector
  while (PEEK(0xD082U) & 0x80) {
    // Exit if a key has been pressed
    if (PEEK(0xD610U)) {
      POKE(0xD080U, 0);
      return 0;
    }
  }
  if (PEEK(0xD082U) & 0x18)
    return 0; // abort if the sector read failed

  return 1;
}

unsigned char draw_directory_contents(unsigned char drive_id)
{
  unsigned char c, i, x;
  unsigned char err;
  short skip_bytes, j;

  // only work on drive 0 and 1
  if (drive_id > 1)
    return 0;

  lcopy(0x40000L + (selection_number * 64), (unsigned long)disk_name_return, 32);

  // Don't draw directories
  if (disk_name_return[0] == '/')
    return 0;

  // null terminate disk_name_return
  for (x = 31; x && disk_name_return[x] == ' '; x--)
    disk_name_return[x] = 0;

  // Try to mount it, with border black while working
  POKE(0xD020U, 0);
  if (drive_id == 0)
    err = mega65_dos_d81attach0(disk_name_return);
  else if (drive_id == 1)
    err = mega65_dos_d81attach1(disk_name_return);
  else
    err = 1;
  if (err) {
    // Mounting the image failed
    display_error(err);
    return 1;
  }
  POKE(0xD020U, 6);

  // Exit if a key has been pressed
  if (PEEK(0xD610U))
    return 1;

  // determine disk image type
  // d68a.6/7 -> d64 flag
  // d68b.6/7 -> d65 flag
  disk_type = ((PEEK(0xd68b) >> (5 + drive_id)) & 0x2) | ((PEEK(0xd68a) >> (6 + drive_id)) & 0x1);
  switch (disk_type) {
    case DISK_TYPE_D81:
      dir_track = 39;
      current_side = 0;
      current_sector = 1;
      skip_bytes = 4;
      break;
    case DISK_TYPE_D64:
      dir_track = 8;
      current_side = 1;
      current_sector = 9;
      skip_bytes = 256 + 0x90;
      break;
    case DISK_TYPE_D65:
    case DISK_TYPE_D71:
      // write_entry
      return 1; // not supported
  }

  // Mounted disk, so now get the directory.

  // Read T40 S1 (sectors begin at 1, not 0)
  POKE(0xD080U, 0x60 | drive_id); // motor and LED on, and select correct drive
  POKE(0xD081U, 0x20); // Wait for motor spin up

  if (!read_sector_with_cancel())
    goto exit_with_motor_off;

#if 0
  // Directory sector debugger (instead of entry renderer)
  // displays sequential 256 byte dir sectors
  // press q to exit, any other key advances one sector
  // display below is track/side/sector/block
  {
    unsigned char block;
    while (1) {
      block = 1;
      while (block < 3) {
        x = 0;
        do {
          c = PEEK(0xD087U);
          if (c >= 'A' && c <= 'Z')
            c &= 0x1f;
          if (c >= 'a' && c <= 'z')
            c &= 0x1f;
          if (x % 16 == 0) {
            POKE(SCREEN_ADDRESS + 21 * 2 + ((x >> 4) * 80), nybl_to_screen(x >> 4));
            POKE(SCREEN_ADDRESS + 22 * 2 + ((x >> 4) * 80), nybl_to_screen(x));
          }
          POKE(SCREEN_ADDRESS + ((24 + (x%16)) * 2) + ((x >> 4) * 80), c & 0x7f);
        } while (++x);
        POKE(SCREEN_ADDRESS + 21 * 2 + 17 * 80, nybl_to_screen(dir_track >> 4));
        POKE(SCREEN_ADDRESS + 22 * 2 + 17 * 80, nybl_to_screen(dir_track));
        POKE(SCREEN_ADDRESS + 24 * 2 + 17 * 80, current_side + 0x30);
        POKE(SCREEN_ADDRESS + 26 * 2 + 17 * 80, nybl_to_screen(current_sector >> 4));
        POKE(SCREEN_ADDRESS + 27 * 2 + 17 * 80, nybl_to_screen(current_sector));
        POKE(SCREEN_ADDRESS + 30 * 2 + 17 * 80, block);
        while ((x = PEEK(0xD610U)) == 0);
        POKE(0xD610U, 0);
        if (x == 'q')
          goto exit_with_motor_off;
        block++;
      }
      current_sector++;
      if (current_sector > 10) {
        if (current_side == 1) {
          current_side = 0;
          dir_track++;
        }
        else
          current_side = 1;
        current_sector = 1;
      }
      if (!read_sector_with_cancel())
        goto exit_with_motor_off;
    }
  }
#else
  // skip start of sector until we reach the disk title
  for (j = 0; j < skip_bytes; j++)
    c = PEEK(0xD087U);

  // Then draw title at the top of the screen
  POKE(SCREEN_ADDRESS + 21 * 2, '"');
  for (x = 0; x < 16; x++) {
    c = PEEK(0xD087U);
    if (c >= 'A' && c <= 'Z')
      c &= 0x1f;
    POKE(SCREEN_ADDRESS + (22 + x) * 2, c & 0x7f);
  }
  POKE(SCREEN_ADDRESS + 38 * 2, '"');
  // reverse for disk title
  for (i = 0; i < 18; i++)
    lpoke(COLOUR_RAM_ADDRESS + (21 * 2) + 1 + i * 2, 0x2e);

  // user impatient?
  if (PEEK(0xD610U))
    goto exit_with_motor_off;

  // move to first dir entry depending on disk_type
  if (disk_type == DISK_TYPE_D81) {
    // D81
    current_sector++;
    if (!read_sector_with_cancel())
      goto exit_with_motor_off;
    // Skip 1st half of sector
    x = 0;
    do
      c = PEEK(0xD087U);
    while (++x);
    entries = 8;
  }
  else { // DISK_TYPE_D64
    current_sector++;
    if (!read_sector_with_cancel())
      goto exit_with_motor_off;
    entries = 8;
  }
  cur_row = 1; // begin drawing on row 1 of screen
  draw_entries();
  if (next_sector >= 254)
    goto exit_with_motor_off;
  do {
    if (disk_type == DISK_TYPE_D81) {
      current_sector++;
      entries = 16;
      skip_bytes = 0;
    }
    else { // DISK_TYPE_D64
      // with D64 dir is normally 18/1, 18/4, 18/7, ...
      if (next_sector < 2 || next_sector > 19) // illegal next sector, abort
        goto exit_with_motor_off;
      else if (next_sector == 2 && dir_track != 8) {
        // go back to end of track 8
        dir_track = 8;
        current_side = 1;
        current_sector = 10;
        skip_bytes = 1;
      }
      else if (next_sector > 2) { // 2 is the next 256 bytes of this 512 byte sector!
        // we just did read 18/1 which is 8/1/10/a, after it sector 3 is 9/0/1/a
        // 9/0 has 10 512 bytes sectors, so we only need to calculate the sector
        dir_track = 9;
        current_side = 0;
        current_sector = (next_sector - 1) / 2;
        skip_bytes = (next_sector - 1) % 2;
      }
      else // we are on the right track...
        dir_track = 0;
    }
    if (dir_track)
      if (!read_sector_with_cancel())
        goto exit_with_motor_off;

    if (skip_bytes)
      for (j = 0; j < 256; j++)
        c = PEEK(0xD087U);

    // once more, then we have the 22 entries we can display
    draw_entries();
  } while (cur_row < 23 && next_sector < 254);
#endif
  // Turn floppy LED and motor back off
exit_with_motor_off:
  POKE(0xD080U, 0);
  return 1;
}

void draw_disk_image_list(void)
{
  unsigned addr = SCREEN_ADDRESS;
  unsigned char i, x;
  unsigned char name[64];
  // First, clear the screen
  POKE(SCREEN_ADDRESS + 0, ' ');
  POKE(SCREEN_ADDRESS + 1, 0);
  POKE(SCREEN_ADDRESS + 2, ' ');
  POKE(SCREEN_ADDRESS + 3, 0);
  lcopy(SCREEN_ADDRESS, SCREEN_ADDRESS + 4, 40 * 2 * 23 - 4);
  lpoke(COLOUR_RAM_ADDRESS + 0, 0);
  lpoke(COLOUR_RAM_ADDRESS + 1, 1);
  lpoke(COLOUR_RAM_ADDRESS + 2, 0);
  lpoke(COLOUR_RAM_ADDRESS + 3, 1);
  lcopy(COLOUR_RAM_ADDRESS, COLOUR_RAM_ADDRESS + 4, 40 * 2 * 23 - 4);

  // Draw instructions
  for (i = 0; i < 80; i++)
    if (messed_up && i > 62)
      POKE(SCREEN_ADDRESS + 23 * 80 + (i << 1), petscii_to_screen(diskchooser_instructions[i + 17]));
    else
      POKE(SCREEN_ADDRESS + 23 * 80 + (i << 1), petscii_to_screen(diskchooser_instructions[i]));

  lcopy((long)highlight_row, COLOUR_RAM_ADDRESS + (23 * 80) + 0, 4);
  lcopy(COLOUR_RAM_ADDRESS + (23 * 80), COLOUR_RAM_ADDRESS + (23 * 80) + 4, 156);

  for (i = 0; i < 23; i++) {
    if ((display_offset + i) < file_count) {
      // Real line
      lcopy(0x40000U + ((display_offset + i) << 6), (unsigned long)name, 64);

      for (x = 0; x < 20; x++) {
        if ((name[x] >= 'A' && name[x] <= 'Z') || (name[x] >= 'a' && name[x] <= 'z'))
          POKE(addr + (x << 1), name[x] & 0x1f);
        else if (name[x] == '_')
          POKE(addr + (x << 1), 0x46);
        else if (name[x] == '~')
          POKE(addr + (x << 1), 0x27); // use a single-quote to substitute for a tilde
        else
          POKE(addr + (x << 1), name[x]);
      }
    }
    else {
      // Blank dummy entry
      for (x = 0; x < 40; x++)
        POKE(addr + (x << 1), ' ');
    }
    if ((display_offset + i) == selection_number) {
      // Highlight the row
      lcopy((long)highlight_row, COLOUR_RAM_ADDRESS + (i * 80), 4);
    }
    else {
      // Normal row
      lcopy((long)normal_row, COLOUR_RAM_ADDRESS + (i * 80), 4);
    }
    lcopy(COLOUR_RAM_ADDRESS + (i * 80), COLOUR_RAM_ADDRESS + (i * 80) + 4, 36);
    addr += (40 * 2);
  }
}

void scan_directory(unsigned char drive_id)
{
  unsigned char x, dir;
  char* ptr;
  struct m65_dirent* dirent;

  file_count = 0;

  closeall();

  // Add the pseudo disks
  lcopy((unsigned long)"- NO DISK -         ", 0x40000L + (file_count * 64), 20);
  file_count++;
  if (drive_id == 0) {
    lcopy((unsigned long)INTERNAL_DRIVE_0, 0x40000L + (file_count * 64), 20);
    file_count++;
  }
  else if (drive_id == 1) {
    lcopy((unsigned long)INTERNAL_DRIVE_1, 0x40000L + (file_count * 64), 20);
    file_count++;
  }
  lcopy((unsigned long)"- NEW D81 DD IMAGE -", 0x40000L + (file_count * 64), 20);
  file_count++;

  // no way to mount D65 yet...
#if WITH_NEW_D65
  lcopy((unsigned long)"- NEW D65 HD IMAGE -", 0x40000L + (file_count * 64), 20);
  file_count++;
#endif

  min_dir_entry = file_count;

  dir = opendir();
  dirent = readdir(dir);
  while (dirent && ((unsigned short)dirent != 0xffffU)) {

    x = strlen(dirent->d_name);

    // check DIR attribute of dirent
    if (dirent->d_type & 0x10) {

      // File is a directory
      if (x < 60) {
        lfill(0x40000L + (file_count * 64), ' ', 64);
        lcopy((long)&dirent->d_name[0], 0x40000L + 1 + (file_count * 64), x);
        // Put / at the start of directory names to make them obviously different
        lpoke(0x40000L + (file_count * 64), '/');
        // Don't list "." directory pointer
        if (strcmp(".", dirent->d_name))
          file_count++;
      }
    }
    else if (x > 4) {
      ptr = &dirent->d_name[x - 4];
      if ((!strcmp(ptr, ".D81")) || (!strcmp(ptr, ".d81")) || (!strcmp(ptr, ".D64")) || (!strcmp(ptr, ".d64"))
          || (!strcmp(ptr, ".D65")) || (!strcmp(ptr, ".d65"))) {
        // File is a disk image
        lfill(0x40000L + (file_count * 64), ' ', 64);
        lcopy((long)&dirent->d_name[0], 0x40000L + (file_count * 64), x);
        file_count++;
      }
    }

    dirent = readdir(dir);
  }

  closedir(dir);
}

char* freeze_select_disk_image(unsigned char drive_id)
{
  unsigned char x;
  char err;
  int idle_time = 0;

  // if working with drive 1, we will be
  if (drive_id == 1) {

  }

  file_count = 0;
  selection_number = 0;
  display_offset = 0;

  // First, clear the screen
  POKE(SCREEN_ADDRESS + 0, ' ');
  POKE(SCREEN_ADDRESS + 1, 0);
  POKE(SCREEN_ADDRESS + 2, ' ');
  POKE(SCREEN_ADDRESS + 3, 0);
  lcopy(SCREEN_ADDRESS, SCREEN_ADDRESS + 4, 40 * 2 * 25 - 4);

  for (x = 0; reading_disk_list_message[x]; x++)
    POKE(SCREEN_ADDRESS + 12 * 40 * 2 + (9 * 2) + (x * 2), reading_disk_list_message[x] & 0x3f);

  scan_directory(drive_id);

  // If we didn't find any disk images, then just return
  if (!file_count)
    return NULL;

  // Okay, we have some disk images, now get the user to pick one!
  draw_disk_image_list();
  while (1) {
    x = PEEK(0xD610U);

    if (!x) {
      // We use a simple lookup table to do this
      x = joy_to_key_disk[PEEK(0xDC00) & PEEK(0xDC01) & 0x1f];
      // Then wait for joystick to release
      while ((PEEK(0xDC00) & PEEK(0xDC01) & 0x1f) != 0x1f)
        continue;
    }

    if (!x) {
      idle_time++;
      if (idle_time == 100 && selection_number >= min_dir_entry) {
        // After sitting idle for 1 second, try mounting disk image and displaying directory listing
        if (draw_directory_contents(drive_id))
          messed_up = 1; // function did mount an image, so we need to return empty if aborted
      }
      usleep(10000);
      continue;
    }
    else
      idle_time = 0;

    // Clear read key
    POKE(0xD610U, 0);

    switch (x) {
    case 0x5f: // <- key at top left of key board
      // Go back up one directory
      mega65_dos_chdir("..");
      file_count = 0;
      selection_number = 0;
      display_offset = 0;
      scan_directory(drive_id);
      draw_disk_image_list();

      break;
    case 0x03: // RUN-STOP = make no change, but only if we did not mess up the drive!
      if (!messed_up)
        return NULL;
      selection_number = 0; // select no disk entry
      // fall though!
    case 0x0d:
    case 0x21: // Return = select this disk.
      // Copy name out
      lcopy(0x40000L + (selection_number * 64), (unsigned long)disk_name_return, 32);
      // Then null terminate it
      for (x = 31; x; x--)
        if (disk_name_return[x] == ' ') {
          disk_name_return[x] = 0;
        }
        else {
          break;
        }

      // First, clear flags for the F011 image
      if (drive_id == 0) {
        // Clear flags for drive 0
        lpoke(0xffd368bL, lpeek(0xffd368bL) & 0xb8);
      }
      else if (drive_id == 1) {
        // Clear flags for drive 1
        lpoke(0xffd368bL, lpeek(0xffd368bL) & 0x47);
      }

      // Try to mount it, with border black while working
      POKE(0xD020U, 0);
      if (disk_name_return[0] == '/') {
        // Its a directory
        mega65_dos_chdir(&disk_name_return[1]);
        file_count = 0;
        selection_number = 0;
        display_offset = 0;
        scan_directory(drive_id);
        draw_disk_image_list();
      }
      else {
        if (disk_name_return[0] == '-') {
          // Special case options
          if (disk_name_return[3] == 'O') {
            // No disk. Set image enable flag, and disable present flag
            if (drive_id == 0)
              lpoke(0xffd368bL, (lpeek(0xffd368bL) & 0xb8) + 0x01);
            else if (drive_id == 1)
              lpoke(0xffd368bL, (lpeek(0xffd368bL) & 0x47) + 0x08);
          }
          else if (disk_name_return[2] == 'I') {
            // Use internal drive (drive 0 only)
            while (!(lpeek(0xffd36a1L) & 1)) {
              lpoke(0xffd36a1L, lpeek(0xffd36a1L) | 0x01);
            }
          }
          else if (disk_name_return[2] == '1') {
            // Use 1565 external drive (drive 1 only)
            while (!(lpeek(0xffd36a1L) & 4)) {
              lpoke(0xffd36a1L, lpeek(0xffd36a1L) | 0x04);
            }
          }
          else if (disk_name_return[3] == 'E') {
            // Create and mount new empty D81 file
            // (this is like exec()/fork(), so there is no return value

            // Save the current freeze slot number, so that the image can get mounted against us
            POKE(0x03C0, slot_number & 0xff);
            POKE(0x03C1, slot_number >> 8);

            // Tell MAKEDISK if we want a D81 or a D65 image
            if (disk_name_return[7] == '8')
              POKE(0x33c, 0); // 0=DD
            else
              POKE(0x33c, 1); // 1=HD
            mega65_dos_exechelper("MAKEDISK.M65");
          }
        }
        else {
          if (drive_id == 0) {
            // hyppo attach does this
            // lpoke(0xffd368bL, (lpeek(0xffd368bL) & 0xb8) + 0x07);
            err = mega65_dos_d81attach0(disk_name_return);
          }
          else if (drive_id == 1) {
            // hyppo attach does this
            // lpoke(0xffd368bL, (lpeek(0xffd368bL) & 0x47) + 0x38);
            err = mega65_dos_d81attach1(disk_name_return);
          }
          else
            err = -1;
          if (err) {
            // Mounting the image failed
            display_error(err);

            // Mark drive as having nothing in it
            if (drive_id == 0) {
              // Clear flags for drive 0
              lpoke(0xffd368bL, lpeek(0xffd368bL) & 0xb8);
              lpoke(0xffd36a1L, lpeek(0xffd36a1L) & 0xfe);
            }
            else if (drive_id == 1) {
              // Clear flags for drive 1
              lpoke(0xffd368bL, lpeek(0xffd368bL) & 0x47);
              lpoke(0xffd36a1L, lpeek(0xffd36a1L) & 0xfb);
            }
            break;
          }
        }
        POKE(0xD020U, 6);

        // only do for internal drive or real entry
        if (selection_number == 1 || selection_number >= min_dir_entry) {
          // Mount succeeded, now seek to track 0 to make sure DOS
          // knows where we are, and to make sure the drive head is
          // sitting properly.
          POKE(0xD080U, 0x60 | drive_id); // motor and LED on
          POKE(0xD081U, 0x20); // Wait for motor spin up

          while (!(PEEK(0xD082) & 0x01)) {
            POKE(0xD081, 0x10);
            usleep(7000);
          }
          // Now check the contents of $D084 to find out the most recently
          // requested track, and seek the head to that track.
          x = freeze_peek(0xFFD3084); // Get last requested track by frozen programme
          while (x) {
            POKE(0xD081, 0x18);
            while (PEEK(0xD082) & 0x80)
              if (hal_border_flicker > 1)
                POKE(0xD020, (PEEK(0xD020) + 1) & 0xf);
            x--;
          }
          POKE(0xD080U, 0); // motor and led off
        }

        // Mounted ok, so return this image
        return disk_name_return;
      }
      break;
    case 0x11:
    case 0x9d: // Cursor down or left
      POKE(0xD020U, 6);
      selection_number++;
      if (selection_number >= file_count)
        selection_number = 0;
      break;
    case 0x91:
    case 0x1d: // Cursor up or right
      POKE(0xD020U, 6);
      selection_number--;
      if (selection_number < 0)
        selection_number = file_count - 1;
      break;
    }

    // Adjust display position
    if (selection_number < display_offset)
      display_offset = selection_number;
    if (selection_number > (display_offset + 22))
      display_offset = selection_number - 22;
    if (display_offset > (file_count - 22))
      display_offset = file_count - 22;
    if (display_offset < 0)
      display_offset = 0;

    draw_disk_image_list();
  }

  return NULL;
}
