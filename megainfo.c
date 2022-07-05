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

  POKE(0xD054U, PEEK(0xD054U) & 0xf8); // turn off CHR16, FCLRLO/HI
  POKE(0xD058U, 80);
  POKE(0xD059U, 0); // 80 bytes per row

  // 80-columns mode
  POKE(0xD031U, 0xE0);

  // Fill colour RAM with a value that won't cause problems in Super-Extended Attribute Mode
  lfill(0xff80000U, 1, 2000);
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
