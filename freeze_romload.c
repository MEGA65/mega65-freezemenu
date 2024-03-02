#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "freezer_common.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

short file_count = 0;
short selection_number = 0;
short display_offset = 0;

unsigned char buffer[512];

char* reading_disk_list_message = "SCANNING DIRECTORY ...";

char* diskchooser_instructions = "SELECT A ROM FILE, PRESS RETURN TO LOAD,"
                                 "  OR PRESS RUN/STOP TO LEAVE UNCHANGED  ";

char *rom_reset_screen = "YOU HAVE LOADED THE ROM FILE            "
                         "                                        "
                         "                                        "
                         "    ROM VERSION:                        "
                         "                                        "
                         "INTO FROZEN MEMORY, REPLACING THE WHOLE "
                         "ROM STORED THERE.                       "
                         "                                        "
                         "IT IS PROBABLY UNSAFE TO JUST RESUME THE"
                         "SYSTEM, A FULL RESET IS RECOMMENDED.    "
                         "                                        "
                         "        RESET SYSTEM NOW? (Y/N)         ";

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

char rom_name_return[32];

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

void clear_screen(unsigned char lines)
{
  POKE(SCREEN_ADDRESS + 0, ' ');
  POKE(SCREEN_ADDRESS + 1, 0);
  POKE(SCREEN_ADDRESS + 2, ' ');
  POKE(SCREEN_ADDRESS + 3, 0);
  lcopy(SCREEN_ADDRESS, SCREEN_ADDRESS + 4, 40 * 2 * lines - 4);
  lpoke(COLOUR_RAM_ADDRESS + 0, 0);
  lpoke(COLOUR_RAM_ADDRESS + 1, 1);
  lpoke(COLOUR_RAM_ADDRESS + 2, 0);
  lpoke(COLOUR_RAM_ADDRESS + 3, 1);
  lcopy(COLOUR_RAM_ADDRESS, COLOUR_RAM_ADDRESS + 4, 40 * 2 * lines - 4);
}

void copy_line_to_screen(long dest, char *src, unsigned int length)
{
  unsigned int i;

  for (i = 0; i < length && src[i] != 0; i++) {
    POKE(dest + (i << 1) + 0, petscii_to_screen(src[i]));
    POKE(dest + (i << 1) + 1, 0);
  }
}

void draw_file_list(void)
{
  unsigned addr = SCREEN_ADDRESS + 4;
  unsigned char i, x;
  unsigned char name[64];
  // First, clear the screen
  clear_screen(23);

  // Draw instructions
  copy_line_to_screen(SCREEN_ADDRESS + 23 * 80, diskchooser_instructions, 80);
  lcopy((long)highlight_row, COLOUR_RAM_ADDRESS + (23 * 80) + 0, 40);
  lcopy((long)highlight_row, COLOUR_RAM_ADDRESS + (23 * 80) + 40, 40);
  lcopy((long)highlight_row, COLOUR_RAM_ADDRESS + (24 * 80) + 0, 40);
  lcopy((long)highlight_row, COLOUR_RAM_ADDRESS + (24 * 80) + 40, 40);

  for (i = 0; i < 23; i++) {
    if ((display_offset + i) < file_count) {
      // Real line
      lcopy(0x40000U + ((display_offset + i) << 6), (unsigned long)name, 64);

      for (x = 0; x < 32; x++) {
        if ((name[x] >= 'A' && name[x] <= 'Z') || (name[x] >= 'a' && name[x] <= 'z'))
          POKE(addr + (x << 1), name[x] & 0x1f);
        else
          POKE(addr + (x << 1), name[x]);
      }
    }
/*    else {
      // Blank dummy entry
      for (x = 0; x < 40; x++)
        POKE(addr + (x << 1), ' ');
    }*/
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

void scan_directory(void)
{
  unsigned char x, dir;
  short last_dir = -1, dir_pos, i;
  struct m65_dirent* dirent;

  file_count = 0;

  closeall();

  dir = opendir();
  dirent = readdir(dir);
  while (dirent && ((unsigned short)dirent != 0xffffU)) {

    x = strlen(dirent->d_name);
    // only accept 32 characters max!
    if (x > 32)
      goto next_entry;

    // check DIR attribute of dirent
    if (dirent->d_type & 0x10) {
      // File is a directory
      // limit filename length and skip '.' directory
      if (strcmp(dirent->d_name, ".")) {
        // keep directories at the top, and
        // always put ROMS dir at the very top
        if (!strcmp(dirent->d_name, "ROMS"))
          dir_pos = 0;
        else
          dir_pos = last_dir + 1;
        if (file_count && dir_pos != file_count) {
          for (i = file_count - 1; i >= dir_pos; i--)
            lcopy(0x40000L + (i * 64), 0x40040L + (i * 64), 64); // can't reverse copy!
        }
        lfill(0x40000L + (dir_pos * 64), ' ', 64);
        lcopy((long)&dirent->d_name[0], 0x40000L + 1 + (dir_pos * 64), x);
        // Put / at the start of directory names to make them obviously different
        lpoke(0x40000L + (dir_pos * 64), '/');
        last_dir++;
        file_count++;
      }
    }
    else if (x > 4 &&
             ((!strncmp(&dirent->d_name[x - 4], ".ROM", 4)) ||    // ROM Files
              (!strncmp(&dirent->d_name[x - 4], ".BIN", 4)) ||    // ROM Files
              (!strncmp(&dirent->d_name[x - 4], ".CHR", 4)) ||    // 8x8 CHaRacter Set FONT files
              (!strncmp(&dirent->d_name[x - 4], ".TCR", 4)) ||    // 8x16 Tall ChaRacter Set FONT files
              (!strncmp(&dirent->d_name[x - 4], ".rom", 4)) ||    // ROM Files
              (!strncmp(&dirent->d_name[x - 4], ".bin", 4)) ||    // ROM Files
              (!strncmp(&dirent->d_name[x - 4], ".chr", 4)) ||    // 8x8 CHaRacter Set FONT files
              (!strncmp(&dirent->d_name[x - 4], ".tcr", 4)))) {   // 8x16 Tall ChaRacter Set FONT files
      // File is a ROM or a CHaRset
      lfill(0x40000L + (file_count * 64), ' ', 64);
      lcopy((long)&dirent->d_name[0], 0x40000L + (file_count * 64), x);
      file_count++;
    }

next_entry:
    dirent = readdir(dir);
  }

  closedir(dir);
}

/*
 * uchar freeze_load_romarea()
 *
 * lets the user select a ROM like file from the SDcard
 * and loads it into frozen memory
 * 
 * returns 1 with the whole ROM was replaced and the user
 * should be prompted for a direct reset
 */
unsigned char freeze_load_romarea(void)
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

  scan_directory();

  // If we didn't find any disk images, then just return
  if (!file_count)
    return 0;

  // Okay, we have some disk images, now get the user to pick one!
  draw_file_list();
  while (1) {
    x = PEEK(0xD610U);

    if (!x) {
      // We use a simple lookup table to do this
      x = joy_to_key_disk[PEEK(0xDC00) & PEEK(0xDC01) & 0x1f];
      // Then wait for joystick to release
      while ((PEEK(0xDC00) & PEEK(0xDC01) & 0x1f) != 0x1f)
        continue;
    }
    else {
      POKE(0xD610U, 0);
    }

    switch (x) {
    case 0x03: // RUN-STOP = make no change
    case 0x1b: // ESC
      return 0;
    case 0x5f: // <- key at top left of key board
      // Go back up one directory

      mega65_dos_chdir("..");
      file_count = 0;
      selection_number = 0;
      display_offset = 0;
      scan_directory();
      draw_file_list();

      break;
    case 0x0d:
    case 0x21: // Return = select this file.
      // Copy name out
      lcopy(0x40000L + (selection_number * 64), (unsigned long)rom_name_return, 32);
      // Then null terminate it
      for (x = 31; x; x--)
        if (rom_name_return[x] == ' ') {
          rom_name_return[x] = 0;
        }
        else {
          break;
        }

      // Is it a directory?
      if (rom_name_return[0] == '/') {
        // Its a directory
        mega65_dos_chdir(&rom_name_return[1]);
        file_count = 0;
        selection_number = 0;
        display_offset = 0;
        scan_directory();
        draw_file_list();
      }
      else {
        // XXX - Actually do loading of ROM / ROM diff file
        POKE(0xD020U, 0);
        if (!strcmp(&rom_name_return[strlen(rom_name_return) - 4], ".ROM") ||
            !strcmp(&rom_name_return[strlen(rom_name_return) - 4], ".BIN") ||
            !strcmp(&rom_name_return[strlen(rom_name_return) - 4], ".rom") ||
            !strcmp(&rom_name_return[strlen(rom_name_return) - 4], ".bin")) {
          int s;
          // Load normal ROM file
          // Begin by loading the file at $40000-$5FFFF
          read_file_from_sdcard(rom_name_return, 0x40000L);

          // Then progressively save it into the frozen memory
          request_freeze_region_list();
          find_freeze_slot_start_sector(0); // we only work on slot 0!
          freeze_slot_start_sector = *(uint32_t*)0xD681U;

          for (s = 0; s < 256; s++) { // ROM is 128k, devided by 512 byte sectors is 256 sectors to load
            // Write each sector to frozen memory
            POKE(0xD020U, (PEEK(0xD020U) + 1) & 0xf);
            lcopy(0x40000L + 512L * (long)s, (long)buffer, 512);
            freeze_store_sector(0x20000L + ((long)s) * 512L, buffer);
          }
          POKE(0xD020U, 6);

          return 1;
        }
        
        if (!strcmp(&rom_name_return[strlen(rom_name_return) - 4], ".CHR") ||
            !strcmp(&rom_name_return[strlen(rom_name_return) - 4], ".TCR") ||
            !strcmp(&rom_name_return[strlen(rom_name_return) - 4], ".chr") ||
            !strcmp(&rom_name_return[strlen(rom_name_return) - 4], ".tcr")) {
          unsigned char cg_7a_set = 0, cg_7a_mask = 0xff;
          unsigned char cg_54_set = 0, cg_54_mask = 0xff;
          unsigned short i;

          // Load CHARSET to chargen WOM
          read_file_from_sdcard(rom_name_return, 0x40000L);
          lcopy(0x40000L, CHARGEN_ADDRESS, 4096);

          request_freeze_region_list();
          find_freeze_slot_start_sector(0); // we only work on slot 0!
          freeze_slot_start_sector = *(uint32_t*)0xD681U;

          if (freeze_region_flags & FREEZE_REGION_HAS_CHARGEN)
            // only put that into the slot, if HYPPO supports it!
            for (i = 0; i < 8; i++) {
              lcopy(0x40000L + 512L*i, (long)sector_buffer, 512);
              freeze_store_sector(CHARGEN_ADDRESS + 512L*i, NULL);
            }

          // set or reset TALL character bit depending on charset extension
          if (!strcmp(&rom_name_return[strlen(rom_name_return) - 4], ".TCR")) {
            // switch palemu off and enable CHARY16
            cg_54_set |= 0x20;
            cg_54_mask ^= 0x20;
            cg_7a_set |= 0x10;
            cg_7a_mask ^= 0x10;
          } else {
            cg_7a_set |= 0x00;
            cg_7a_mask ^= 0x10;
          }
          freeze_poke(0xFFD307AL, cg_54_set | (freeze_peek(0xFFD307AL) & cg_54_mask));
          freeze_poke(0xFFD307AL, cg_7a_set | (freeze_peek(0xFFD307AL) & cg_7a_mask));
          lpoke(0xFFD307AL, cg_54_set | (lpeek(0xFFD307AL) & cg_54_mask));
          lpoke(0xFFD307AL, cg_7a_set | (lpeek(0xFFD307AL) & cg_7a_mask));

          return 0; // no reset needed, most probably...
        }
      }
      break;
    
    case 0x13: // HOME
      selection_number = 0;
      break;
    case 0x93: // Shift-HOME
      selection_number = file_count - 1;
      break;
    case 0x1d: // Cursor right, next page
      selection_number += 22;
    case 0x11: // Cursor down, one down
      selection_number++;
      if (selection_number >= file_count)
        selection_number = file_count - 1;
      break;
    case 0x9d: // Cursor left, prev page
      selection_number -= 22;
    case 0x91: // Cursor up, one up
      selection_number--;
      if (selection_number < 0)
        selection_number = 0;
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

    if (x)
      draw_file_list();
    x = 0;
  }

  return 0;
}

void user_reset_prompt(void)
{
  unsigned char x = 0;

  clear_screen(25);
  copy_line_to_screen(SCREEN_ADDRESS + 80, rom_reset_screen, 12*80);
  copy_line_to_screen(SCREEN_ADDRESS + 3*80 + 8, rom_name_return, 32);
  copy_line_to_screen(SCREEN_ADDRESS + 4*80 + 34, detect_rom(), 11);

  while (x != 'Y' && x!= 'y' && x != 'N' && x != 'n') {
    x = PEEK(0xD610U);

    if (!x) {
      // We use a simple lookup table to do this
      x = joy_to_key_disk[PEEK(0xDC00) & PEEK(0xDC01) & 0x1f];
      // Then wait for joystick to release
      while ((PEEK(0xDC00) & PEEK(0xDC01) & 0x1f) != 0x1f)
        continue;
      // translate joystick to keys
      if (x == 0xd) // fire is Y
        x = 'Y';
      else if (x == 0x9d) // left is abort
        x = 'N';
    }
    else {
      POKE(0xD610U, 0);
    }
  }

  if (x == 'y' || x == 'Y') {
    freeze_poke(0xFFD3640U + 8, freeze_peek(0x2FFFCL));
    freeze_poke(0xFFD3640U + 9, freeze_peek(0x2FFFDL));
    // Reset $01 port values
    freeze_poke(0xFFD3640U + 0x10, 0x3f);
    freeze_poke(0xFFD3640U + 0x11, 0x3f);
    // disable interrupts, clear decimal mode
    freeze_poke(0xFFD3640U + 0x07, 0xe7);
    // Clear memory mapping
    for (x = 0x0a; x <= 0x0f; x++)
      freeze_poke(0xFFD3640U + x, 0);
    // Turn off extended graphics mode, only keep palemu
    freeze_poke(0xFFD3054U, freeze_peek(0xFFD3054U) & 0x20);
    unfreeze_slot(0);

    while (1)
      POKE(0xD020U, (PEEK(0xD020U) + 1) & 0xf);
  }
}

// returns 1 if rom was changed and user skipped reset
unsigned char do_rom_loader(void)
{
  unsigned char changed = 0;

  // Get user to select a ROM file from the SD card
  // This loads it into RAM, and then writes it out into the freeze slot
  if (freeze_load_romarea()) {
    user_reset_prompt();
    changed = 1;
  }

  // need to go up to root dir, or we can't load FREEZER again!
  mega65_dos_cdroot();

  // Return, so that control can go back to the freezer
  return changed;
}
