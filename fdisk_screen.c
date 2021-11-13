#include "fdisk_screen.h"
#include "fdisk_memory.h"
#include "ascii.h"

extern unsigned char* charset;

long screen_line_address = SCREEN_ADDRESS;
char screen_column = 0;

unsigned char* footer_messages[FOOTER_MAX + 1] = {
  "MEGA65 FREEZE MONITOR V00.01 :     X - RETURN TO FREEZE MENU, M - DISPLAY MEMORY",
  "MEGA65 SPRITE EDITOR V00.01 :        1 - CLEAR, 2- DRAW ETC, H - HELP, F3 - EXIT",
  "                                                                                ",
  "A FATAL ERROR HAS OCCURRED, SORRY.                                              "
};

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

void write_line_len(char* s, char col, char length)
{
  char len = 0;
  // Work out length, and convert from ASCII to PETSCII
  while (s[len]) {
    if (s[len] >= 'a' && s[len] <= 'z')
      s[len] -= 0x60;
    else if (s[len] >= 'A' && s[len] <= 'Z')
      s[len] -= 0x40;
    len++;
  }
  write_line_raw(s, col, length);
}

void write_line_raw(char* s, char col, char length)
{
  lcopy((long)&s[0], screen_line_address + col, length);
  screen_line_address += 80;
  if ((screen_line_address - SCREEN_ADDRESS) >= (24 * 80)) {
    screen_line_address -= 80;
    lcopy(SCREEN_ADDRESS + 80, SCREEN_ADDRESS, 23 * 80);
    lcopy(COLOUR_RAM_ADDRESS + 80, COLOUR_RAM_ADDRESS, 23 * 80);
    lfill(SCREEN_ADDRESS + 23 * 80, ' ', 80);
    lfill(COLOUR_RAM_ADDRESS + 23 * 80, 1, 80);
  }
}

char stemp[80];
void write_line(char* s, char col)
{
  char len = 0;
  // Copy string so that it doesn't get modified if the caller doesn't expect it
  while (s[len]) {
    stemp[len] = s[len];
    len++;
    if (len > 78)
      break;
  }
  stemp[len] = 0;
  write_line_len(stemp, col, len);
}

void recolour_last_line(char colour)
{
  long colour_address = COLOUR_RAM_ADDRESS + (screen_line_address - SCREEN_ADDRESS) - 80;
  lfill(colour_address, colour, 80);
  return;
}

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

long addr;
void display_footer(unsigned char index)
{
  char i;
  addr = (long)footer_messages[index];
  lcopy(addr, FOOTER_ADDRESS, 80);
  for (i = 0; i < 80; i++)
    if (PEEK(FOOTER_ADDRESS + i) >= 'a' && PEEK(FOOTER_ADDRESS + i) <= 'z')
      POKE(FOOTER_ADDRESS + i, PEEK(FOOTER_ADDRESS + i) - 0x60);
    else if (PEEK(FOOTER_ADDRESS + i) >= 'A' && PEEK(FOOTER_ADDRESS + i) <= 'Z')
      POKE(FOOTER_ADDRESS + i, PEEK(FOOTER_ADDRESS + i) - 0x40);
  set_screen_attributes(FOOTER_ADDRESS, 80, ATTRIB_REVERSE);
}

void setup_screen(void)
{
  unsigned char v;

  m65_io_enable();

  // Normal 8-bit text mode
  POKE(0xD054U, 0);

  // 80-column mode, fast CPU, extended attributes enable
  *((unsigned char*)0xD031) = 0xe0;

  // 80 columns requires $D016 = $C9 to be properly positioned
  POKE(0xD016U, 0xC9);

  // Put screen memory somewhere (2KB required)
  // We are using $8000-$87FF for screen
  // Using custom charset @ $A000
  *(unsigned char*)0xD018U = (((CHARSET_ADDRESS - 0x8000U) >> 11) << 1) + (((SCREEN_ADDRESS - 0x8000U) >> 10) << 4);

  // VIC RAM Bank to $8000-$BFFF
  v = *(unsigned char*)0xDD00U;
  v &= 0xfc;
  v |= 0x01;
  *(unsigned char*)0xDD00U = v;

  // Screen colours
  POKE(0xD020U, 0);
  POKE(0xD021U, 6);

  // Clear screen RAM
  lfill(SCREEN_ADDRESS, 0x20, 2000);

  // Clear colour RAM: white text
  lfill(0x1f800, 0x01, 2000);

  // Copy ASCII charset into place
  lcopy((int)&charset[0], CHARSET_ADDRESS, 0x800);

  // Set screen line address and write point
  screen_line_address = SCREEN_ADDRESS;
  screen_column = 0;

  display_footer(FOOTER_COPYRIGHT);
}

void screen_colour_line(unsigned char line, unsigned char colour)
{
  // Set colour RAM for this screen line to this colour
  // (use bit-shifting as fast alternative to multiply)
  lfill(0x1f800 + (line << 6) + (line << 4), colour, 80);
}

static unsigned char i;

void fatal_error(unsigned char* filename, unsigned int line_number)
{
  display_footer(FOOTER_FATAL);
  for (i = 0; filename[i]; i++)
    POKE(FOOTER_ADDRESS + 44 + i, filename[i]);
  POKE(FOOTER_ADDRESS + 44 + i, ':');
  i++;
  screen_decimal(FOOTER_ADDRESS + 44 + i, line_number);
  lfill(COLOUR_RAM_ADDRESS - SCREEN_ADDRESS + FOOTER_ADDRESS, 2 | ATTRIB_REVERSE, 80);
  for (;;)
    continue;
}

void set_screen_attributes(long p, unsigned char count, unsigned char attr)
{
  // This involves setting colour RAM values, so we need to either LPOKE, or
  // map the 2KB colour RAM in at $D800 and work with it there.
  // XXX - For now we are LPOKING
  long addr = COLOUR_RAM_ADDRESS - SCREEN_ADDRESS + p;
  for (i = 0; i < count; i++) {
    lpoke(addr, lpeek(addr) | attr);
    addr++;
  }
}

char read_line(char* buffer, unsigned char maxlen)
{
  char len = 0;
  char c;
  char reverse = 0x90;

  // Read input using hardware keyboard scanner

  // Flush keyboard input queue before reading input
  while (PEEK(0xD610U))
    POKE(0xD610U, 0);

  while (len < maxlen) {
    c = *(unsigned char*)0xD610U; // read char

#if 0
    reverse ^=0x20;
#endif

    // Show cursor
    lpoke(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS,
        reverse | (lpeek(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS) & 0xf));

    if ((PEEK(0xD611U) & 0x0b) >= 0x09) {
      // C= + shift, so toggle case

      // Toggle upper/lower case font
      POKE(0xD018U, PEEK(0xD018U) ^ 0x02);

      while ((PEEK(0xD611U) & 0x0b) >= 0x09)
        continue;
    }

    if (c) {

      if (c == 0x0e) {
        // Toggle upper/lower case font
        POKE(0xD018U, PEEK(0xD018U) ^ 0x02);
      }
      else if (c == 0x14) {
        // DELETE
        if (len) {
          // Remove blink attribute from this char
          lpoke(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS,
              lpeek(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS) & 0xf);

          // Go back one and erase
          len--;
          lpoke(screen_line_address + len, ' ');

          // Re-enable blink for cursor
          lpoke(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS,
              lpeek(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS) | reverse);
          buffer[len] = 0;
        }
      }
      else if (c == 0x0d) {
        buffer[len] = 0;

        // Hide cursor
        lpoke(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS,
            0x00 | (lpeek(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS) & 0xf));

        return len;
      }
      else {

        // Remove blink attribute from this char
        lpoke(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS,
            lpeek(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS) & 0xf);
        buffer[len++] = c;

        // Mask char so that it looks right using screen codes instead of ASCII codes
        if (c > 0x40)
          c &= 0x1f;
        lpoke(screen_line_address + len - 1, c);
      }

      // Clear keys from hardware keyboard scanner
      // XXX we clear all keys here, and work around a bug that causes crazy
      // fast key repeating. This can be turned back into acknowledging the
      // single key again later
      while (*(unsigned char*)0xD610U) {
        *(unsigned char*)0xd610U = 1;
      }
    }
  }

  // Hide cursor
  lpoke(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS,
      0x00 | (lpeek(len + screen_line_address + COLOUR_RAM_ADDRESS - SCREEN_ADDRESS) & 0xf));

  // clear char from queue
  while (c && (PEEK(0xD610U) == c))
    POKE(0xD610U, 1);

  return len;
}
