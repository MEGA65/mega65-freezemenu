#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "freezer_common.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

#ifdef WITH_AUDIOMIXER

unsigned char* audio_menu = "         MEGA65 AUDIO MIXER MENU        "
                            "  (C) FLINDERS UNI, M.E.G.A. 2018-2022  "
                            " cccccccccccccccccccccccccccccccccccccc "
                            "        LFT RGT PH1 PH2 BTL BTR HDL HDR "
                            "        cccccccccccccccccccccccccccccccc"
                            "   SIDLb                                "
                            "   SIDRb                                "
                            " PHONE1b                                "
                            " PHONE2b                                "
                            "BTOOTHLb                                "
                            "BTOOTHRb                                "
                            "LINEINLb                                "
                            "LINEINRb                                "
                            "  DIGILb                                "
                            "  DIGIRb                                "
                            "  MIC0Lb                                "
                            "  MIC0Rb                                "
                            "  MIC1Lb                                "
                            "  MIC1Rb                                "
                            " OPL FMb                                "
                            " MASTERb                                "
                            " cccccccccccccccccccccccccccccccccccccc "
                            " T - TEST SOUND, CURSOR KEYS - NAVIGATE "
                            " +/- ADJUST VALUE,    0/* - FAST ADJUST "
                            " F3 - SIMPLE MODE,  M - TOGGLE MIC MUTE "
                            "\0";

unsigned char* audio_menu_simple = "         MEGA65 AUDIO MIXER MENU        "
                                   "  (C) FLINDERS UNI, M.E.G.A. 2018-2022  "
                                   " cccccccccccccccccccccccccccccccccccccc "
                                   "                                        "
                                   "         LEFT OUTPUT CHANNEL:           "
                                   "        cccccccccccccccccccccccccccccccc"
                                   "    MASTERb                             "
                                   " L SID 3+4b                             "
                                   " R SID 1+2b                             "
                                   " LEFT DIGIb                             "
                                   "RIGHT DIGIb                             "
                                   "SFX OPL FMb                             "
                                   "                                        "
                                   "        RIGHT OUTPUT CHANNEL:           "
                                   "        cccccccccccccccccccccccccccccccc"
                                   "    MASTERb                             "
                                   " L SID 3+4b                             "
                                   " R SID 1+2b                             "
                                   " LEFT DIGIb                             "
                                   "RIGHT DIGIb                             "
                                   "SFX OPL FMb                             "
                                   " cccccccccccccccccccccccccccccccccccccc "
                                   " T - TEST SOUND, CURSOR KEYS - NAVIGATE "
                                   " +/- VOL, S - STEREO/MONO, W - SWAP L/R "
                                   " F3 - EXIT, M - MUTE, A - ADVANCED MODE "
                                   "\0";

void audioxbar_setcoefficient(uint8_t n, uint8_t value)
{
  // Select the coefficient
  POKE(0xD6F4, n);

  // Now wait at least 16 cycles for it to settle
  POKE(0xD020U, PEEK(0xD020U));
  POKE(0xD020U, PEEK(0xD020U));

  POKE(0xD6F5U, value);
}

uint8_t audioxbar_getcoefficient(uint8_t n)
{
  // Select the coefficient
  POKE(0xD6F4, n);

  // Now wait at least 16 cycles for it to settle
  POKE(0xD020U, PEEK(0xD020U));
  POKE(0xD020U, PEEK(0xD020U));

  return PEEK(0xD6F5U);
}

static uint8_t c, value, select_row, select_column, simple_row;
static uint8_t mute_save[4];
static uint16_t i;

void draw_advanced_mixer(void)
{
  uint16_t offset;
  uint8_t colour;

  // debug output gray
  lpoke(COLOUR_RAM_ADDRESS + 3*80 + 5, 12);
  lpoke(COLOUR_RAM_ADDRESS + 3*80 + 7, 12);
  lpoke(COLOUR_RAM_ADDRESS + 3*80 + 11, 12);
  lpoke(COLOUR_RAM_ADDRESS + 3*80 + 13, 12);
  audio_menu[3*40 + 2] = nybl_to_screen(select_column);
  audio_menu[3*40 + 3] = nybl_to_screen(select_row);

  c = 0;
  do {

    // Work out address of where to draw the value
    offset = 8 + 5 * 40;              // Start of first value location
    offset += ((c & 0x1e) >> 1) * 40; // Low bits of number indicate Y position
    offset += (c >> 3) & 0x1e;        // High bits pick the column
    offset += (c & 1) + (c & 1);      // lowest bit picks LSB/MSB
    if (c & 0x10)
      offset -= 2; // XXX Why do we need this fudge factor?

    // And get the value to display
    value = audioxbar_getcoefficient(c);
    audio_menu[offset] = nybl_to_screen(value >> 4);
    audio_menu[offset + 1] = nybl_to_screen(value);

    // Now pick the colour to display
    // We want to make it easy to find values, so we should
    // have pairs of columns for odd and even rows, and a
    // highlight colour for the currently selected coefficient
    // (or just reverse video)

    colour = 12;
    if (((c & 0x1e) >> 1) == select_row)
      colour = 13;
    if ((c >> 5) == select_column) {
      if (colour == 13)
        colour = 1;
      else
        colour = 13;
    }
    if (colour == 1) {
      // debug output
      audio_menu[3*40 + 5] = nybl_to_screen(c >> 4);
      audio_menu[3*40 + 6] = nybl_to_screen(c);
    }

    lpoke(COLOUR_RAM_ADDRESS + offset + offset + 1, colour);
    lpoke(COLOUR_RAM_ADDRESS + offset + offset + 3, colour);

  } while (++c);

  // Update the coefficients in the audio_menu display, then
  // display it after, so that we have no flicker

  // Freezer can't use printf() etc, because C64 ROM has not started, so ZP will be a mess
  // (in fact, most of memory contains what the frozen program had. Only our freezer program
  // itself has been loaded to replace some of RAM).
  for (i = 0; audio_menu[i]; i++) {
    if ((audio_menu[i] >= 'A') && (audio_menu[i] <= 'Z'))
      POKE(SCREEN_ADDRESS + i * 2 + 0, audio_menu[i] - 0x40);
    else if ((audio_menu[i] >= 'a') && (audio_menu[i] <= 'z'))
      POKE(SCREEN_ADDRESS + i * 2 + 0, audio_menu[i] - 0x20);
    else
      POKE(SCREEN_ADDRESS + i * 2 + 0, audio_menu[i]);
    POKE(SCREEN_ADDRESS + i * 2 + 1, 0);
  }
}

// clang-format off
char* numbers[80] = {
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
  "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
  "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
  "30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
  "40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
  "50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
  "60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
  "70", "71", "72", "73", "74", "75", "76", "77", "78", "79" };

unsigned int minus_db_table[256] = {
  65535L, 52026L, 41303L, 32789L, 26031, 20665, 16406, 13024,
  10339, 8208, 6516, 5173, 4107, 3260, 2588, 2054, 
  1631, 1295, 1028, 816, 648, 514, 408, 324,
  257, 204, 162, 128, 102, 81, 64, 51,
  40, 32, 25, 20, 16, 12, 10, 8,
  6, 5, 4, 3, 2, 2, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 0 };
// clang-format on

unsigned char db = 0;

void val_to_db(unsigned int val)
{
  db = 0;
  while (val < minus_db_table[db])
    db++;
}

unsigned char msg[11];
void draw_db_bar(unsigned char line, unsigned int val)
{
  unsigned int bar_addr = (unsigned int)audio_menu_simple + line * 40 + 11;
  // Work out the approximate db value of the signal
  val_to_db(val);

  // Now draw the db bar.  We allow upto 20 chars wide
  // for the range 0 -- -79db = 1/4 char per dB.
  for (i = 0; i < 20; i++) {
    if (db >= 39) {
      POKE(bar_addr + i, 0x20);
    }
    else {
      // Filled bar
      if ((39 - db) > ((i * 2)))
        POKE(bar_addr + i, 0xa0);
      // Empty cell
      else if ((39 - db) < (i * 2))
        POKE(bar_addr + i, 0x20);
      // 1/2
      else if ((39 - db) == ((i * 2) + 0))
        POKE(bar_addr + i, 117);
    }
  }

  // And the annotation to the right
  bar_addr += 24;
  if (!db) {
    snprintf(msg, 10, " 0DB");
    for (i = 0; msg[i]; i++)
      POKE(bar_addr + i, msg[i]);
    for (; i < 5; i++)
      POKE(bar_addr + i, ' ');
  }
  else {
    i = 0;
    POKE(bar_addr + i, '-');
    i++;
    if (db > 79)
      db = 79;
    for (; numbers[db][i - 1]; i++)
      POKE(bar_addr + i, numbers[db][i - 1]);
    POKE(bar_addr + i, 'D');
    i++;
    POKE(bar_addr + i, 'B');
    i++;
    for (; i < 5; i++)
      POKE(bar_addr + i, ' ');
  }
}

uint16_t v, v2;

void set_amplifier(unsigned char leftRight, unsigned short v)
{
  /*
    Map 16-bit unsigned volume level to amplifier level.
    This is not super simple, as amplifier value $00 = +24dB,
    which is not a good idea to go that high.
    $20 is safe enough on the MEGA65 R3, but $28 is about the
    limit on the MEGAphone without causing power rail sagging
    on maximum volume.  $28 should thus not be in the "red"
    zone of the mixer.

    $FF is effectively mute on the amplifier.

    So $0000 = $FF and $FFFF = $20
    So a linear mapping between those should be fine.

  */

  // This does not work, disabled!

  // avoid compiler warning because of disabled code
  if (leftRight == v)
    return;

#if 0
  unsigned char amp_value = 0x20 + (v / 293);

  // Do we have an amplifier, and if so, where is it?
  switch (PEEK(0xD629)) {
  case 0x03: // MEGA65R3
    // $FFD71DC
    // try 20 times, no endless loop please!
    for (c = 0; c < 99 && lpeek(0xffd71e1 + leftRight) != amp_value; c++) {
      lpoke(0xffd71e1 + leftRight, amp_value);
      usleep(10000U);
    }
    break;
  case 0x21: // MEGAphone R1
  case 0x22: // MEGAphone R2
  case 0x23: // MEGAphone R3
    // $FFD7030
    // try 20 times, no endless loop please!
    for (c = 0; c < 99 && lpeek(0xffd7035 + leftRight) != amp_value; c++) {
      lpoke(0xffd7035 + leftRight, amp_value);
      usleep(10000U);
    }
    break;
  }
  if (c == 99) {
    POKE(0xD020U, 2);
    POKE(0xD021U, 2);
    usleep(100000L);
    POKE(0xD020U, 6);
    POKE(0xD021U, 6);
  }
#endif
}

void change_db(unsigned char row, unsigned char change)
{
  // clang-format off
  switch (row) {
  case  0: c = 0xde; break;
  case  1: c = 0xc0; break;
  case  2: c = 0xc2; break;
  case  3: c = 0xd0; break;
  case  4: c = 0xd2; break;
  case  5: c = 0xdc; break;
  case  6: c = 0xfe; break;
  case  7: c = 0xe0; break;
  case  8: c = 0xe2; break;
  case  9: c = 0xf0; break;
  case 10: c = 0xf2; break;
  case 11: c = 0xfc; break;
  }
  // clang-format on

  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  val_to_db(v);
  if (change == 0) { // minus 1
    if (db < 80)
      db++;
  } else { // plus 1
    if (db)
      db--;
  }
  v = minus_db_table[db];
  audioxbar_setcoefficient(c + 0, v & 0xff);
  audioxbar_setcoefficient(c + 1, v >> 8);

  if (row == 0)
    set_amplifier(0, v);
  if (row == 6)
    set_amplifier(1, v);
}

void swap_coefficients(unsigned char a, unsigned char b)
{
  v = audioxbar_getcoefficient(a);
  v2 = audioxbar_getcoefficient(b);
  audioxbar_setcoefficient(a, v2);
  audioxbar_setcoefficient(b, v);
}

void stereo_swap(void)
{
  // Swap left and right sides
  swap_coefficients(0xc0, 0xe2);
  swap_coefficients(0xc1, 0xe3);
  swap_coefficients(0xc2, 0xe0);
  swap_coefficients(0xc3, 0xe1);
  swap_coefficients(0xd0, 0xf2);
  swap_coefficients(0xd1, 0xf3);
  swap_coefficients(0xd2, 0xf0);
  swap_coefficients(0xd3, 0xf1);
}

void stereo_toggle(void)
{
  v = audioxbar_getcoefficient(0xc0);
  v2 = audioxbar_getcoefficient(0xc2);
  if (v == v2) {
    v = 3;
    v2 = 12;
  }
  else {
    v = 6;
    v2 = 6;
  }

  // Make stereo with 12dB difference between left and right
  for (i = 0; i < 0x20; i += 0x20) {
    if (i & 0x20) {
      // Left side output

      // Left SID
      audioxbar_setcoefficient(0x00 + i, minus_db_table[v] & 0xff);
      audioxbar_setcoefficient(0x01 + i, minus_db_table[v] >> 8);
      // Right SID
      audioxbar_setcoefficient(0x02 + i, minus_db_table[v2] & 0xff);
      audioxbar_setcoefficient(0x03 + i, minus_db_table[v2] >> 8);
      // Left Digi
      audioxbar_setcoefficient(0x10 + i, minus_db_table[v] & 0xff);
      audioxbar_setcoefficient(0x11 + i, minus_db_table[v] >> 8);
      // Right Digi
      audioxbar_setcoefficient(0x12 + i, minus_db_table[v2] & 0xff);
      audioxbar_setcoefficient(0x13 + i, minus_db_table[v2] >> 8);
      // OPL SFX FM
      audioxbar_setcoefficient(0x1c + i, minus_db_table[v] & 0xff);
      audioxbar_setcoefficient(0x1d + i, minus_db_table[v] >> 8);
    }
    else {
      // Right side output

      // Left SID
      audioxbar_setcoefficient(0x00 + i, minus_db_table[v2] & 0xff);
      audioxbar_setcoefficient(0x01 + i, minus_db_table[v2] >> 8);
      // Right SID
      audioxbar_setcoefficient(0x02 + i, minus_db_table[v] & 0xff);
      audioxbar_setcoefficient(0x03 + i, minus_db_table[v] >> 8);
      // Left Digi
      audioxbar_setcoefficient(0x10 + i, minus_db_table[v2] & 0xff);
      audioxbar_setcoefficient(0x11 + i, minus_db_table[v2] >> 8);
      // Right Digi
      audioxbar_setcoefficient(0x12 + i, minus_db_table[v] & 0xff);
      audioxbar_setcoefficient(0x13 + i, minus_db_table[v] >> 8);
      // OPL SFX FM
      audioxbar_setcoefficient(0x1c + i, minus_db_table[v] & 0xff);
      audioxbar_setcoefficient(0x1d + i, minus_db_table[v] >> 8);
    }
  }
}

unsigned char db_bar_highlight[80];
unsigned char db_bar_lowlight[80];

void draw_simple_mixer(void)
{
  // Update the volume bars and dB levels
  // display it after, so that we have no flicker

  // Left output channel
  c = 0xde; // Master volume control
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(6, v);
  c = 0xc0; // Left SIDs
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(7, v);
  c = 0xc2; // Right SIDs
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(8, v);
  c = 0xd0; // Left DIGI
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(9, v);
  c = 0xd2; // Right DIGI
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(10, v);
  c = 0xdc; // OPL2 / FM
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(11, v);

  // Right output channel
  c = 0xfe; // Master volume control
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(15, v);
  c = 0xe0; // Left SIDs
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(16, v);
  c = 0xe2; // Right SIDs
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(17, v);
  c = 0xf0; // Left DIGI
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(18, v);
  c = 0xf2; // Right DIGI
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(19, v);
  c = 0xfc; // OPL2 / FM
  v = audioxbar_getcoefficient(c);
  v |= audioxbar_getcoefficient(c + 1) << 8;
  draw_db_bar(20, v);

  // Freezer can't use printf() etc, because C64 ROM has not started, so ZP will be a mess
  // (in fact, most of memory contains what the frozen program had. Only our freezer program
  // itself has been loaded to replace some of RAM).
  for (i = 0; audio_menu_simple[i]; i++) {
    if ((audio_menu_simple[i] >= '@') && (audio_menu_simple[i] <= 'Z'))
      POKE(SCREEN_ADDRESS + i * 2 + 0, audio_menu_simple[i] - 0x40);
    else if ((audio_menu_simple[i] >= 'b') && (audio_menu_simple[i] <= 'c'))
      POKE(SCREEN_ADDRESS + i * 2 + 0, audio_menu_simple[i] - 0x20);
    else
      POKE(SCREEN_ADDRESS + i * 2 + 0, audio_menu_simple[i]);
    POKE(SCREEN_ADDRESS + i * 2 + 1, 0);
  }

  // Work out the line to highlight

  select_column = 6 + select_row;
  if (select_row >= 6)
    select_column += 3;

  for (i = 6; i < 21; i++) {
    if (i == select_column) {
      // Highligh colouring
      lcopy((long)db_bar_highlight, COLOUR_RAM_ADDRESS + i * 80, 80);
    }
    else {
      // Normal colouring
      if (i < 12 || i > 14)
        lcopy((long)db_bar_lowlight, COLOUR_RAM_ADDRESS + i * 80, 80);
    }
  }
}

unsigned char frames;
unsigned char note;
unsigned char sid_num;
unsigned int sid_addr;
unsigned int notes[5] = { 5001, 5613, 4455, 2227, 3338 };

void test_audio(unsigned char advanced_view)
{
  /*
    Play notes and samples through 4 SIDs and left/right digi
  */

  // Reset all sids
  lfill(0xffd3400, 0, 0x100);

  // Full volume on all SIDs
  POKE(0xD418U, 0x0f);
  POKE(0xD438U, 0x0f);
  POKE(0xD458U, 0x0f);
  POKE(0xD478U, 0x0f);

  for (note = 0; note < 5; note++) {
    // clang-format off
      switch(note) {
      case 0: sid_num = 0; break;
      case 1: sid_num = 2; break;
      case 2: sid_num = 1; break;
      case 3: sid_num = 3; break;
      case 4: sid_num = 0; break;
      }
    // clang-format on

    sid_addr = 0xd400 + (0x20 * sid_num);

    // Play note
    POKE(sid_addr + 0, notes[note] & 0xff);
    POKE(sid_addr + 1, notes[note] >> 8);
    POKE(sid_addr + 4, 0x10);
    POKE(sid_addr + 5, 0x0c);
    POKE(sid_addr + 6, 0x00);
    POKE(sid_addr + 4, 0x11);

    if (advanced_view) {
      // Highlight the appropriate part of the screen
      for (i = 5 * 80; i < 7 * 80; i += 2)
        lpoke(0xff80001L + i, lpeek(0xff80001L + i) & 0x0f);
      switch (sid_num) {
      case 0:
        for (i = 0; i < 80; i += 2)
          lpoke(0xff80001L + 6 * 80 + i, lpeek(0xff80001L + 6 * 80 + i) | 0x20);
        break;
      case 1:
        for (i = 0; i < 80; i += 2)
          lpoke(0xff80001L + 6 * 80 + i, lpeek(0xff80001L + 6 * 80 + i) | 0x60);
        break;
      case 2:
        for (i = 0; i < 80; i += 2)
          lpoke(0xff80001L + 5 * 80 + i, lpeek(0xff80001L + 5 * 80 + i) | 0x20);
        break;
      case 3:
        for (i = 0; i < 80; i += 2)
          lpoke(0xff80001L + 5 * 80 + i, lpeek(0xff80001L + 5 * 80 + i) | 0x60);
        break;
      }
    }
    else {
      switch (sid_num) {
      case 0:
      case 1:
        lcopy((long)db_bar_lowlight, COLOUR_RAM_ADDRESS + 7 * 80, 80);
        lcopy((long)db_bar_lowlight, COLOUR_RAM_ADDRESS + 16 * 80, 80);
        lcopy((long)db_bar_highlight, COLOUR_RAM_ADDRESS + 8 * 80, 80);
        lcopy((long)db_bar_highlight, COLOUR_RAM_ADDRESS + 17 * 80, 80);
        break;
      case 2:
      case 3:
        lcopy((long)db_bar_highlight, COLOUR_RAM_ADDRESS + 7 * 80, 80);
        lcopy((long)db_bar_highlight, COLOUR_RAM_ADDRESS + 16 * 80, 80);
        lcopy((long)db_bar_lowlight, COLOUR_RAM_ADDRESS + 8 * 80, 80);
        lcopy((long)db_bar_lowlight, COLOUR_RAM_ADDRESS + 17 * 80, 80);
        break;
      }
    }

    // Wait 1/2 second before next note
    // (==25 frames)
    /*
       So the trick here, is that we need to decide if we are doing 4-SID mode,
       where all SIDs are 1/2 volume (gain can of course be increased to compensate),
       or whether we allow the primary pair of SIDs to be louder.
       We have to write to 4-SID registers at least every couple of frames to keep them active
    */
    for (frames = 0; frames < 35; frames++) {
      // Make sure all 4 SIDs remain active
      // by proding while waiting
      while (PEEK(0xD012U) != 0x80) {
        POKE(0xD438U, 0x0f);
        POKE(0xD478U, 0x0f);
      }

      while (PEEK(0xD012U) == 0x80)
        continue;
    }
  }

  // Clear highlight
  if (advanced_view) {
    for (i = 5 * 80; i < 7 * 80; i += 2)
      lpoke(0xff80001L + i, lpeek(0xff80001L + i) & 0x0f);
  }
  else {
    lcopy((long)db_bar_lowlight, COLOUR_RAM_ADDRESS + 9 * 80, 80);
    lcopy((long)db_bar_lowlight, COLOUR_RAM_ADDRESS + 17 * 80, 80);
    lcopy((long)db_bar_lowlight, COLOUR_RAM_ADDRESS + 7 * 80, 80);
    lcopy((long)db_bar_lowlight, COLOUR_RAM_ADDRESS + 15 * 80, 80);
  }
  // Silence SIDs gradually to avoid pops
  /*
  for (frames = 15; frames < 16; frames--) {
    while (PEEK(0xD012U) != 0x80); // wait for raster
    POKE(0xD418U, frames);
    POKE(0xD438U, frames);
    POKE(0xD458U, frames);
    POKE(0xD478U, frames);
  }
  */
  while (PEEK(0xD012U) != 0x80);
  POKE(0xD418U, 0x0);
  POKE(0xD438U, 0x0);
  POKE(0xD458U, 0x0);
  POKE(0xD478U, 0x0);

  // Reset all sids
  lfill(0xffd3400, 0, 0x80);
}

unsigned char cin;

void do_advanced_mixer(void)
{
  select_row = 0;
  select_column = 0;

  // reset colour ram
  lfill(0xff80000U, 1, 2000);

  draw_advanced_mixer();

  // clear keybuffer
  while ((cin = PEEK(0xD610U)))
    POKE(0xD610U, 0);

  while (1) {
    cin = PEEK(0xD610U);
    if (cin) {
      // Flush char from input buffer
      POKE(0xD610U, 0);

      // Get coefficient number ready
      i = (select_column << 5);
      i += (select_row << 1);
      i++;
      value = audioxbar_getcoefficient(i);

      // Process char
      switch (cin) {
      case 0x03:
      case 0xf3: // RUN/STOP or F3 to exit
        // reset colour ram
        lfill(0xff80000U, 1, 2000);
        return;
      case 0x11:
        select_row++;
        select_row &= 0x0f;
        break;
      case 0x1d:
        select_column++;
        select_column &= 0x7;
        break;
      case 0x91:
        select_row--;
        select_row &= 0x0f;
        break;
      case 0x9d:
        select_column--;
        select_column &= 0x7;
        break;
      case '+':
        value++;
        audioxbar_setcoefficient(i - 1, value);
        audioxbar_setcoefficient(i, value);
        break;
      case '0':
        value += 0x10;
        audioxbar_setcoefficient(i - 1, value);
        audioxbar_setcoefficient(i, value);
        break;
      case '-':
        value--;
        audioxbar_setcoefficient(i - 1, value);
        audioxbar_setcoefficient(i, value);
        break;
      case '*':
        value -= 0x10;
        audioxbar_setcoefficient(i - 1, value);
        audioxbar_setcoefficient(i, value);
        break;
      case 't':
      case 'T':
        test_audio(1);
        break;
      case 'm':
      case 'M':
        if (audioxbar_getcoefficient(0x14)) {
          for (i = 0x00; i < 0x100; i += 0x20) {
            audioxbar_setcoefficient(i + 0x14, 0);
            audioxbar_setcoefficient(i + 0x15, 0);
            audioxbar_setcoefficient(i + 0x16, 0);
            audioxbar_setcoefficient(i + 0x17, 0);
          }
        }
        else {
          for (i = 0x00; i < 0x100; i += 0x20) {
            audioxbar_setcoefficient(i + 0x14, 0x30);
            audioxbar_setcoefficient(i + 0x15, 0x30);
            audioxbar_setcoefficient(i + 0x16, 0x30);
            audioxbar_setcoefficient(i + 0x17, 0x30);
          }
        }
        break;
      default:
        // For invalid or unimplemented functions flash the border and screen
        POKE(0xD020U, 1);
        POKE(0xD021U, 1);
        usleep(150000L);
        POKE(0xD020U, 6);
        POKE(0xD021U, 6);
        cin = 0;
        break;
      }
      if (cin)
        draw_advanced_mixer();
    }
  }
}

void do_audio_mixer(void)
{
  select_row = 0;

  for (i = 0; i < 80; i += 2) {
    if (i >= 22 && i < 46) {
      db_bar_highlight[i + 1] = 5;
      db_bar_lowlight[i + 1] = 13;
    }
    else if (i >= 46 && i < 54) {
      db_bar_highlight[i + 1] = 8;
      db_bar_lowlight[i + 1] = 7;
    }
    else if (i >= 54 && i < 70) {
      db_bar_highlight[i + 1] = 2;
      db_bar_lowlight[i + 1] = 10;
    }
    else {
      db_bar_highlight[i + 0] = 0;
      db_bar_lowlight[i + 0] = 0;
      db_bar_highlight[i + 1] = 1;
      db_bar_lowlight[i + 1] = 12;
    }
  }

  draw_simple_mixer();

  // clear keybuffer
  while ((cin = PEEK(0xD610U)))
    POKE(0xD610U, cin);

  while (1) {
    cin = PEEK(0xD610U);
    if (cin) {
      // Flush char from input buffer
      POKE(0xD610U, 0);

      switch (cin) {
      case 0x03:
      case 0xF3: // RUN/STOP / F3 = Exit
        return;
      case 'A':
      case 'a': // Advanced mode
        simple_row = select_row;
        do_advanced_mixer();
        select_row = simple_row;
        break;
      case '+':
        change_db(0, 1); // master left
        change_db(6, 1); // master right
        break;
      case '-':
        change_db(0, 0); // master left
        change_db(6, 0); // master right
        break;
      case 0x1d: // Right = + 1 to DB of signal
        change_db(select_row, 1);
        break;
      case 0x9d: // Left = -1 to DB of signal
        change_db(select_row, 0);
        break;
      case 0x11:
        select_row++;
        if (select_row >= 12)
          select_row = 0;
        break;
      case 0x91:
        select_row--;
        if (select_row >= 12)
          select_row = 11;
        break;
      case 't':
      case 'T':
        test_audio(0); // simple view highlighting
        break;
      case 'w':
      case 'W':
        // Switch coefficients for left and right channels
        stereo_swap();
        break;
      case 's':
      case 'S':
        stereo_toggle();
        break;
      case 'm':
      case 'M': // Mute
        if (audioxbar_getcoefficient(0xfe)) {
          mute_save[0] = audioxbar_getcoefficient(0xfe);
          mute_save[1] = audioxbar_getcoefficient(0xff);
          mute_save[2] = audioxbar_getcoefficient(0xde);
          mute_save[3] = audioxbar_getcoefficient(0xdf);
          audioxbar_setcoefficient(0xfe, 0);
          audioxbar_setcoefficient(0xff, 0);
          audioxbar_setcoefficient(0xde, 0);
          audioxbar_setcoefficient(0xdf, 0);
        }
        else {
          audioxbar_setcoefficient(0xfe, mute_save[0]);
          audioxbar_setcoefficient(0xff, mute_save[1]);
          audioxbar_setcoefficient(0xde, mute_save[2]);
          audioxbar_setcoefficient(0xdf, mute_save[3]);
        }
        break;
      default:
        // For invalid or unimplemented functions flash the border and screen
        POKE(0xD020U, 1);
        POKE(0xD021U, 1);
        usleep(150000L);
        POKE(0xD020U, 6);
        POKE(0xD021U, 6);
        cin = 0;
        break;
      }
      // only draw menu if there was a keypress to handle
      if (cin)
        draw_simple_mixer();
    }
  }
}

#endif
