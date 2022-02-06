/*
 * common freezer like tools stuff
 */
#include <sys/types.h>
#include <ctype.h>
#include <stdint.h>

#include "fdisk_memory.h"

char *deadly_haiku[3] = { "Error consumes all", "As sand erodes rock and stone", "Now also your mind" };

// for touch UI
signed char swipe_dir = 0;

// sector stuff
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
  colour_table[0x01] = 0x06;  // dim blue -> blue
  // colour_table[0x02] = 0x06;  // medium dim blue -> blue
  // colour_table[0x03] = 0x06;  // bright blue -> blue
  colour_table[0x04] = 0x00;  // dim green + no blue
  colour_table[0x05] = 0x25;  
  colour_table[0x06] = 0x26;  
  colour_table[0x07] = 0x27;  
  colour_table[0x08] = 0x28;  
  colour_table[0x09] = 0x29;  
  colour_table[0x0A] = 0x2a;  
  colour_table[0x0B] = 0x2b;  
  colour_table[0x0C] = 0x2c;  
  colour_table[0x0D] = 0x2d;  
  colour_table[0x0E] = 0x2e;  
  colour_table[0x0F] = 0x2f;
#endif
}

unsigned char ascii_to_screencode(char c)
{
  if (c >= 0x60)
    return c - 0x60;
  return c;
}
