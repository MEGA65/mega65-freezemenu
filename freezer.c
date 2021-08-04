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

static uint8_t sector_buffer[512];

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

unsigned char* freeze_menu = "        MEGA65 FREEZE MENU V0.1.3       "
                             "  (C) MUSEUM OF ELECTRONIC GAMES & ART  "
                             " cccccccccccccccccccccccccccccccccccccc "
#define LOAD_RESUME_OFFSET (3 * 40 + 4)
                             " F3-LOAD     F5-RESET   F7-SAVE TO SLOT "
                             " cccccccccccccccccccccccccccccccccccccc "
#define CPU_MODE_OFFSET (5 * 40 + 13)
#define JOY_SWAP_OFFSET (5 * 40 + 36)
                             " (C)PU MODE:   4510  (J)OY SWAP:    YES "
#define ROM_NAME_OFFSET (6 * 40 + 8)
#define CART_ENABLE_OFFSET (6 * 40 + 36)
                             " (R)OM:  C65 911101  CAR(T) ENABLE: YES "
#define CPU_FREQ_OFFSET (7 * 40 + 13)
#define VIDEO_MODE_OFFSET (7 * 40 + 33)
                             " CPU (F)REQ: 40 MHZ  (V)IDEO:    NTSC60 "
                             " cccccccccccccccccccccccccccccccccccccc "
                             " M - MONITOR         E - POKES          "
                             " P - (UN)PROTECT ROM S - SPRITE EDITOR  "
                             " A - AUDIO & VOLUME  X - POKE FINDER    "
                             " cccccccccccccccccccccccccccccccccccccc "
                             "                                        "
#define PROCESS_NAME_OFFSET (14 * 40 + 21)
                             "                                        "
                             "                                        "
#define PROCESS_ID_OFFSET (16 * 40 + 34)
#define SLOT_NUMBER_OFFSET (17 * 40 + 34)
                             "                     TASK ID:           "
#define FREEZE_SLOT_OFFSET (17 * 40 + 20)
                             "                     FREEZE SLOT:       "
                             "                                        "

                             "                     (0) INTERNAL DRIVE:"
#define DRIVE0_NUM_OFFSET (20 * 40 + 37)
                             "                         (8) DEVICE #   "
#define D81_IMAGE0_NAME_OFFSET (21 * 40 + 22)
                             "                                        "
                             "                     (1) EXTERNAL 1565: "
#define DRIVE1_NUM_OFFSET (23 * 40 + 37)
                             "                         (9) DEVICE #   "
#define D81_IMAGE1_NAME_OFFSET (24 * 40 + 22)
                             "                                        "
                             "\0";

static unsigned short i;
char* deadly_haiku[3] = { "Error consumes all", "As sand erodes rock and stone", "Now also your mind" };


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
};

void setup_menu_screen(void)
{
  POKE(0xD018U, 0x15); // upper case

  // We now keep video mode, assuming the user to have correctly set it up
  // NTSC 60Hz mode for monitor compatibility?
  //  POKE(0xD06FU,0x80);

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
  POKE(0xD054U, 0x05);
  POKE(0xD058U, 80);
  POKE(0xD059U, 0); // 80 bytes per row

  // Screen at $00B800
  POKE(0xD060,0);
  POKE(0xD061,0xB8);
  POKE(0xD062,0);
  POKE(0xD063,0);
  // Colour RAM at offset $0000
  POKE(0xD064,0);
  POKE(0xD065,0);  
  
  // Fill colour RAM with sensible value at the start
  lfill(0xff8000U, 1, 2000);
}

unsigned char ascii_to_screencode(char c)
{
  if (c >= 0x60)
    return c - 0x60;
  return c;
}

void screen_of_death(char* msg)
{
#if 0
  POKE(0,0x41);
  POKE(0xD02FU,0x47); POKE(0xD02FU,0x53);

  // Reset video mode
  POKE(0xD05DU,0x01); POKE(0xD011U,0x1b); POKE(0xD016U,0xc8);
  POKE(0xD018U,0x17); // lower case
  POKE(0xD06FU,0x80); // NTSC 60Hz mode for monitor compatibility?
  POKE(0xD06AU,0x00); // Charset from bank 0

  // No sprites
  POKE(0xD015U,0x00);
  
  // Normal video mode
  POKE(0xD054U,0x00);

  // Reset colour palette to normal for black and white
  POKE(0xD100U,0x00);  POKE(0xD200U,0x00);  POKE(0xD300U,0x00);
  POKE(0xD101U,0xFF);  POKE(0xD201U,0xFF);  POKE(0xD301U,0xFF);
  
  POKE(0xD020U,0); POKE(0xD021U,0);

  // Reset CPU IO ports
  POKE(1,0x3f); POKE(0,0x3F);
  lfill(0x0400U,' ',1000);
  lfill(0xd800U,1,1000);

  for(i=0;deadly_haiku[0][i];i++) POKE(0x0400+10*40+11+i,ascii_to_screencode(deadly_haiku[0][i]));
  for(i=0;deadly_haiku[1][i];i++) POKE(0x0400+12*40+11+i,ascii_to_screencode(deadly_haiku[1][i]));
  for(i=0;deadly_haiku[2][i];i++) POKE(0x0400+14*40+11+i,ascii_to_screencode(deadly_haiku[2][i]));
#endif
  while (1)
    continue;
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

unsigned char thumbnail_buffer[4096];

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
  // But there isn't currently a good solution to this, short of having
  // a second buffer into which to render it.
  unsigned char x, y, i;
  unsigned short yoffset,yoffset_out,xoffset,j;
  uint32_t thumbnail_sector = find_thumbnail_offset();

  // Can't find thumbnail area?  Then show no thumbnail
  if (thumbnail_sector == 0xFFFFFFFFL) {
    lfill(0x50000L, 0, 10 * 6 * 64);
    return;
  }
  // Copy thumbnail memory to buffer
  for (i = 0; i < 8; i++) {
    sdcard_readsector(freeze_slot_start_sector + thumbnail_sector + i);
    lcopy((long)sector_buffer, thumbnail_buffer + (i * 0x200), 0x200);
    NAVIGATION_KEY_CHECK();
  }

  // Pick colours of all pixels in the thumbnail
  for(j=0;j<4096;j++) thumbnail_buffer[j]=colour_table[thumbnail_buffer[j]];
  
  // Rearrange pixels
  yoffset = 80+14; // skip dud first line
  for (y = 0; y < 48; y++) {
    yoffset_out = ((y & 7) << 3) + (y >> 3) * 64;
    xoffset=0;
    for (x = 0; x < 73; x+=8) {
      // Also the whole thing is rotated by one byte, so add that on as we plot the pixel
      // PGS Optimise here

      j=8; if (x==72) j=2;

      lcopy ((unsigned long)&thumbnail_buffer[x + yoffset],
	     0x50000L + xoffset + yoffset_out,
	     j);

      xoffset+=64*6;      
    }
    NAVIGATION_KEY_CHECK();
    yoffset += 80;
  }

}

struct process_descriptor_t process_descriptor;

void draw_freeze_menu(void)
{
  unsigned char x, y;
  // Wait until we are in vertical blank area before redrawing, so that we don't have flicker

  // Update messages based on the settings we allow to be easily changed

  if (slot_number) {
    lcopy((unsigned long)"LOAD  ", (unsigned long)&freeze_menu[LOAD_RESUME_OFFSET], 6);
    lcopy((unsigned long)" FREEZE SLOT:      ", (unsigned long)&freeze_menu[FREEZE_SLOT_OFFSET], 19);
    // Display slot ID as decimal
    screen_decimal((unsigned long)&freeze_menu[SLOT_NUMBER_OFFSET], slot_number);
  }
  else {
    lcopy((unsigned long)"RESUME", (unsigned long)&freeze_menu[LOAD_RESUME_OFFSET], 6);

    // Display "- PAUSED STATE -"
    lcopy((unsigned long)" - PAUSED STATE -   ", (unsigned long)&freeze_menu[FREEZE_SLOT_OFFSET], 19);
  }

  // Draw drive numbers for internal drive
  lfill((unsigned long)&freeze_menu[DRIVE0_NUM_OFFSET], 0, 2);
  lfill((unsigned long)&freeze_menu[DRIVE1_NUM_OFFSET], 0, 2);
  screen_decimal((unsigned long)&freeze_menu[DRIVE0_NUM_OFFSET], freeze_peek(0x10113L));
  screen_decimal((unsigned long)&freeze_menu[DRIVE1_NUM_OFFSET], freeze_peek(0x10114L));

  // CPU MODE
  if (freeze_peek(0xffd367dL) & 0x20)
    lcopy((unsigned long)"  4502", (unsigned long)&freeze_menu[CPU_MODE_OFFSET], 6);
  else
    lcopy((unsigned long)"  AUTO", (unsigned long)&freeze_menu[CPU_MODE_OFFSET], 6);

  // Joystick 1/2 swap
  lcopy((unsigned long)((PEEK(0xd612L) & 0x20) ? "YES" : " NO"), (unsigned long)&freeze_menu[JOY_SWAP_OFFSET], 3);

  // ROM version
  lcopy((long)detect_rom(), (unsigned long)&freeze_menu[ROM_NAME_OFFSET], 11);

  // Cartridge enable
  lcopy(
      (unsigned long)((freeze_peek(0xffd367dL) & 0x01) ? "YES" : " NO"), (unsigned long)&freeze_menu[CART_ENABLE_OFFSET], 3);

  // CPU frequency
  switch (detect_cpu_speed()) {
  case 1:
    lcopy((unsigned long)"  1", (unsigned long)&freeze_menu[CPU_FREQ_OFFSET], 3);
    break;
  case 2:
    lcopy((unsigned long)"  2", (unsigned long)&freeze_menu[CPU_FREQ_OFFSET], 3);
    break;
  case 3:
    lcopy((unsigned long)"3.5", (unsigned long)&freeze_menu[CPU_FREQ_OFFSET], 3);
    break;
  case 40:
    lcopy((unsigned long)" 40", (unsigned long)&freeze_menu[CPU_FREQ_OFFSET], 3);
    break;
  default:
    lcopy((unsigned long)"???", (unsigned long)&freeze_menu[CPU_FREQ_OFFSET], 3);
    break;
  }

  if (freeze_peek(0xffd306fL) & 0x80) {
    // NTSC60
    lcopy((unsigned long)"NTSC60", (unsigned long)&freeze_menu[VIDEO_MODE_OFFSET], 6);
  }
  else {
    // PAL50
    lcopy((unsigned long)" PAL50", (unsigned long)&freeze_menu[VIDEO_MODE_OFFSET], 6);
  }

  /* Display info from the process descriptor
     The useful bits are:
     $00     - Task ID (0-255, $FF = operating system)
     $01-$10 - Process name (16 characters)
     $11     - D81 image 0 flags
     $12     - D81 image 1 flags
     $13     - D81 image 0 name len
     $14     - D81 image 1 name len
     $15-$34 - D81 image 0 file name (max 32 chars, not null terminated)
     $35-$54 - D81 image 0 file name (max 32 chars, not null terminated)
     $55-$7F - RESERVED
     $80-$FF - File descriptors

     We should just read the sector containing all this, and get it out all at once.
  */
  lfill((long)&process_descriptor, 0, sizeof(process_descriptor));
  freeze_fetch_sector(0xFFFBD00L, (unsigned char*)&process_descriptor);

  // Display process ID as decimal
  screen_decimal((unsigned long)&freeze_menu[PROCESS_ID_OFFSET], process_descriptor.task_id);

  // Blank out process descriptor fields
  lfill((unsigned long)&freeze_menu[PROCESS_NAME_OFFSET], '?', 16);
  lfill((unsigned long)&freeze_menu[D81_IMAGE0_NAME_OFFSET], ' ', 19);
  lfill((unsigned long)&freeze_menu[D81_IMAGE1_NAME_OFFSET], ' ', 19);

  if ((process_descriptor.process_name[0] >= ' ') && (process_descriptor.process_name[0] <= 0x7f)) {
    // Process name: But only display if valid
    for (i = 0; i < 16; i++)
      if (!process_descriptor.process_name[i])
        break;
    if (i == 16)
      lcopy((unsigned long)process_descriptor.process_name, (unsigned long)&freeze_menu[PROCESS_NAME_OFFSET], 16);

    // Show name of current mounted disk image
    if (process_descriptor.d81_image0_namelen) {
      for (i = 0; i < process_descriptor.d81_image0_namelen; i++)
        if (!process_descriptor.d81_image0_name[i])
          break;
      if (i == process_descriptor.d81_image0_namelen)
        lcopy((unsigned long)process_descriptor.d81_image0_name, (unsigned long)&freeze_menu[D81_IMAGE0_NAME_OFFSET],
            process_descriptor.d81_image0_namelen < 19 ? process_descriptor.d81_image0_namelen : 19);
    }
    if (process_descriptor.d81_image1_namelen) {
      for (i = 0; i < process_descriptor.d81_image1_namelen; i++)
        if (!process_descriptor.d81_image1_name[i])
          break;
      if (i == process_descriptor.d81_image1_namelen)
        lcopy((unsigned long)process_descriptor.d81_image1_name, (unsigned long)&freeze_menu[D81_IMAGE1_NAME_OFFSET],
            process_descriptor.d81_image1_namelen < 19 ? process_descriptor.d81_image1_namelen : 19);
    }
  }

  while (PEEK(0xD012U) < 0xf8)
    continue;

  // Clear screen, blue background, white text, like Action Replay
  POKE(0xD020U, 6);
  POKE(0xD021U, 6);

  lfill(SCREEN_ADDRESS, 0, 2000);
  lfill(0xFF80000L, 1, 2000);
  // Make disk image names different colour to avoid confusion
  for (i = 40; i < 80; i += 2) {
    lpoke(0xff80000 + 21 * 80 + 1 + i, 0xe);
    lpoke(0xff80000 + 24 * 80 + 1 + i, 0xe);
  }

  // Freezer can't use printf() etc, because C64 ROM has not started, so ZP will be a mess
  // (in fact, most of memory contains what the frozen program had. Only our freezer program
  // itself has been loaded to replace some of RAM).
  for (i = 0; freeze_menu[i]; i++) {
    if ((freeze_menu[i] >= 'A') && (freeze_menu[i] <= 'Z'))
      POKE(SCREEN_ADDRESS + i * 2 + 0, freeze_menu[i] - 0x40);
    else if ((freeze_menu[i] >= 'a') && (freeze_menu[i] <= 'z'))
      POKE(SCREEN_ADDRESS + i * 2 + 0, freeze_menu[i] - 0x20);
    else
      POKE(SCREEN_ADDRESS + i * 2 + 0, freeze_menu[i]);
    POKE(SCREEN_ADDRESS + i * 2 + 1, 0);
  }

  // Draw the thumbnail surround area
  //  if ((process_descriptor.process_name[0] >= ' ') && (process_descriptor.process_name[0] <= 0x7f)) {
  if (1) {
    unsigned char snail = 0;
    uint32_t screen_data_start;
    unsigned short* tile_num;
    unsigned short tile_offset;
    if (detect_rom()[3] == '5') {
      if (detect_cpu_speed() == 1) {
#ifdef WITH_GUS
        read_file_from_sdcard("GUSTHUMB.M65", 0x052000L);
        snail = 1;
#else
        read_file_from_sdcard("C64THUMB.M65", 0x052000L);
        snail = 0;
#endif
      }
      else {
        read_file_from_sdcard("C65THUMB.M65", 0x052000L);
        snail = 0;
      }
    }
    else {
      read_file_from_sdcard("C64THUMB.M65", 0x052000L);
      snail = 0;
    }
#if 0
      if (detect_cpu_speed()==40) {
	read_file_from_sdcard("F40THUMB.M65",0x052000L);
	snail=1;
      }
#endif

    // Work out where the tile data begins
    screen_data_start = 0x52000L + 0x300L + 0x40L;
    tile_offset = (screen_data_start >> 6);
    // Work out where the screen data begins
    screen_data_start = lpeek(0x5203dL) + (lpeek(0x5203eL) << 8);
    screen_data_start += 0x52000L + 0x40L;
    for (y = 0; y < 13; y++) {
      // Copy row of screen data
      lcopy(screen_data_start + (y << 6), SCREEN_ADDRESS + (13 * 80) + (y * 80), (19 * 2));
      // Add tile number based on data starting at $52040 = $1481
      for (x = 0; x < 19; x++) {
        tile_num = (unsigned short*)(SCREEN_ADDRESS + (13 * 80) + (y * 80) + (x << 1));
        if (*tile_num)
          (*tile_num) += tile_offset;
        else
          *tile_num = 0x20;
      }
    }

    // Now draw the 10x6 character block for thumbnail display itself
    // This sits in the region below the menu where we will also have left and right arrows,
    // the program name etc, so you can easily browse through the freeze slots.
    if (snail) {
      for (x = 0; x < 9; x++)
        for (y = 0; y < 6; y++) {
          POKE(SCREEN_ADDRESS + (80 * 16) + (8 * 2) + (x * 2) + (y * 80) + 0, x * 6 + y); // $50000 base address
          POKE(SCREEN_ADDRESS + (80 * 16) + (8 * 2) + (x * 2) + (y * 80) + 1, 0x14);      // $50000 base address
        }
    }
    else {
      for (x = 0; x < 9; x++)
        for (y = 0; y < 6; y++) {
          POKE(SCREEN_ADDRESS + (80 * 14) + (5 * 2) + (x * 2) + (y * 80) + 0, x * 6 + y); // $50000 base address
          POKE(SCREEN_ADDRESS + (80 * 14) + (5 * 2) + (x * 2) + (y * 80) + 1, 0x14);      // $50000 base address
        }
    }
  }
}

// Left/right do left/right
// fire = F3
// down = disk menu
// up = toggle PAL/NTSC ?
unsigned char joy_to_key[32] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF3,       // With fire pressed
  0, 0, 0, 0, 0, 0, 0, 0x1d, 0, 0, 0, 0x9d, 0, 'd', 'v', 0 // without fire
};

#ifdef WITH_TOUCH
// clang-format off
unsigned char touch_keys[2][9] = {
  { 0xF3, 0x00, 'c', 'r', 'f', 0x00, 'm', 'a', 'd' },
  { 0xF7, 0x00, 'j', 't', 'v', 0x00, 'e', 'k', 'x' } };
// clang-format on

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

  // Put $DD00 DDR back to default
  POKE(0xDD02,0xFF);
  
  // Enable extended attributes so we can use reverse
  POKE(0xD031U, PEEK(0xD031U) | 0x20);

  // Correct horizontal scaling
  POKE(0xD05AU, 0x78);
  
  // Reset character set address
  POKE(0xD068, 0x00);
  POKE(0xD069, 0x10);
  POKE(0xD06A, 0x00);

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

  draw_freeze_menu();

  draw_thumbnail();

  // Flush input buffer
  mega65_fast();
  while (PEEK(0xD610U))
    POKE(0xD610U, 0);

  // Ensure correct keyboard DDR etc
  POKE(0xDC00U, 0xFF);
  POKE(0xDC02U, 0x00);

  // Main keyboard input loop
  while (1) {
    {
      unsigned char c = PEEK(0xD610U);

      // Flush char from input buffer
      if (c)
        POKE(0xD610U, 0);
      else {

        // If no keyboard input, check for joystick input
        // We should make this context sensitive, but for now just want
        // easy choosing of frozen programs to run, so fire will be F3,
        // and left and right on the joystick will be left and right
        // cursor keys.
        // We use a simple lookup table to do this
        c = joy_to_key[PEEK(0xDC00) & PEEK(0xDC01) & 0x1f];
        // Then wait for joystick to release
        while ((PEEK(0xDC00) & PEEK(0xDC01) & 0x1f) != 0x1f)
          continue;
      }
#ifdef WITH_TOUCH
      if (!c) {
        // Check for touch panel activity
        poll_touch_panel();
        if ((last_touch & 1) && (!(PEEK(0xD6B0) & 1))) {
          if (y > 8 && y < 17) {
            if (x < 26)
              x = 0;
            else
              x = 1;
            c = touch_keys[x][y - 9];
            // Wait for touch to be released
            // XXX - Records touch event as where your finger was when you touched, not released.
            while (PEEK(0xD6B0) & 1)
              continue;
          }
        }

        // Set speaker volume by placing finger along top edge of screen.
        // (We now also support setting the amplifier gain from in the audio mixer,
        // including setting the gain for stereo speakers.  This little hack below
        // will likely disappear when we add touch support to the audio mixer)
        if (y > 0 && y < 7) {
          if (PEEK(0xD6B0) & 1) {
            if (x > 5)
              x -= 5;
            else
              x = 0;
            if (x > 39)
              x = 39;
            for (y = 0; y < x * 2; y += 2)
              lpoke(SCREEN_ADDRESS + y, 0xA0);
            for (; y < 80; y += 2)
              lpoke(SCREEN_ADDRESS + y, 0x20);
            y = 0;
            lpoke(0xFFD7035L, 0xff - (x * 5));
          }
        }

        // Check for side/side swiping
        // (In theory the touch panel supports gestures, but we have not got them working.
        // so we will just infer them.  Move sideways more than 3 characters within a short
        // period of time will be deemed to be a side swipe).
        if (y > 17) {
          if (PEEK(0xD6B0) & 1) {
            if (x > last_x && swipe_dir < 0)
              swipe_dir = 1;
            if (x > last_x && swipe_dir >= 0)
              swipe_dir++;
            if (x > last_x) {
              // Swipe screen to the right

              // Copy is overlapping, so copy it somewhere else first, then copy it down
              lcopy(SCREEN_ADDRESS + (80 * 13), 0x40000L, 12 * 80 - 2);
              lcopy(0x40000, SCREEN_ADDRESS + (80 * 13) + 2, 12 * 80 - 2);
            }

            if ((x < last_x) && (swipe_dir > 0))
              swipe_dir = -1;
            if ((x < last_x))
              swipe_dir--;
            if (x < last_x) {
              // Swipe screen to the left
              lcopy(SCREEN_ADDRESS + (80 * 13), SCREEN_ADDRESS + (80 * 13) - 2, 12 * 80 - 2);
            }

            if (swipe_dir == -5) {
              c = 0x1d;
              swipe_dir = 0;
            }
            if (swipe_dir == 5) {
              c = 0x9d;
              swipe_dir = 0;
            }

            last_x = x;
          }
          else {
            if (last_touch & 1) { }
          }
        }
      }
      last_touch = PEEK(0xD6B0);
#endif

      // Process char
      if (c)
        switch (c) {

        case 0x11: // Cursor down
        case 0x9D: // Cursor left
          if (slot_number)
            slot_number--;
          else
            slot_number = get_freeze_slot_count() - 1;
          POKE(0xD020U, 0);
          find_freeze_slot_start_sector(slot_number);
          freeze_slot_start_sector = *(uint32_t*)0xD681U;

          draw_freeze_menu();
	  draw_thumbnail();
          POKE(0xD020U, 6);
          break;
        case 0x91: // Cursor up
        case 0x1D: // Cursor right
          if ((slot_number + 1) < get_freeze_slot_count())
            slot_number++;
          else
            slot_number = 0;
          POKE(0xD020U, 0);
          find_freeze_slot_start_sector(slot_number);
          freeze_slot_start_sector = *(uint32_t*)0xD681U;

          draw_freeze_menu();
	  draw_thumbnail();
          POKE(0xD020U, 6);
          break;

        case 'M':
        case 'm': // Monitor
          freeze_monitor();
          setup_menu_screen();
          draw_freeze_menu();
          break;

        case 'A':
        case 'a': // Audio mixer
          mega65_dos_exechelper("AUDIOMIX.M65");
          break;

        case 'S':
        case 's': // Sprite Editor
          mega65_dos_exechelper("SPRITED.M65");
          break;

        case 'J':
        case 'j': // Toggle joystick swap
          POKE(0xD612L, (PEEK(0xD612L) ^ 0x20) & 0xEF);

          draw_freeze_menu();
          break;

        case 'T':
        case 't': // Toggle cartridge enable
          freeze_poke(0xFFD367dL, freeze_peek(0xFFD367dL) ^ 0x01);
          draw_freeze_menu();
          break;

#if 0
      case 'P': case 'p': // Toggle ROM area write-protect
	freeze_poke(0xFFD367dL,freeze_peek(0xFFD367dL)^0x04);
	draw_freeze_menu();
	break;
#endif

        case 'c':
        case 'C': // Toggle CPU mode
          freeze_poke(0xFFD367dL, freeze_peek(0xFFD367dL) ^ 0x20);
          draw_freeze_menu();
          break;

        case 'F':
        case 'f': // Change CPU speed
          next_cpu_speed();
          draw_freeze_menu();
          break;

        case 'V':
        case 'v': // Toggle video mode
          // Toggle video mode setting
          // Then also toggle vertical border and text/graphics area positions, by updating the following:
          // $FFD3048 = LSB, top border position
          // $FFD3049.0-3 = MSB, top border position
          // $FFD3049.7-4 = PRESERVE
          // $FFD304A = LSB, bottom border position
          // $FFD304B.0-3 = MSB, bottom border position
          // $FFD304B.7-4 = PRESERVE
          // $FFD304E = TEXTYPOS LSB
          // $FFD304F.0-3 = TEXTYPOS MSB
          // $FFD304F.4-7 = PRESERVE
          // $FFD306F.0-5 = VIC-II first raster
          // $FFD3072     = Sprite Y position adjust
          c = freeze_peek(0xFFD306fL) & 0x80;
          if (c == 0x80) {
            // Switch to PAL
            freeze_poke(0xFFD306fL, 0x00);
            freeze_poke(0xFFD3072L, 0x00);
            freeze_poke(0xFFD3048L, 0x69);
            freeze_poke(0xFFD3049L, 0x0 + (lpeek(0xFFD3049L) & 0xf0));
            freeze_poke(0xFFD304AL, 0xFA);
            freeze_poke(0xFFD304BL, 0x1 + (lpeek(0xFFD304BL) & 0xf0));
            freeze_poke(0xFFD304EL, 0x69);
            freeze_poke(0xFFD304FL, 0x0 + (lpeek(0xFFD304FL) & 0xf0));
            freeze_poke(0xFFD3072L, 0);
          }
          else {
            // Switch to NTSC
            freeze_poke(0xFFD306fL, 0x87);
            freeze_poke(0xFFD3072L, 0x18);
            freeze_poke(0xFFD3048L, 0x2A);
            freeze_poke(0xFFD3049L, 0x0 + (lpeek(0xFFD3049L) & 0xf0));
            freeze_poke(0xFFD304AL, 0xB9);
            freeze_poke(0xFFD304BL, 0x1 + (lpeek(0xFFD304BL) & 0xf0));
            freeze_poke(0xFFD304EL, 0x2A);
            freeze_poke(0xFFD304FL, 0x0 + (lpeek(0xFFD304FL) & 0xf0));
            freeze_poke(0xFFD3072L, 24);
          }
          draw_freeze_menu();
          break;
        case '8':
        case '9':
          // Change drive number of internal drives
          freeze_poke(0x10113L - '8' + c, freeze_peek(0x10113L - '8' + c) ^ 2);
          draw_freeze_menu();
          break;
        case '0': // Select mounted disk image
        {
          char* disk_image = freeze_select_disk_image(0);

          // Restore freeze region offset list to $0400 screen
          request_freeze_region_list();

          if ((unsigned short)disk_image == 0xFFFF) {
            // Have no disk image
          }
          else if (disk_image) {

            {
              unsigned char i;
              POKE(0xD020U, 6);

              // Replace disk image name in process descriptor block
              for (i = 0; (i < 32) && disk_image[i]; i++)
                freeze_poke(0xFFFBD00L + 0x15 + i, disk_image[i]);
              // Update length of name
              freeze_poke(0xFFFBD00L + 0x13, i);
              // Pad with spaces as required by hypervisor
              for (; i < 32; i++)
                freeze_poke(0xFFFBD00L + 0x15 + i, ' ');
            }
          }
        }

          draw_freeze_menu();
          break;
        case '1': // Select mounted disk image for 2nd drive
        {
          char* disk_image = freeze_select_disk_image(1);

          // Restore freeze region offset list to $0400 screen
          request_freeze_region_list();

          if ((unsigned short)disk_image == 0xFFFF) {
            // Have no disk image
          }
          else if (disk_image) {

            {
              unsigned char i;
              POKE(0xD020U, 6);

              // Replace disk image name in process descriptor block
              for (i = 0; (i < 32) && disk_image[i]; i++)
                freeze_poke(0xFFFBD00L + 0x35 + i, disk_image[i]);
              // Update length of name
              freeze_poke(0xFFFBD00L + 0x14, i);
              // Pad with spaces as required by hypervisor
              for (; i < 32; i++)
                freeze_poke(0xFFFBD00L + 0x35 + i, ' ');
            }
          }
        }
          draw_freeze_menu();
          break;

        case 0xf5: // F5 = Reset
        {
          // Set C64 memory map, PC to reset vector and resume
          freeze_poke(0xFFD3640U + 8, freeze_peek(0x2FFFCL));
          freeze_poke(0xFFD3640U + 9, freeze_peek(0x2FFFDL));
          // Reset $01 port values
          freeze_poke(0xFFD3640U + 0x10, 0x3f);
          freeze_poke(0xFFD3640U + 0x11, 0x3f);
          // disable interrupts, clear decimal mode
          freeze_poke(0xFFD3640U + 0x07, 0xe7);
          // Clear memory mapping
          for (c = 0x0a; c <= 0x0f; c++)
            freeze_poke(0xFFD3640U + c, 0);
        }
          // fall through
        case 0xf3: // F3 = resume
          unfreeze_slot(slot_number);

          // should never get here
          screen_of_death("unfreeze failed");

          break;

        case 0xf7: // F7 = save to slot
        {
          // Get start sectors of the source and destination slots
          uint32_t i;
          uint32_t j;
          uint32_t dest_freeze_slot_start_sector;
          find_freeze_slot_start_sector(0);
          freeze_slot_start_sector = *(uint32_t*)0xD681U;
          find_freeze_slot_start_sector(slot_number);
          dest_freeze_slot_start_sector = *(uint32_t*)0xD681U;

          // 512KB = 1024 sectors
          // Process in 64KB blocks, so that we can do multi-sector writes
          // and generally be about 10x faster than otherwise.
          for (i = 0; i < 1024; i += 128) {
            POKE(0xD020U, 0x0e);
            for (j = 0; j < 128; j++) {
              sdcard_readsector(freeze_slot_start_sector + i + j);
              lcopy((unsigned long)sector_buffer, 0x40000U + (j << 9), 512);
            }
            POKE(0xD020U, 0x00);
            for (j = 0; j < 128; j++) {
              lcopy(0x40000U + (j << 9), (unsigned long)sector_buffer, 512);
#ifdef USE_MULTIBLOCK_WRITE
              if (!j)
                sdcard_writesector(dest_freeze_slot_start_sector + i + j, 1);
              else
                sdcard_writenextsector();
#else
              sdcard_writesector(dest_freeze_slot_start_sector + i + j, 0);
#endif
            }
#ifdef USE_MULTIBLOCK_WRITE
            // Close multi-sector write job
            sdcard_writemultidone();
#endif
          }
          POKE(0xD020U, 6);

          draw_freeze_menu();
        } break;

        case 'R':
        case 'r': // Switch ROMs
          mega65_dos_exechelper("ROMLOAD.M65");
          break;

        case 'X':
        case 'x': // Poke finder
        case 'E':
        case 'e': // Enter POKEs
        case 'k':
        case 'K': // Sprite killer
        default:
          // For invalid or unimplemented functions flash the border and screen
          POKE(0xD020U, 1);
          POKE(0xD021U, 1);
          usleep(150000L);
          POKE(0xD020U, 6);
          POKE(0xD021U, 6);
          break;
        }
    }
  }

  return;
}
