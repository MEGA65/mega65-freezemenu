/*
  Based on mega65-fdisk program as a starting point.

*/

#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

signed char swipe_dir = 0;

// Used to quickly return from functions if a navigation key has been pressed
// (used to avoid delays when navigating through the list of freeze slots
#define NAVIGATION_KEY_CHECK()                                                                                              \
  {                                                                                                                         \
    if (((PEEK(0xD610U) & 0x7f) == 0x11) || ((PEEK(0xD610U) & 0x7f) == 0x1D))                                               \
      return;                                                                                                               \
  }

uint8_t sector_buffer[512];

unsigned short slot_number = 0;

void clear_sector_buffer(void)
{
#ifndef __CC65__DONTUSE
  int i;
  for (i = 0; i < 512; i++)
    sector_buffer[i] = 0;
#else
  lfill((uint32_t)sector_buffer, 0, 512);
#endif
}

unsigned char c64_palette[64] = { 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0xba, 0x13, 0x62, 0x00, 0x66, 0xad, 0xff,
  0x00, 0xbb, 0xf3, 0x8b, 0x00, 0x55, 0xec, 0x85, 0x00, 0xd1, 0xe0, 0x79, 0x00, 0xae, 0x5f, 0xc7, 0x00, 0x9b, 0x47, 0x81,
  0x00, 0x87, 0x37, 0x00, 0x00, 0xdd, 0x39, 0x78, 0x00, 0xb5, 0xb5, 0xb5, 0x00, 0xb8, 0xb8, 0xb8, 0x00, 0x0b, 0x4f, 0xca,
  0x00, 0xaa, 0xd9, 0xfe, 0x00, 0x8b, 0x8b, 0x8b, 0x00 };

unsigned char colour_table[256];

void set_palette(void)
{
  // First set the 16 C64 colours
  unsigned char c;
  POKE(0xD070U, 0xFF);
  for (c = 0; c < 16; c++) {
    POKE(0xD100U + c, c64_palette[c * 4 + 0]);
    POKE(0xD200U + c, c64_palette[c * 4 + 1]);
    POKE(0xD300U + c, c64_palette[c * 4 + 2]);
  }

  // Then prepare a colour cube in the rest of the palette
  for (c = 0x10; c; c++) {
    // 3 bits for red
    POKE(0xD100U + c, (c >> 4) & 0xe);
    // 3 bits for green
    POKE(0xD200U + c, (c >> 1) & 0xe);
    // 2 bits for blue
    POKE(0xD300U + c, (c << 2) & 0xf);
  }

  // Make colour lookup table
  c = 0;
  do {
    colour_table[c] = c;
  } while (++c);

  // Now map C64 colours directly
  colour_table[0x00] = 0x20; // black   ($00 = transparent colour, so we have to use very-dim red instead)
  colour_table[0xff] = 0x01; // white
  colour_table[0xe0] = 0x02; // red
  colour_table[0x1f] = 0x03; // cyan
  colour_table[0xe3] = 0x04; // purple
  colour_table[0x1c] = 0x05; // green
  colour_table[0x03] = 0x06; // blue
  colour_table[0xfc] = 0x07; // yellow
  colour_table[0xec] = 0x08; // orange
  colour_table[0xa8] = 0x09; // brown
  colour_table[0xad] = 0x0a; // pink
  colour_table[0x49] = 0x0b; // grey1
  colour_table[0x92] = 0x0c; // grey2
  colour_table[0x9e] = 0x0d; // lt.green
  colour_table[0x93] = 0x0e; // lt.blue
  colour_table[0xb6] = 0x0f; // grey3

// We should also map colour cube colours 0x00 -- 0x0f to
// somewhere sensible.
// 0x00 = black, so can stay
#if 0
  colour_table[0x01]=0x06;  // dim blue -> blue
  // colour_table[0x02]=0x06;  // medium dim blue -> blue
  // colour_table[0x03]=0x06;  // bright blue -> blue
  colour_table[0x04]=0x00;  // dim green + no blue
  colour_table[0x05]=0x25;  
  colour_table[0x06]=0x26;  
  colour_table[0x07]=0x27;  
  colour_table[0x08]=0x28;  
  colour_table[0x09]=0x29;  
  colour_table[0x0A]=0x2a;  
  colour_table[0x0B]=0x2b;  
  colour_table[0x0C]=0x2c;  
  colour_table[0x0D]=0x2d;  
  colour_table[0x0E]=0x2e;  
  colour_table[0x0F]=0x2f;
#endif
};

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
  POKE(0xD054U, (PEEK(0xD054)&0xa8)| 0x05);
  POKE(0xD058U, 80);
  POKE(0xD059U, 0); // 80 bytes per row

  // Fill colour RAM with a value that won't cause problems in Super-Extended Attribute Mode
  lfill(0xff80000U, 1, 2000);
}

unsigned char ascii_to_screencode(char c)
{
  if (c >= 0x60)
    return c - 0x60;
  return c;
}

static unsigned short i;

unsigned char detect_cpu_speed(void)
{
  if (freeze_peek(0xffd367dL) & 0x10)
    return 40;
  if (freeze_peek(0xffd3054L) & 0x40)
    return 40;
  if (freeze_peek(0xffd3031L) & 0x40)
    return 3;
  if (freeze_peek(0xffd0030L) & 0x01)
    return 2;
  return 1;
}

void next_cpu_speed(void)
{
  switch (detect_cpu_speed()) {
  case 1:
    // Make it 2MHz
    freeze_poke(0xffd0030L, 1);
    break;
  case 2:
    // Make it 3.5MHz
    freeze_poke(0xffd0030L, 0);
    freeze_poke(0xffd3031L, freeze_peek(0xffd3031L) | 0x40);
    break;
  case 3:
    // Make it 40MHz
    freeze_poke(0xffd367dL, freeze_peek(0xffd367dL) | 0x10);
    break;
  case 40:
  default:
    // Make it 1MHz
    freeze_poke(0xffd0030L, 0);
    freeze_poke(0xffd3031L, freeze_peek(0xffd3031L) & 0xbf);
    freeze_poke(0xffd3054L, freeze_peek(0xffd3054L) & 0xbf);
    freeze_poke(0xffd367dL, freeze_peek(0xffd367dL) & 0xef);
    break;
  }
}

char c65_rom_name[12];
char* detect_rom(void)
{
  // Check for C65 ROM via version string
  if ((freeze_peek(0x20016L) == 'V') && (freeze_peek(0x20017L) == '9')) {
    c65_rom_name[0] = ' ';
    c65_rom_name[1] = 'C';
    c65_rom_name[2] = '6';
    c65_rom_name[3] = '5';
    c65_rom_name[4] = ' ';
    for (i = 0; i < 6; i++)
      c65_rom_name[5 + i] = freeze_peek(0x20017L + i);
    c65_rom_name[11] = 0;
    return c65_rom_name;
  }

  if (freeze_peek(0x2e47dL) == 'J') {
    // Probably jiffy dos
    if (freeze_peek(0x2e535L) == 0x06)
      return "sx64 jiffy ";
    else
      return "c64 jiffy  ";
  }

  // Else guess using detection routines from detect_roms.c
  // These were built using a combination of the ROMs from zimmers.net/pub/c64/firmware,
  // the RetroReplay ROM collection, and the JiffyDOS ROMs
  if (freeze_peek(0x2e449L) == 0x2e)
    return "C64GS      ";
  if (freeze_peek(0x2e119L) == 0xc9)
    return "C64 REV1   ";
  if (freeze_peek(0x2e67dL) == 0xb0)
    return "C64 REV2 JP";
  if (freeze_peek(0x2ebaeL) == 0x5b)
    return "C64 REV3 DK";
  if (freeze_peek(0x2e0efL) == 0x28)
    return "C64 SCAND  ";
  if (freeze_peek(0x2ebf3L) == 0x40)
    return "C64 SWEDEN ";
  if (freeze_peek(0x2e461L) == 0x20)
    return "CYCLONE 1.0";
  if (freeze_peek(0x2e4a4L) == 0x41)
    return "DOLPHIN 1.0";
  if (freeze_peek(0x2e47fL) == 0x52)
    return "DOLPHIN 2AU";
  if (freeze_peek(0x2eed7L) == 0x2c)
    return "DOLPHIN 2P1";
  if (freeze_peek(0x2e7d2L) == 0x6b)
    return "DOLPHIN 2P2";
  if (freeze_peek(0x2e4a6L) == 0x32)
    return "DOLPHIN 2P3";
  if (freeze_peek(0x2e0f9L) == 0xaa)
    return "DOLPHIN 3.0";
  if (freeze_peek(0x2e462L) == 0x45)
    return "DOSROM V1.2";
  if (freeze_peek(0x2e472L) == 0x20)
    return "MERCRY3 PAL";
  if (freeze_peek(0x2e16dL) == 0x84)
    return "MERCRY NTSC";
  if (freeze_peek(0x2e42dL) == 0x4c)
    return "PET 4064   ";
  if (freeze_peek(0x2e1d9L) == 0xa6)
    return "SX64 CROACH";
  if (freeze_peek(0x2eba9L) == 0x2d)
    return "SX64 SCAND ";
  if (freeze_peek(0x2e476L) == 0x2a)
    return "TRBOACS 2.6";
  if (freeze_peek(0x2e535L) == 0x07)
    return "TRBOACS 3P1";
  if (freeze_peek(0x2e176L) == 0x8d)
    return "TRBOASC 3P2";
  if (freeze_peek(0x2e42aL) == 0x72)
    return "TRBOPROC US";
  if (freeze_peek(0x2e4acL) == 0x81)
    return "C64C 251913";
  if (freeze_peek(0x2e479L) == 0x2a)
    return "C64 REV2   ";
  if (freeze_peek(0x2e535L) == 0x06)
    return "SX64 REV4  ";
  return "UNKNOWN ROM";
}

void draw_thumbnail(void)
{
  // Take the 4K of thumbnail data and render it to the display
  // area at $50000.
  // This requires a bit of fiddling:
  // First, the thumbnail data has a nominal address of $0010000
  // in the frozen memory, which overlaps with the main RAM,
  // so we can't use our normal routine to find the start of freeze
  // memory. Instead, we will find that region directly, and then
  // process the 8 sectors of data in a linear fashion.
  // The thumbnail bytes themselves are arranged linearly, so we
  // have to work out the right place to store them in the thumbnail
  // data.  We would really like to avoid having to use lpoke for
  // this all the time, because lpoke() uses a DMA for every memory
  // access, which really slows things down. This would be bad, since
  // we want users to be able to very quickly and smoothly flip between
  // the freeze slots and see what is there.
  // So we will instead copy the sectors down to $8800, and then
  // render the thumbnail at $9000, and then copy it into place with
  // a single DMA.
  unsigned char x, y, i;
  unsigned short yoffset;
  uint32_t thumbnail_sector = find_thumbnail_offset();

  // Can't find thumbnail area?  Then show no thumbnail
  if (thumbnail_sector == 0xFFFFFFFFL) {
    lfill(0x50000L, 0, 10 * 6 * 64);
    return;
  }
  // Copy thumbnail memory to $08800
  for (i = 0; i < 8; i++) {
    sdcard_readsector(freeze_slot_start_sector + thumbnail_sector + i);
    lcopy((long)sector_buffer, 0x8800L + (i * 0x200), 0x200);
    NAVIGATION_KEY_CHECK();
  }

  // Rearrange pixels
  yoffset = 0;
  for (y = 0; y < 48; y++) {
    for (x = 0; x < 73; x++) {
      // Also the whole thing is rotated by one byte, so add that on as we plot the pixel
      POKE(0xA000U + (x & 7) + (x >> 3) * (64 * 6L) + ((y & 7) << 3) + (y >> 3) * 64,
          colour_table[PEEK(0x8800U + 1 + 7 + x + yoffset)]);
    }
    NAVIGATION_KEY_CHECK();
    yoffset += 80;
  }
  // Copy to final area
  lcopy(0xA000U, 0x50000U, 4096);
}

struct process_descriptor_t process_descriptor;

// Left/right do left/right
// fire = F3
// down = disk menu
// up = toggle PAL/NTSC ?
unsigned char joy_to_key[32] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF3,       // With fire pressed
  0, 0, 0, 0, 0, 0, 0, 0x1d, 0, 0, 0, 0x9d, 0, 'd', 'v', 0 // without fire
};

#ifdef WITH_TOUCH
unsigned char touch_keys[2][9] = { { 0xF3, 0x00, 'c', 'r', 'f', 0x00, 'm', 'a', 'd' },
  { 0xF7, 0x00, 'j', 't', 'v', 0x00, 'e', 'k', 'x' } };

unsigned short x;
unsigned short y;

unsigned char last_touch = 0;
unsigned char last_x;

void poll_touch_panel(void)
{
  if (PEEK(0xD6B0U) & 1) {
    x = PEEK(0xD6B9) + ((PEEK(0xD6BB) & 0x03) << 8);
    y = PEEK(0xD6BA) + ((PEEK(0xD6BB) & 0x30) << 4);
    x = x >> 4;
    y = y >> 4;
  }
  else {
    x = 0;
    y = 0;
  }
}
#endif

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

  freeze_monitor();
  mega65_dos_exechelper("FREEZER.M65");

  return;
}
