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
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

extern unsigned short slot_number;

short file_count = 0;
short selection_number = 0;
short display_offset = 0;

char* reading_disk_list_message = "SCANNING DIRECTORY ...";

char* diskchooser_instructions = "  SELECT DISK IMAGE, THEN PRESS RETURN  "
                                 "  OR PRESS RUN/STOP TO LEAVE UNCHANGED  ";

// clang-format off
unsigned char normal_row[40] = {
  0, 1, 0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1, 0, 1 };

unsigned char highlight_row[40] = {
  0, 0x21, 0, 0x21, 0, 0x21, 0, 0x21, 0, 0x21, 0, 0x21, 0, 0x21, 0, 0x21,
  0, 0x21, 0, 0x21, 0, 0x21, 0, 0x21, 0, 0x21, 0, 0x21, 0, 0x21, 0, 0x21,
  0, 0x21, 0, 0x21, 0, 0x21, 0, 0x21 };

unsigned char dir_line_colour[40] = {
  0, 0xe, 0, 0xe, 0, 0xe, 0, 0xe, 0, 0xe, 0, 0xe, 0, 0xe, 0, 0xe,
  0, 0xe, 0, 0xe, 0, 0xe, 0, 0xe, 0, 0xe, 0, 0xe, 0, 0xe, 0, 0xe,
  0, 0xe, 0, 0xe, 0, 0xe, 0, 0xe };
// clang-format on

char disk_name_return[32];

unsigned char joy_to_key_disk[32] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0d,         // With fire pressed
  0, 0, 0, 0, 0, 0, 0, 0x9d, 0, 0, 0, 0x1d, 0, 0x11, 0x91, 0 // without fire
};

char draw_directory_entry(unsigned char screen_row)
{
  char type;
  char firsta0 = 1;
  char invalid = 0;
  unsigned char i, c;
  // Skip first 5 bytes
  for (i = 0; i < 2; i++)
    c = PEEK(0xD087U);
  type = PEEK(0xD087U);
  if (!(type & 0xf))
    invalid = 1;
  for (i = 0; i < 2; i++)
    c = PEEK(0xD087U);
  // Then draw the 16 chars with quotes
  POKE(SCREEN_ADDRESS + (screen_row * 80) + (21 * 2), '"');
  for (i = 0; i < 16; i++) {
    c = PEEK(0xD087U);
    if (!c)
      invalid = 1;
    if (firsta0 && (c == 0xa0)) {
      POKE(SCREEN_ADDRESS + (screen_row * 80) + (22 * 2) + (i * 2), 0x22);
      firsta0 = 0;
    }
    else {
      if (c >= 'A' && c <= 'Z')
        c &= 0x1f;
      if (c >= 'a' && c <= 'z')
        c &= 0x1f;
      POKE(SCREEN_ADDRESS + (screen_row * 80) + (22 * 2) + (i * 2), c & 0x7f);
    }
  }
  if (firsta0) {
    POKE(SCREEN_ADDRESS + (screen_row * 80) + (38 * 2), '"');
  }
  if (type & 0x40)
    POKE(SCREEN_ADDRESS + (screen_row * 80) + (39 * 2), '<');
  if (!type & 0xf0)
    POKE(SCREEN_ADDRESS + (screen_row * 80) + (39 * 2), '*');

  // Read the rest of the entry to advance buffer pointer nicely
  for (i = 0; i < 11; i++)
    c = PEEK(0xD087U);

  if (invalid) {
    // Erase whatever we drew
    for (i = 21; i < 40; i++)
      POKE(SCREEN_ADDRESS + (screen_row * 80) + (i * 2), ' ');
  }
  else {
    lcopy((unsigned long)dir_line_colour, COLOUR_RAM_ADDRESS + (screen_row * 80) + (21 * 2), 19 * 2);
  }

  return invalid;
}

static unsigned char current_sector, dir_track, entries, cur_row;

void draw_entries(void)
{
  unsigned char i;
  for (i = 0; i < entries; i++) {
    if (!draw_directory_entry(cur_row))
      cur_row++;
    if (cur_row >= 23)
      break;
  }
}

int read_sector_with_cancel(void)
{
  POKE(0xD084U, dir_track);
  POKE(0xD085U, current_sector);
  POKE(0xD086U, 0);
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

void draw_directory_contents(void)
{
  unsigned char c, i, x;

  dir_track = 39;

  lcopy(0x40000L + (selection_number * 64), (unsigned long)disk_name_return, 32);

  // Don't draw directories (although it would be nice to show their contents)
  if (disk_name_return[0] == '/')
    return;

  // Then null terminate it
  for (x = 31; x; x--)
    if (disk_name_return[x] == ' ') {
      disk_name_return[x] = 0;
    }
    else {
      break;
    }

  // For D64 files, directory sector is at T18/S0
  i = strlen(disk_name_return);
  if (disk_name_return[i - 2] == '6' && disk_name_return[i - 1] == '4')
    dir_track = 17;

  // Try to mount it, with border black while working
  POKE(0xD020U, 0);
  if (mega65_dos_attachd81(disk_name_return)) {
    // Mounting the image failed
    POKE(0xD020U, 2);

    // XXX - Get DOS error code, and replace directory listing area with
    // appropriate error message
    return;
  }
  POKE(0xD020U, 6);

  // Exit if a key has been pressed
  if (PEEK(0xD610U))
    return;

  // Mounted disk, so now get the directory.

  // Read T40 S1 (sectors begin at 1, not 0)
  POKE(0xD080U, 0x60); // motor and LED on
  POKE(0xD081U, 0x20); // Wait for motor spin up
  current_sector = 1;

  if (!read_sector_with_cancel())
    return;

  // Disk name is in bytes $04-$14, so skip first four bytes of sector buffer
  // (we have to assign the PEEK here, so it doesn't get optimised away)
  i = 4;
  if (dir_track == 17) // d64 image? Then find disk name at 0x90
    i = 0x90;
  for (x = 0; x < i; x++)
    c = PEEK(0xD087U);
  // Then draw title at the top of the screen
  for (x = 0; x < 16; x++) {
    c = PEEK(0xD087U);
    if (c >= 'A' && c <= 'Z')
      c &= 0x1f;
    if (c >= 'a' && c <= 'z')
      c &= 0x1f;
    POKE(SCREEN_ADDRESS + (21 * 2) + (x * 2), c & 0x7f);
  }
  // reverse for disk title
  for (i = 0; i < 16; i++)
    lpoke(COLOUR_RAM_ADDRESS + (21 * 2) + 1 + i * 2, 0x2e);

  if (PEEK(0xD610U)) {
    POKE(0xD080U, 0);
    return;
  }

  if (dir_track != 17) // is not a d64? (i.e., d81 or d65?)
  {
    current_sector = 2;
    if (!read_sector_with_cancel())
      return;
    // Skip 1st half of sector
    x = 0;
    do
      c = PEEK(0xD087U);
    while (++x);
    entries = 8;
  }
  else // for d64 files, skip 0x60 bytes more
  {
    for (x = 0; x < 0x60; x++)
      c = PEEK(0xD087U);
    entries = 16;
  }
  cur_row = 1; // begin drawing on row 1 of screen
  draw_entries();

  current_sector++;
  if (!read_sector_with_cancel())
    return;

  entries = 16;
  draw_entries();

  // Turn floppy LED and motor back off
  POKE(0xD080U, 0);
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
  for (i = 0; i < 80; i++) {
    if (diskchooser_instructions[i] >= 'A' && diskchooser_instructions[i] <= 'Z')
      POKE(SCREEN_ADDRESS + 23 * 80 + (i << 1) + 0, diskchooser_instructions[i] & 0x1f);
    else
      POKE(SCREEN_ADDRESS + 23 * 80 + (i << 1) + 0, diskchooser_instructions[i]);
    POKE(SCREEN_ADDRESS + 23 * 80 + (i << 1) + 1, 0);
  }
  lcopy((long)highlight_row, COLOUR_RAM_ADDRESS + (23 * 80) + 0, 40);
  lcopy((long)highlight_row, COLOUR_RAM_ADDRESS + (23 * 80) + 40, 40);
  lcopy((long)highlight_row, COLOUR_RAM_ADDRESS + (24 * 80) + 0, 40);
  lcopy((long)highlight_row, COLOUR_RAM_ADDRESS + (24 * 80) + 40, 40);

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
      lcopy((long)highlight_row, COLOUR_RAM_ADDRESS + (i * 80), 40);
    }
    else {
      // Normal row
      lcopy((long)normal_row, COLOUR_RAM_ADDRESS + (i * 80), 40);
    }
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
    lcopy((unsigned long)"- INTERNAL 3.5\" -   ", 0x40000L + (file_count * 64), 20);
    file_count++;
  }
  if (drive_id == 1) {
    lcopy((unsigned long)"- 1565 DRIVE 1 -    ", 0x40000L + (file_count * 64), 20);
    file_count++;
  }
  lcopy((unsigned long)"- NEW D81 DD IMAGE -", 0x40000L + (file_count * 64), 20);
  file_count++;
  lcopy((unsigned long)"- NEW D65 HD IMAGE -", 0x40000L + (file_count * 64), 20);
  file_count++;

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
  int idle_time = 0;

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
      if (idle_time == 100) {
        // After sitting idle for 1 second, try mounting disk image and displaying directory listing
        draw_directory_contents();
      }
      usleep(10000);
      continue;
    }
    else
      idle_time = 0;

    // Clear read key
    POKE(0xD610U, 0);

    switch (x) {
    case 0x03: // RUN-STOP = make no change
      return NULL;
    case 0x5f: // <- key at top left of key board
      // Go back up one directory
      mega65_dos_chdir("..");
      file_count = 0;
      selection_number = 0;
      display_offset = 0;
      scan_directory(drive_id);
      draw_disk_image_list();

      break;
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
            if (drive_id == 1)
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
          if (drive_id == 0)
            lpoke(0xffd368bL, (lpeek(0xffd368bL) & 0xb8) + 0x07);
          if (drive_id == 1)
            lpoke(0xffd368bL, (lpeek(0xffd368bL) & 0x47) + 0x38);
          if (mega65_dos_attachd81(disk_name_return)) {
            // Mounting the image failed
            POKE(0xD020U, 2);

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

            // XXX - Get DOS error code, and replace directory listing area with
            // appropriate error message

            break;
          }
        }
        POKE(0xD020U, 6);

        // Mount succeeded, now seek to track 0 to make sure DOS
        // knows where we are, and to make sure the drive head is
        // sitting properly.
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
