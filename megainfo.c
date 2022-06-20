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

// clang-format off
unsigned char c64_palette[64]={
  0x00, 0x00, 0x00, 0x00,
  0xff, 0xff, 0xff, 0x00,
  0xba, 0x13, 0x62, 0x00,
  0x66, 0xad, 0xff, 0x00,
  0xbb, 0xf3, 0x8b, 0x00,
  0x55, 0xec, 0x85, 0x00,
  0xd1, 0xe0, 0x79, 0x00,
  0xae, 0x5f, 0xc7, 0x00,
  0x9b, 0x47, 0x81, 0x00,
  0x87, 0x37, 0x00, 0x00,
  0xdd, 0x39, 0x78, 0x00,
  0xb5, 0xb5, 0xb5, 0x00,
  0xb8, 0xb8, 0xb8, 0x00,
  0x0b, 0x4f, 0xca, 0x00,
  0xaa, 0xd9, 0xfe, 0x00,
  0x8b, 0x8b, 0x8b, 0x00
};
// clang-format on

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
  POKE(0xD054U, 0x40); // turn off extra vic perks
  POKE(0xD058U, 80);
  POKE(0xD059U, 0); // 80 bytes per row

  // 80-columns mode
  POKE(0xD031U, 0xE0);

  // Fill colour RAM with a value that won't cause problems in Super-Extended Attribute Mode
  lfill(0xff80000U, 1, 2000);
}

unsigned char ascii_to_screencode(char c)
{
  if (c >= 0x60)
    return c - 0x60;
  return c;
}

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

  // SD or SDHC card?
  if (PEEK(0xD680U) & 0x10)
    sdhc_card = 1;
  else
    sdhc_card = 0;

  setup_menu_screen();

  do_megainfo();
  mega65_dos_exechelper("FREEZER.M65");

  return;
}
