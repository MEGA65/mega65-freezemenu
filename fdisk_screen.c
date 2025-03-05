#include "fdisk_screen.h"
#include "fdisk_memory.h"
#include "ascii.h"

long screen_line_address = SCREEN_ADDRESS;
char screen_column = 0;

unsigned char screen_hex_buffer[6];

unsigned char screen_hex_digits[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 0x01, 0x02, 0x03, 0x04, 0x05,
  0x06 };
unsigned char to_screen_hex(unsigned char c)
{
  return screen_hex_digits[c & 0xf];
}

void screen_hex_byte(unsigned int addr, long value)
{
  POKE(addr + 0, to_screen_hex(value >> 4));
  POKE(addr + 1, to_screen_hex(value >> 0));
}

void screen_hex(unsigned int addr, long value)
{
  POKE(addr + 0, to_screen_hex(value >> 28));
  POKE(addr + 1, to_screen_hex(value >> 24));
  POKE(addr + 2, to_screen_hex(value >> 20));
  POKE(addr + 3, to_screen_hex(value >> 16));
  POKE(addr + 4, to_screen_hex(value >> 12));
  POKE(addr + 5, to_screen_hex(value >> 8));
  POKE(addr + 6, to_screen_hex(value >> 4));
  POKE(addr + 7, to_screen_hex(value >> 0));
}

void format_hex(const int addr, const long value, const char columns)
{
  char i, c;
  char dec[9];
  screen_hex((int)&dec[0], value);

  c = 8 - columns;
  while (c) {
    for (i = 0; i < 7; i++)
      dec[i] = dec[i + 1];
    dec[7] = ' ';
    c--;
  }
  for (i = 0; i < columns; i++)
    lpoke(addr + i, dec[i]);
}

unsigned char screen_decimal_digits[16][5] = { { 0, 0, 0, 0, 1 }, { 0, 0, 0, 0, 2 }, { 0, 0, 0, 0, 4 }, { 0, 0, 0, 0, 8 },
  { 0, 0, 0, 1, 6 }, { 0, 0, 0, 3, 2 }, { 0, 0, 0, 6, 4 }, { 0, 0, 1, 2, 8 }, { 0, 0, 2, 5, 6 }, { 0, 0, 5, 1, 2 },
  { 0, 1, 0, 2, 4 }, { 0, 2, 0, 4, 8 }, { 0, 4, 0, 9, 6 }, { 0, 8, 1, 9, 2 }, { 1, 6, 3, 8, 4 }, { 3, 2, 7, 6, 8 } };

unsigned char ii, j, carry, temp;
static unsigned int value;
void screen_decimal(unsigned int addr, unsigned int v)
{
  // XXX - We should do this off-screen and copy into place later, to avoid glitching
  // on display.

  value = v;

  // Start with all zeros
  for (ii = 0; ii < 5; ii++)
    screen_hex_buffer[ii] = 0;

  // Add power of two strings for all non-zero bits in value.
  // XXX - We should use BCD mode to do this more efficiently
  for (ii = 0; ii < 16; ii++) {
    if (value & 1) {
      carry = 0;
      for (j = 4; j < 128; j--) {
        temp = screen_hex_buffer[j] + screen_decimal_digits[ii][j] + carry;
        if (temp > 9) {
          temp -= 10;
          carry = 1;
        }
        else
          carry = 0;
        screen_hex_buffer[j] = temp;
      }
    }
    value = value >> 1;
  }

  // Now convert to ascii digits
  for (j = 0; j < 5; j++)
    screen_hex_buffer[j] = screen_hex_buffer[j] | '0';

  // and shift out leading zeros
  for (j = 0; j < 4; j++) {
    if (screen_hex_buffer[0] != '0')
      break;
    screen_hex_buffer[0] = screen_hex_buffer[1];
    screen_hex_buffer[1] = screen_hex_buffer[2];
    screen_hex_buffer[2] = screen_hex_buffer[3];
    screen_hex_buffer[3] = screen_hex_buffer[4];
    screen_hex_buffer[4] = ' ';
  }

  // Copy to screen
  for (j = 0; j < 5; j++)
    POKE(addr + j, screen_hex_buffer[j]);
}

void format_decimal(const int addr, const int value, const char columns)
{
  char i;
  char dec[6];
  screen_decimal((int)&dec[0], value);

  for (i = 0; i < columns; i++)
    lpoke(addr + i, dec[i]);
}
