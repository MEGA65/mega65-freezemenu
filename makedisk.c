/*
  Based on mega65-fdisk program as a starting point.

*/

#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "freezer_common.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"

void setup_menu_screen(void)
{
  POKE(0xD018U, 0x15); // upper case

  // NTSC 60Hz mode for monitor compatibility?
  //  POKE(0xD06FU, 0x80);

  // Reset border widths
  POKE(0xD05CU, 80);
  POKE(0xD05DU, 0xC0);

  // No sprites
  POKE(0xD015U, 0x00);

  // Move screen to SCREEN_ADDRESS
  POKE(0xD018U, (((CHARSET_ADDRESS - 0x8000U) >> 11) << 1) + (((SCREEN_ADDRESS - 0x8000U) >> 10) << 4));
  POKE(0xDD00U, (PEEK(0xDD00U) & 0xfc) | 0x01);

  // 16-bit text mode with full colour for chars >$FF
  // (which we will use for showing the thumbnail)
  POKE(0xD054U, (PEEK(0xD054) & 0xa8) | 0x05);
  POKE(0xD058U, 80);
  POKE(0xD059U, 0); // 80 bytes per row

  // Fill colour RAM with a value that won't cause problems in Super-Extended Attribute Mode
  //  lfill(0xff80000U, 1, 2000);
}

static unsigned short i;

void draw_box(
    unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2, unsigned char colour, unsigned char erase)
{
  unsigned char x, y;

  // Clear colour RAM
  for (x = x1; x <= x2; x++) {
    for (y = y1; y <= y2; y++) {
      lpoke(COLOUR_RAM_ADDRESS + y * 80 + x * 2 + 1, colour);
      lpoke(COLOUR_RAM_ADDRESS + y * 80 + x * 2 + 0, 0);
    }
  }

  if (erase) {
    for (x = x1 + 1; x < x2; x++) {
      for (y = y1 + 1; y < y2; y++) {
        lpoke(SCREEN_ADDRESS + y * 80 + x * 2 + 0, 0x20);
        lpoke(SCREEN_ADDRESS + y * 80 + x * 2 + 1, 0);
      }
    }
  }

  for (x = x1; x < x2; x++) {
    lpoke(SCREEN_ADDRESS + y1 * 80 + x * 2, 0x40); // horizontal line, centred
    lpoke(SCREEN_ADDRESS + y1 * 80 + x * 2 + 1, 0);
    lpoke(SCREEN_ADDRESS + y2 * 80 + x * 2, 0x40); // horizontal line, centred
    lpoke(SCREEN_ADDRESS + y2 * 80 + x * 2 + 1, 0);
  }

  for (y = y1; y < y2; y++) {
    lpoke(SCREEN_ADDRESS + y * 80 + x1 * 2, 0x42); // vertical line, centred
    lpoke(SCREEN_ADDRESS + y * 80 + x1 * 2 + 1, 0);
    lpoke(SCREEN_ADDRESS + y * 80 + x2 * 2, 0x42); // vertical line, centred
    lpoke(SCREEN_ADDRESS + y * 80 + x2 * 2 + 1, 0);
  }
  lpoke(SCREEN_ADDRESS + y1 * 80 + x1 * 2, 0x55); // top left corner
  lpoke(SCREEN_ADDRESS + y1 * 80 + x2 * 2, 73);   // top right corner
  lpoke(SCREEN_ADDRESS + y2 * 80 + x1 * 2, 74);   // bottom left corner
  lpoke(SCREEN_ADDRESS + y2 * 80 + x2 * 2, 75);   // bottom right corner
  lpoke(SCREEN_ADDRESS + y1 * 80 + x1 * 2 + 1, 0);
  lpoke(SCREEN_ADDRESS + y1 * 80 + x2 * 2 + 1, 0);
  lpoke(SCREEN_ADDRESS + y2 * 80 + x1 * 2 + 1, 0);
  lpoke(SCREEN_ADDRESS + y2 * 80 + x2 * 2 + 1, 0);
}

void write_text(unsigned char x1, unsigned char y1, unsigned char colour, char* t)
{
  unsigned char ofs = 0, x, c;
  for (x = x1; t[x - x1]; x++) {
    c = t[x - x1];
    if (c > 0x60)
      c -= 0x60;
    if (c > 0x40)
      c -= 0x40;
    lpoke(SCREEN_ADDRESS + y1 * 80 + x * 2 + 0, c);
    lpoke(SCREEN_ADDRESS + y1 * 80 + x * 2 + 1, 0);
    lpoke(COLOUR_RAM_ADDRESS + y1 * 80 + x * 2 + 0, 0x00);
    lpoke(COLOUR_RAM_ADDRESS + y1 * 80 + x * 2 + 1, colour);
  }
}

void input_text(unsigned char x1, unsigned char y1, unsigned char len, unsigned char colour, char* out)
{
  unsigned char ofs = 0, x, c;
  for (x = x1; x < (x1 + len); x++) {
    lpoke(SCREEN_ADDRESS + y1 * 80 + x1 * 2 + 0, ' ');
    lpoke(SCREEN_ADDRESS + y1 * 80 + x1 * 2 + 1, 0);
    lpoke(COLOUR_RAM_ADDRESS + y1 * 80 + x1 * 2 + 0, 0x00);
    lpoke(COLOUR_RAM_ADDRESS + y1 * 80 + x1 * 2 + 1, colour);
  }

  out[0] = 0;

  while (1) {
    // Enable cursor on current char
    lpoke(COLOUR_RAM_ADDRESS + y1 * 80 + (x1 + ofs) * 2 + 1, colour | 0x30);

    c = PEEK(0xD610);
    if (c >= 0x41 && c <= 0x5a || c >= 0x61 && c <= 0x7a || c >= 0x30 && c <= 0x39) {
      if (ofs < len) {
        out[ofs] = c;
        // ASCII to screen code conversion
        if (c > 0x60) {
          c -= 0x60;
        }
        lpoke(SCREEN_ADDRESS + y1 * 80 + (x1 + ofs) * 2 + 0, c);
        lpoke(COLOUR_RAM_ADDRESS + y1 * 80 + (x1 + ofs) * 2 + 1, colour);
        ofs++;
      }
    }
    else {
      switch (c) {
      case 0x14: // delete
        // XXX actually copy chars down, instead of just erasing from
        // end of line, and allow cursor left and right
        lpoke(SCREEN_ADDRESS + y1 * 80 + (x1 + ofs) * 2 + 0, ' ');
        lpoke(COLOUR_RAM_ADDRESS + y1 * 80 + (x1 + ofs) * 2 + 1, colour);
        if (ofs)
          ofs--;
        lpoke(SCREEN_ADDRESS + y1 * 80 + (x1 + ofs) * 2 + 0, ' ');
        lpoke(COLOUR_RAM_ADDRESS + y1 * 80 + (x1 + ofs) * 2 + 1, colour);
        break;
      case 0x03:
        out[0] = 0;
        POKE(0xD610, 0);
        return;
      case 0x0d:
        out[ofs] = 0;
        POKE(0xD610, 0);
        return;
      }
    }
    if (c)
      POKE(0xD610, 0);
  }
}

char hexchar(unsigned char v)
{
  v = v & 0xf;
  if (v < 10)
    return '0' + v;
  return 0x41 + v - 10;
}

void hexout(char* m, unsigned long v, int n)
{
  if (!n)
    return;
  do {
    m[n - 1] = hexchar(v);
    v = v >> 4L;

  } while (--n);
}

char msg[80];

// clang-format off
unsigned char bam_sector1[0x100] = {
  0x28, 0x02, 0x44, 0xbb, 0x39, 0x38, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x28, 0xff, 0xff, 0xff, 0xff, 0xff, 0x24, 0xf0, 0xff, 0xff, 0xff, 0xff
};
// clang-format on

unsigned char to_hex(unsigned char i)
{
  if (i < 10)
    return '0' + i;
  return 0x41 + i - 10;
}

void format_disk_image(unsigned long file_sector, char* diskname, unsigned char isD65)
{
  unsigned char i;
  unsigned short s;
  unsigned short sect_count = 80 * 20;
  if (isD65)
    sect_count = 85 * 64;

  // Make sure entire image is empty
  clear_sector_buffer();
  for (s = 0; s < sect_count; s++) {
    // XXX - Using multi-sector writes here would be much faster
    sdcard_writesector(file_sector + s, 0);
  }

  // Link to first directory sector
  sector_buffer[0] = 0x28;
  sector_buffer[1] = 0x03;
  // Diskname
  lcopy((long)diskname, (long)&sector_buffer[4], 16);
  if (strlen(diskname) < 16) {
    for (i = strlen(diskname); i < 16; i++)
      sector_buffer[4 + i] = 0xa0;
  }
  // Random disk ID
  i = PEEK(0xD012);
  sector_buffer[0x16] = to_hex(i & 0xf);
  sector_buffer[0x17] = to_hex(i >> 4);
  // DOS type
  sector_buffer[0x19] = 0x31;
  sector_buffer[0x1A] = 0x44;

  lcopy((long)bam_sector1, (long)&sector_buffer[0x100], 0x100);

  // Disk ID in BAM
  sector_buffer[0x104] = to_hex(i & 0xf);
  sector_buffer[0x105] = to_hex(i >> 4);

  if (!isD65)
    sdcard_writesector(file_sector + (39 * 10 * 2 + 0), 0);
  else
    sdcard_writesector(file_sector + (39 * 64 * 2 + 0), 0);

  clear_sector_buffer();
  lcopy((long)bam_sector1, (long)sector_buffer, 0x100);
  // Disk ID in BAM
  sector_buffer[0x004] = to_hex(i & 0xf);
  sector_buffer[0x005] = to_hex(i >> 4);
  sector_buffer[0x101] = 0xff;
  // Link to first sector of dir
  sector_buffer[0x000] = 0x00;
  sector_buffer[0x001] = 0xFF;
  // Mark all sectors free in 2nd half of disk
  sector_buffer[0x0FA] = 40;
  sector_buffer[0x0FB] = 0xff;

  if (!isD65)
    sdcard_writesector(file_sector + (39 * 10 * 2 + 1), 0);
  else
    sdcard_writesector(file_sector + (39 * 64 * 2 + 1), 0);
}

void do_make_disk_image(unsigned char isD65)
{
  char diskname[16 + 1];
  char filename[16 + 1];
  unsigned char filename_len;
  unsigned short slot_number = 0;
  unsigned long file_sector;

  fat32_open_file_system();
  if (!fat1_sector) {
    draw_box(10, 8, 30, 13, 2, 1);
    write_text(11, 9, 7, "COULD NOT FIND SD CARD");
    while (!PEEK(0xD610))
      continue;
    POKE(0xD610, 0);
    return;
  }

  draw_box(10, 8, 30, 14, 14, 1);
  write_text(11, 9, 14, "ENTER NAME FOR");
  if (isD65)
    write_text(11, 10, 14, "HD (D65) IMAGE:");
  else
    write_text(11, 10, 14, "DD (D81) IMAGE:");
  input_text(11, 12, 8, 1, filename);
  for (filename_len = 0; filename[filename_len]; filename_len++) {
    // Convert to upper case and work out length of string
    if (filename[filename_len] >= 0x61 && filename[filename_len] <= 0x7a)
      filename[filename_len] -= 0x20;
  }
  if (!filename_len)
    return;

  // Copy filename into diskname before it gets extended by the filename extension
  strcpy(diskname, filename);

  filename[filename_len++] = '.';
  filename[filename_len++] = 0x44;
  if (isD65) {
    filename[filename_len++] = 0x36;
    filename[filename_len++] = 0x35;
  }
  else {
    filename[filename_len++] = 0x38;
    filename[filename_len++] = 0x31;
  }
  filename[filename_len] = 0;
  lcopy((long)filename, 0x0400, 16);

  draw_box(10, 8, 30, 14, 7, 1);
  write_text(11, 9, 7, "CREATING IMAGE...");

  // Actually create the file
  //  while(!PEEK(0xD610)) POKE(0xD020,PEEK(0xD020)+1); POKE(0xD610,0);
  file_sector = fat32_create_contiguous_file(
      filename, isD65 ? (85 * 64 * 2 * 512L) : (80 * 10 * 2 * 512L), root_dir_sector, fat1_sector, fat2_sector);
  if (!file_sector) {
    // Error making file
    draw_box(10, 8, 30, 14, 2, 1);
    write_text(11, 9, 2, "Error creating file");
    write_text(11, 12, 1, "Press almost any key...");
    while (!PEEK(0xD610))
      continue;
    POKE(0xD610, 0);
  }
  else {
    // File creation succeeded

    // Write header, BAM and zero out directory track
    write_text(11, 10, 14, "FORMATTING IMAGE...");
    format_disk_image(file_sector, diskname, isD65);

    draw_box(8, 8, 32, 14, 13, 1);
    write_text(9, 9, 13, "Created disk image");
    write_text(9, 12, 1, "Press almost any key...");

    // Mark it as mounted in freeze slot stored in $03C0/1
    slot_number = PEEK(0x3C0) + (PEEK(0x3C1) << 8L);
    request_freeze_region_list();
    find_freeze_slot_start_sector(slot_number);
    freeze_slot_start_sector = *(uint32_t*)0xD681U;

    // Replace disk image name in process descriptor block
    for (i = 0; (i < 32) && filename[i]; i++)
      freeze_poke(0xFFFBD00L + 0x15 + i, filename[i]);
    // Update length of name
    freeze_poke(0xFFFBD00L + 0x13, i);
    // Pad with spaces as required by hypervisor
    for (; i < 32; i++)
      freeze_poke(0xFFFBD00L + 0x15 + i, ' ');

    while (!PEEK(0xD610))
      continue;
    POKE(0xD610, 0);
  }
}

#ifdef __CC65__
void main(void)
#else
int main(int argc, char** argv)
#endif
{
#ifdef __CC65__
  mega65_fast();
#endif

  // Disable interrupts and interrupt sources
  __asm__("sei");
  POKE(0xDC0DU, 0x7F);
  POKE(0xDD0DU, 0x7F);
  POKE(0xD01AU, 0x00);
  // XXX add missing C65 AND M65 peripherals
  // C65 UART, ethernet etc

  // Bank out BASIC ROM, leave KERNAL and IO in
  POKE(0x00, 0x3F);
  POKE(0x01, 0x36);

  // No decimal mode!
  __asm__("cld");

  // Enable extended attributes so we can use reverse
  POKE(0xD031U, PEEK(0xD031U) | 0x20);

  // Correct horizontal scaling
  POKE(0xD05AU, 0x78);

  // Silence SIDs
  POKE(0xD418U, 0);
  POKE(0xD438U, 0);

  set_palette();

  // Now find the start sector of the slot, and make a copy for safe keeping
  slot_number = 0;
  find_freeze_slot_start_sector(slot_number);
  freeze_slot_start_sector = *(uint32_t*)0xD681U;

  // SD or SDHC card?
  if (PEEK(0xD680U) & 0x10)
    sdhc_card = 1;
  else
    sdhc_card = 0;

  setup_menu_screen();

  request_freeze_region_list();

  if (PEEK(0x033C))
    do_make_disk_image(1); // 0=DD, 1=HD
  else
    do_make_disk_image(0); // 0=DD, 1=HD
  mega65_dos_exechelper("FREEZER.M65");

  return;
}
