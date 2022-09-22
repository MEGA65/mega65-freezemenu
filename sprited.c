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
#include "ascii.h"

unsigned char colour_table[256];

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
  lfill(0xff80000U, 1, 2000);
}

unsigned short i;

struct process_descriptor_t process_descriptor;

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

  // done in freeze_sprited.c:Initialize
  // Now find the start sector of the slot, and make a copy for safe keeping
  //slot_number = 0;
  //find_freeze_slot_start_sector(slot_number);
  //freeze_slot_start_sector = *(uint32_t*)0xD681U;

  // SD or SDHC card?
  if (PEEK(0xD680U) & 0x10)
    sdhc_card = 1;
  else
    sdhc_card = 0;

  // done in freeze_sprited.c:Initialize
  //request_freeze_region_list();

  // Back to 40 column, 8-bit text mode
  POKE(0xD031U, 0x00);
  POKE(0xD054U, (PEEK(0xD054) & 0xa8) | 0x00);
  // Lower case
  POKE(0xD018U, 0x16);

  do_sprite_editor();

  // Back to 40 column mode
  POKE(0xD031U, 0x00);
  // 256-colour char data from chip RAM, not expansion RAM
  POKE(0xD063U, 0x00);

  mega65_dos_exechelper("FREEZER.M65");

  return;
}
