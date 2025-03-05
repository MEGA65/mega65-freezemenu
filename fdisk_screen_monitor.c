#include "fdisk_screen.h"
#include "fdisk_screen_monitor.h"
#include "fdisk_memory.h"
#include "ascii.h"

extern unsigned char *charset;

char *footer_messages[FOOTER_MAX + 1] = { "MEGA65 FREEZE MONITOR V00.01 :     X - RETURN TO FREEZE MENU, M - DISPLAY MEMORY",
  "MEGA65 SPRITE EDITOR V00.01 :        1 - CLEAR, 2- DRAW ETC, H - HELP, F3 - EXIT",
  "                                                                                ",
  "A FATAL ERROR HAS OCCURRED, SORRY.                                              " };

void write_line_len(char *s, char col, char length)
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

void write_line_raw(char *s, char col, char length)
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
void write_line(char *s, char col)
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
  POKE(0xD054U, (PEEK(0xD054) & 0xa8) | 0x00);

  // 80-column mode, fast CPU, extended attributes enable
  *((unsigned char *)0xD031) = 0xe0;

  // 80 columns requires $D016 = $C9 to be properly positioned
  POKE(0xD016U, 0xC9);

  // Put screen memory somewhere (2KB required)
  // We are using $8000-$87FF for screen
  // Using custom charset @ $A000
  *(unsigned char *)0xD018U = (((CHARSET_ADDRESS - 0x8000U) >> 11) << 1) + (((SCREEN_ADDRESS - 0x8000U) >> 10) << 4);

  // VIC RAM Bank to $8000-$BFFF
  v = *(unsigned char *)0xDD00U;
  v &= 0xfc;
  v |= 0x01;
  *(unsigned char *)0xDD00U = v;

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

void fatal_error(unsigned char *filename, unsigned int line_number)
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

char read_line(char *buffer, unsigned char maxlen)
{
  char len = 0;
  char c;
  char reverse = 0x90;

  // Read input using hardware keyboard scanner

  // Flush keyboard input queue before reading input
  while (PEEK(0xD610U))
    POKE(0xD610U, 0);

  while (len < maxlen) {
    c = *(unsigned char *)0xD610U; // read char

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
      while (*(unsigned char *)0xD610U) {
        *(unsigned char *)0xd610U = 1;
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
