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

unsigned char* freeze_menu_bar = (unsigned char *)
                             "F3-RESUME    F5-RESET      HELP-MEGAINFO"
                             "F3-LOAD SLOT F7-SAVE SLOT  HELP-MEGAINFO";

unsigned char* freeze_menu = (unsigned char *)
                             "        MEGA65 FREEZE MENU V0.3.0       "
                             "  (C) MUSEUM OF ELECTRONIC GAMES & ART  "
                             "cccccccccccccccccccccccccccccccccccccccc"
#define LOAD_RESUME_OFFSET (3 * 40)
                             "F3-RESUME    F5-RESET      HELP-MEGAINFO"
                             "cccccccccccccccccccccccccccccccccccccccc"
#define CPU_MODE_OFFSET (5 * 40 + 13)
#define JOY_SWAP_OFFSET (5 * 40 + 36)
                             " (C)PU MODE:   4510  (J)OY SWAP:    YES "
#define CPU_FREQ_OFFSET (6 * 40 + 13)
#define CART_ENABLE_OFFSET (6 * 40 + 36)
                             " CPU (F)REQ: 40 MHZ  CAR(T) ENABLE: YES "
// #define ROM_NAME_OFFSET (7 * 40 + 8)
#define CRTEMU_MODE_OFFSET (7 * 40 + 16)
#define VIDEO_MODE_OFFSET (7 * 40 + 33)
                             " C(R)T EMU:     OFF  (V)IDEO:    NTSC60 "
                             "cccccccccccccccccccccccccccccccccccccccc"
#define TOOLS_MENU_OFFSET (9 * 40)
                             " M - MONITOR         L - LOAD ROM/CHAR  "
                             " A - AUDIO & VOLUME                     "
                             " S - SPRITE EDITOR   HELP - MEGAINFO    "
                             "cccccccccccccccccccccccccccccccccccccccc"
                             "~~~~~~~~~~~~~~~~~~~~                    "
#define PROCESS_NAME_OFFSET (14 * 40 + 21)
                             "~~~~~~~~~~~~~~~~~~~~                    "
#define PROCESS_ROM_OFFSET (15 * 40 + 26)
                             "~~~~~~~~~~~~~~~~~~~~ ROM:               "
#define PROCESS_ID_OFFSET (16 * 40 + 34)
#define SLOT_NUMBER_OFFSET (17 * 40 + 34)
                             "~~~~~~~~~~~~~~~~~~~~ TASK ID:           "
#define FREEZE_SLOT_OFFSET (17 * 40 + 20)
                             "~~~~~~~~~~~~~~~~~~~~ FREEZE SLOT:       "
                             "~~~~~~~~~~~~~~~~~~~~                    "

                             "~~~~~~~~~~~~~~~~~~~~ (0) INTERNAL DRIVE:"
#define DRIVE0_NUM_OFFSET (20 * 40 + 35)
                             "~~~~~~~~~~~~~~~~~~~~     (8) UNIT #     "
#define D81_IMAGE0_NAME_OFFSET (21 * 40 + 22)
                             "~~~~~~~~~~~~~~~~~~~~                    "
                             "~~~~~~~~~~~~~~~~~~~~ (1) EXTERNAL 1565: "
#define DRIVE1_NUM_OFFSET (23 * 40 + 35)
                             "~~~~~~~~~~~~~~~~~~~~     (9) UNIT #     "
#define D81_IMAGE1_NAME_OFFSET (24 * 40 + 22)
                             "~~~~~~~~~~~~~~~~~~~~                    "
                             "\0";
unsigned char* freeze_root_warn = (unsigned char *)
                             " NEED TO CHANGE CURRENT DIR TO ROOT TO  "
                             " START TOOL! THIS WILL BREAK DISK IMAGE "
                             " MOUNTS FROM SUBDIRS!    PROCEED (Y/N)? "
                             "\0";

// name of the file that is loaded by charset restore F14
#define DEFAULT_CHARSET "CHARSET.M65"
#define MAIN_ROM_FILE "MEGA65.ROM"

static unsigned short i;
unsigned char rom_changed = 0;
unsigned char not_in_root = 0;
#ifdef WITH_TOUCH
signed char swipe_dir = 0;
#endif

void topetsciiupper(char* str, int len);

unsigned char colour_table[256];

void make_colour_lookup(void)
{
  unsigned char c;

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
}

// clang-format off
unsigned char viciv_regs[0x80] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x9B, 0x37, 0x00, 0x00, 0x00, 0xC9, 0x00, 0x14, 0x71, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0E, 0x06, 0x01, 0x02, 0x03, 0x04, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x00,
  0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x68, 0x00, 0xF8, 0x01, 0x50, 0x00, 0x68, 0x00,
  0x0C, 0x83, 0x00, 0x81, 0x05, 0x00, 0x00, 0x00, 0x50, 0x00, 0x78, 0x01, 0x50, 0xC0, 0x28, 0x00,
  0x00, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x90, 0x00, 0x00, 0xF8, 0x07, 0x00, 0x00,
  0xFF, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x81, 0x18, 0xC2, 0x00, 0x00, 0x7F };
// clang-format on

void setup_menu_screen(void)
{
  // Reset all VIC-IV registers
  // EXCEPT preserve $D054 CRT emulation mode...
  viciv_regs[0x54] = (viciv_regs[0x54] & (0xff - 0x20)) | (PEEK(0xD054) & 0x20);
  // EXCEPT preserve PAL/NTSC
  viciv_regs[0x6F] = PEEK(0xD06F);
  // fix position for PAL/NTSC
  if (viciv_regs[0x6f] & 0x80) {
    viciv_regs[0x48] = 0x2A;
    viciv_regs[0x4A] = 0xB9;
    viciv_regs[0x4E] = 0x2A;
    viciv_regs[0x72] = 0x18; // SPRYADJ
  }
  else {
    viciv_regs[0x48] = 0x68;
    viciv_regs[0x4A] = 0xF8;
    viciv_regs[0x4E] = 0x68;
    viciv_regs[0x72] = 0x00; // SPRYADJ
  }

  lcopy((long)viciv_regs, 0xffd3000L, 47);
  // don't write D02f, or we switch back to vic-ii
  lcopy((long)viciv_regs + 48, 0xffd3030L, 80);

  // Reset border widths
  // No sprites
  // Move screen to SCREEN_ADDRESS
  // 16-bit text mode with full colour for chars >$FF
  // (which we will use for showing the thumbnail)
  // 80 bytes per row
  // Screen at $00B800
  // Colour RAM at offset $0000

  // Fill colour RAM with sensible value at the start
  lfill(0xff80000U, 1, 2000);
}

unsigned char next_cpu_speed(void)
{
  switch (detect_cpu_speed()) {
  case 1:
    // Make it 2MHz: 2MHZ && !FAST && !VFAST
    // ffd0030 is a special register to access the C128 D030.0 bit
    freeze_poke(0xffd0030L, 1);
    freeze_poke(0xffd3031L, freeze_peek(0xffd3031L) & 0xbf);
    freeze_poke(0xffd3054L, freeze_peek(0xffd3054L) & 0xbf);
    return 1;
  case 2:
    // Make it 3.5MHz: !2MHZ && FAST && !VFAST
    freeze_poke(0xffd0030L, 0);
    freeze_poke(0xffd3031L, freeze_peek(0xffd3031L) | 0x40);
    freeze_poke(0xffd3054L, freeze_peek(0xffd3054L) & 0xbf);
    // freeze_poke(0xffd367dL, freeze_peek(0xffd367dL) & 0xef);
    break;
  case 3:
    // Make it 40MHz: !2MHZ && FAST && VFAST
    freeze_poke(0xffd0030L, 0);
    freeze_poke(0xffd3031L, freeze_peek(0xffd3031L) | 0x40);
    freeze_poke(0xffd3054L, freeze_peek(0xffd3054L) | 0x40);
    // freeze_poke(0xffd367dL, freeze_peek(0xffd367dL) | 0x10);
    break;
  case 40:
  default:
    // Make it 1MHz: !2MHZ && !FAST && !VFAST
    freeze_poke(0xffd0030L, 0);
    freeze_poke(0xffd3031L, freeze_peek(0xffd3031L) & 0xbf);
    freeze_poke(0xffd3054L, freeze_peek(0xffd3054L) & 0xbf);
    // If a program forced 40 MHz via POKE 0,65, the Hypervisor flag at
    // ffd367d is set, and is overriding the other flags. Clear it.
    // The Freezer itself does not set this.
    freeze_poke(0xffd367dL, freeze_peek(0xffd367dL) & 0xef);
    return 1;
  }
  return 0;
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
  unsigned short yoffset, yoffset_out, xoffset, j;
  uint32_t thumbnail_sector = find_thumbnail_offset();

  // Can't find thumbnail area?  Then show no thumbnail
  if (thumbnail_sector == 0xFFFFFFFFUL) {
    lfill(0x50000L, 0, 10 * 6 * 64);
    return;
  }
  // Copy thumbnail memory to buffer
  for (i = 0; i < 8; i++) {
    sdcard_readsector(freeze_slot_start_sector + thumbnail_sector + i);
    lcopy((long)sector_buffer, (long)thumbnail_buffer + (i * 0x200), 0x200);
    NAVIGATION_KEY_CHECK();
  }

  // Pick colours of all pixels in the thumbnail
  for (j = 0; j < 4096; j++)
    thumbnail_buffer[j] = colour_table[thumbnail_buffer[j]];
  // Fix column 0 of pixels
  yoffset = 0;
  for (j = 0; j < 49; j++) {
    thumbnail_buffer[yoffset] = thumbnail_buffer[yoffset + 1];
    yoffset += 80;
  }

  // Rearrange pixels
  yoffset = 80 + 13; // skip dud first line
  for (y = 0; y < 48; y++) {
    yoffset_out = ((y & 7) << 3) + (y >> 3) * 64;
    xoffset = 0;
    for (x = 0; x < 73; x += 8) {
      // Also the whole thing is rotated by one byte, so add that on as we plot the pixel
      // PGS Optimise here

      j = 8;
      if (x == 72)
        j = 2;

      lcopy((unsigned long)&thumbnail_buffer[x + yoffset], 0x50000L + xoffset + yoffset_out, j);

      xoffset += 64 * 6;
    }
    NAVIGATION_KEY_CHECK();
    yoffset += 80;
  }
}

struct process_descriptor_t process_descriptor;

// clang-format off
char last_thumb_frame = -1;
unsigned char thumb_xoff = 5, thumb_yoff = 1;
unsigned short tile_offset;
#define F_M65 0
#define F_C65 1
#define F_C64 2
char thumb_frame_name[][13] = {
  "M65THUMB.M65",
  "C65THUMB.M65",
  "C64THUMB.M65"
};

void predraw_freeze_menu(void)
{
  // Clear screen, blue background, white text, like Action Replay
  POKE(0xD020U, 6);
  POKE(0xD021U, 6);

  lfill(0xFF80000L, 1, 2000);
  // Make disk image names different colour to avoid confusion
  for (i = 40; i < 80; i += 2) {
    lpoke(0xff80000 + 21 * 80 + 1 + i, 0xe);
    lpoke(0xff80000 + 24 * 80 + 1 + i, 0xe);
    if (i > 50) // ROM VERSION
      lpoke(0xff80000 + 15 * 80 + 1 + i, 0xf);
  }

  // Clear 16-bit text mode screen using DMA copy to copy the
  // manually cleared first couple of chars (we need two, because
  // of the pipelining in the DMA engine).
  lpoke(SCREEN_ADDRESS, 0x20);
  lpoke(SCREEN_ADDRESS + 1, 0x00);
  lpoke(SCREEN_ADDRESS + 2, 0x20);
  lpoke(SCREEN_ADDRESS + 3, 0x00);
  lcopy(SCREEN_ADDRESS, SCREEN_ADDRESS + 4, 2000 - 4);

  last_thumb_frame = -1;
}

#define UPDATE_ALL     0x7f
#define UPDATE_UPPER   0x0f
#define UPDATE_TOP     0x01
#define UPDATE_ROM     0x02
#define UPDATE_FREQ    0x04
#define UPDATE_LOWER   0x70
#define UPDATE_PROCESS 0x10
#define UPDATE_DISK    0x20
#define UPDATE_THUMB   0x40
#define UPDATE_CHGSLOT 0x80
// clang-format on

void copy_convert_to_screen(unsigned char *data, short offset)
{
  offset <<= 1;

  for (i = 0; data[i]; i++)
    if (data[i] != '~') { // skip thumb area
      if ((data[i] >= 'A') && (data[i] <= 'Z'))
        POKE(SCREEN_ADDRESS + i * 2 + 0 + offset, data[i] - 0x40);
      else if ((data[i] >= 'a') && (data[i] <= 'z'))
        POKE(SCREEN_ADDRESS + i * 2 + 0 + offset, data[i] - 0x20);
      else
        POKE(SCREEN_ADDRESS + i * 2 + 0 + offset, data[i]);
      POKE(SCREEN_ADDRESS + i * 2 + 1 + offset, 0);
    }
}

void draw_freeze_menu(unsigned char part)
{
  unsigned char x, y;

  if (part & UPDATE_CHGSLOT) {
    find_freeze_slot_start_sector(slot_number);
    freeze_slot_start_sector = *(uint32_t*)0xD681U;
    request_freeze_region_list();
  }

  // Update messages based on the settings we allow to be easily changed
  if (part & UPDATE_TOP) {

    if (slot_number) {
      lcopy((unsigned long)freeze_menu_bar + 40, (unsigned long)&freeze_menu[LOAD_RESUME_OFFSET], 40);
      lcopy((unsigned long)" FREEZE SLOT:      ", (unsigned long)&freeze_menu[FREEZE_SLOT_OFFSET], 19);
      // Display slot ID as decimal
      screen_decimal((unsigned long)&freeze_menu[SLOT_NUMBER_OFFSET], slot_number);
    }
    else {
      lcopy((unsigned long)freeze_menu_bar, (unsigned long)&freeze_menu[LOAD_RESUME_OFFSET], 40);
      if (rom_changed)
        lfill((unsigned long)&freeze_menu[LOAD_RESUME_OFFSET], ' ', 9);

      // Display "- PAUSED STATE -"
      lcopy((unsigned long)" - PAUSED STATE -   ", (unsigned long)&freeze_menu[FREEZE_SLOT_OFFSET], 19);
    }

    // CPU MODE
    if (freeze_peek(0xffd367dL) & 0x20)
      lcopy((unsigned long)"  4502", (unsigned long)&freeze_menu[CPU_MODE_OFFSET], 6);
    else
      lcopy((unsigned long)"  AUTO", (unsigned long)&freeze_menu[CPU_MODE_OFFSET], 6);

    // Joystick 1/2 swap
    lcopy((unsigned long)((PEEK(0xd612L) & 0x20) ? "YES" : " NO"), (unsigned long)&freeze_menu[JOY_SWAP_OFFSET], 3);

    // Cartridge enable
    lcopy((unsigned long)((freeze_peek(0xffd367dL) & 0x01) ? "YES" : " NO"), (unsigned long)&freeze_menu[CART_ENABLE_OFFSET],
        3);

    if (freeze_peek(0xFFD3054L) & 0x20) // PALEMU
      lcopy((unsigned long)" ON", (unsigned long)&freeze_menu[CRTEMU_MODE_OFFSET], 3);
    else // PAL50
      lcopy((unsigned long)"OFF", (unsigned long)&freeze_menu[CRTEMU_MODE_OFFSET], 3);

    if (freeze_peek(0xffd306fL) & 0x80) // NTSC60
      lcopy((unsigned long)"NTSC60", (unsigned long)&freeze_menu[VIDEO_MODE_OFFSET], 6);
    else // PAL50
      lcopy((unsigned long)" PAL50", (unsigned long)&freeze_menu[VIDEO_MODE_OFFSET], 6);
  }

  // ROM version
  /*
  if (part & UPDATE_ROM)
    lcopy((long)detect_rom(), (unsigned long)&freeze_menu[ROM_NAME_OFFSET], 11);
  */

  // CPU frequency
  if (part & UPDATE_FREQ)
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

  if ((part & UPDATE_PROCESS) || (part & UPDATE_THUMB))
    detect_rom();

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
  if ((part & UPDATE_PROCESS) || (part & UPDATE_DISK)) {
    lfill((long)&process_descriptor, 0, sizeof(process_descriptor));
    freeze_fetch_sector(0xFFFBD00L, (unsigned char*)&process_descriptor);
  }

  if (part & UPDATE_PROCESS) {
    // Display process ID as decimal
    screen_decimal((unsigned long)&freeze_menu[PROCESS_ID_OFFSET], process_descriptor.task_id);

    // Process name: only display if no unprintable PETSCII chars
    for (i = 0; i < 16; i++)
      if ((process_descriptor.process_name[i] & 0x7f) < 0x20)
        break;
    if (i == 16)
      lcopy((unsigned long)process_descriptor.process_name, (unsigned long)&freeze_menu[PROCESS_NAME_OFFSET], 16);
    else
      lcopy((unsigned long)"UNNAMED TASK    ", (unsigned long)&freeze_menu[PROCESS_NAME_OFFSET], 16);

    lcopy((unsigned long)mega65_rom_name, (unsigned long)&freeze_menu[PROCESS_ROM_OFFSET], 11);
  }

  if (part & UPDATE_DISK) {
    // Draw drive numbers for internal drive
    lfill((unsigned long)&freeze_menu[DRIVE0_NUM_OFFSET], 0, 2);
    lfill((unsigned long)&freeze_menu[DRIVE1_NUM_OFFSET], 0, 2);
    screen_decimal((unsigned long)&freeze_menu[DRIVE0_NUM_OFFSET], freeze_peek(0x10113L));
    screen_decimal((unsigned long)&freeze_menu[DRIVE1_NUM_OFFSET], freeze_peek(0x10114L));

    lfill((unsigned long)&freeze_menu[D81_IMAGE0_NAME_OFFSET], ' ', 18);
    lfill((unsigned long)&freeze_menu[D81_IMAGE1_NAME_OFFSET], ' ', 18);

    // Show name of current mounted disk image
    if (process_descriptor.d81_image0_namelen) {
      for (i = 0; i < process_descriptor.d81_image0_namelen; i++)
        if (!process_descriptor.d81_image0_name[i])
          break;
      if (i == process_descriptor.d81_image0_namelen) {
        topetsciiupper(process_descriptor.d81_image0_name, process_descriptor.d81_image0_namelen);
        lcopy((unsigned long)process_descriptor.d81_image0_name, (unsigned long)&freeze_menu[D81_IMAGE0_NAME_OFFSET],
            process_descriptor.d81_image0_namelen < 18 ? process_descriptor.d81_image0_namelen : 18);
      }
    }

    if (process_descriptor.d81_image1_namelen) {
      for (i = 0; i < process_descriptor.d81_image1_namelen; i++)
        if (!process_descriptor.d81_image1_name[i])
          break;
      if (i == process_descriptor.d81_image1_namelen) {
        topetsciiupper(process_descriptor.d81_image1_name, process_descriptor.d81_image1_namelen);
        lcopy((unsigned long)process_descriptor.d81_image1_name, (unsigned long)&freeze_menu[D81_IMAGE1_NAME_OFFSET],
            process_descriptor.d81_image1_namelen < 18 ? process_descriptor.d81_image1_namelen : 18);
      }
    }
  }

  // wait till raster leaves screen
  while (PEEK(0xD012U) < 0xf8)
    continue;

  // Freezer can't use printf() etc, because C64 ROM has not started, so ZP will be a mess
  // (in fact, most of memory contains what the frozen program had. Only our freezer program
  // itself has been loaded to replace some of RAM).
  copy_convert_to_screen(freeze_menu, 0);

  // Draw the thumbnail surround area
  if (part & UPDATE_THUMB) {
    int8_t thumb_frame = F_M65;

    switch (mega65_rom_type) {
      case MEGA65_ROM_C64:
        thumb_frame = F_C64;
        break;
      case MEGA65_ROM_C65:
        thumb_frame = F_C65;
        break;
      case MEGA65_ROM_M65:
        if (detect_cpu_speed() == 1)
          thumb_frame = F_C64;
        else
          thumb_frame = F_M65;
        break;
      case MEGA65_ROM_OPENROM:
      default:
        thumb_frame = F_M65;
        break;
    }

    // only load new image if needed
    if (!not_in_root && thumb_frame != last_thumb_frame) { // only load a frame if we are in the root
      while (thumb_frame > -1) {
        if (!read_file_from_sdcard(thumb_frame_name[thumb_frame], 0x052000L))
          break;
        // fall through to next lower thumb image
        thumb_frame--;
      }
    }

    // Work out where the tile data begins
    if (thumb_frame > -1 && thumb_frame != last_thumb_frame) {
      uint32_t screen_data_start;
      unsigned short* tile_num;

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
      thumb_xoff = lpeek(0x52020L);
      thumb_yoff = thumb_xoff >> 4;
      thumb_xoff &= 0xf;
      last_thumb_frame = thumb_frame;
    }
    else if (thumb_frame == -1) {
      thumb_xoff = 5;
      thumb_yoff = 2;
      last_thumb_frame = -1;
    }

    // Now draw the 10x6 character block for thumbnail display itself
    // This sits in the region below the menu where we will also have left and right arrows,
    // the program name etc, so you can easily browse through the freeze slots.
    draw_thumbnail();
    for (x = 0; x < 9; x++)
      for (y = 0; y < 6; y++) {
        POKE(SCREEN_ADDRESS + (80 * 13) + ((thumb_xoff + x) * 2) + ((thumb_yoff + y) * 80) + 0, x * 6 + y); // $50000 base address
        POKE(SCREEN_ADDRESS + (80 * 13) + ((thumb_xoff + x) * 2) + ((thumb_yoff + y) * 80) + 1, 0x14);      // $50000 base address
      }
  }

  // restore border colour (fdisk/sd stuff still twiddles with it)
  POKE(0xD020U, 6);
}

// NOTE: I wanted to tweak the string to look nicer, but this gave me dos driver errors once back in BASIC (doing a DIR)
char tweak(char c)
{
  if (c < 0x60 || c >= 0x7a)
    return c;
  return c & 0x5f;
}

void topetsciiupper(char* str, int len)
{
  int i;
  for (i = 0; i < len; i++)
    str[i] = tweak(str[i]);
}

// Left/right do left/right
// fire = F3
// down = disk menu
// up = toggle PAL/NTSC ?
unsigned char joy_to_key[32] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF3,       // With fire pressed
  0, 0, 0, 0, 0, 0, 0, 0x1d, 0, 0, 0, 0x9d, 0, 'd', 'v', 0 // without fire
};

unsigned char origD689 = 0;

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

void store_selected_disk_image(int diskid, char* disk_image)
{
  int disk_img_name_loc = diskid ? 0x35 : 0x15;
  int disk_img_name_length_loc = diskid ? 0x14 : 0x13;
  unsigned char i;

  // Replace disk image name in process descriptor block
  for (i = 0; (i < 32) && disk_image[i]; i++)
    freeze_poke(0xFFFBD00L + disk_img_name_loc + i, tweak(disk_image[i]));
  // Update length of name
  freeze_poke(0xFFFBD00L + disk_img_name_length_loc, i);
  // Pad with spaces as required by hypervisor
  for (; i < 32; i++)
    freeze_poke(0xFFFBD00L + disk_img_name_loc + i, ' ');
}

void select_mounted_disk_image(int diskid)
{
  char* disk_image = freeze_select_disk_image(diskid);

  // Restore freeze region offset list to $0400 screen
  request_freeze_region_list();

  if ((unsigned short)disk_image == 0xFFFF) {
    // Have no disk image
  }
  else if (disk_image) {
    POKE(0xD020U, 6);
    store_selected_disk_image(diskid, disk_image);
  }

  predraw_freeze_menu();
  draw_freeze_menu(UPDATE_ALL);
}

#if 0
void debug_region_list()
{
  // display region list on screen
  unsigned long test;
  unsigned short j;
  for (j = 0; j < freeze_region_count; j++) {
    test = freeze_region_list[j].address_base;
    POKE(SCREEN_ADDRESS + j*80 + 60 - 16, 32);
    for (i = 0; i < 8; i++) {
      POKE(SCREEN_ADDRESS + j*80 + 60 - i*2, nybl_to_screen((uint8_t)test));
      test >>= 4;
    }
    POKE(SCREEN_ADDRESS + j*80 + 78 - 16, 32);
    test = freeze_region_list[j].region_length;
    for (i = 0; i < 8; i++) {
      POKE(SCREEN_ADDRESS + j*80 + 78 - i*2, nybl_to_screen((uint8_t)test));
      test >>= 4;
    }
  }
  test = address_to_freeze_slot_offset(CHARGEN_ADDRESS);
  for (i = 0; i < 8; i++) {
    POKE(SCREEN_ADDRESS + j*80 + 78 - i*2, nybl_to_screen((uint8_t)test));
    test >>= 4;
  }

  // wait for a key
  while (PEEK(0xD610U))
    POKE(0xD610U, 0);
  while (!PEEK(0xD610U))
    usleep(1000);
  POKE(0xD610U, 0);
  POKE(0xD020U, 6);
}
#endif

#define CHARGEN_FIXMEM  0x01 // write char data to chargen memory
#define CHARGEN_FIXSLOT 0x02 // write char data to slot storage
#define CHARGEN_FORCE   0x40 // if check can't load region, do fix anyway
#define CHARGEN_NOCHECK 0x80 // don't execute check, always fix
void fix_chargen_area(unsigned char flags)
{
  unsigned short i = 512; // needs to be 512 for nocheck to trigger!
  long charset_start;

  // debug_region_list();

  if (!(flags & CHARGEN_NOCHECK)) {
    if (!freeze_fetch_sector(CHARGEN_ADDRESS, NULL))
      // check if everything is zero
      for (i = 0; i < 512 && !sector_buffer[i]; i++);
    else
      // error while reading sector (old core?)
      i = (flags & CHARGEN_FORCE) ? 512 : 0;
  }

  // if first chargen sector was zero...
  if (i == 512) {
    charset_start = -1;
    // try to load DEFAULT_CHARSET or MEGA65.ROM
    if (!read_file_from_sdcard(DEFAULT_CHARSET, 0x40000L))
      charset_start = 0x40000L;
    else if (!read_file_from_sdcard(MAIN_ROM_FILE, 0x40000L))
      charset_start = 0x4D000L;

    if (charset_start != -1) {
      // copy the font to chargen WOM directly
      if (flags & CHARGEN_FIXMEM)
        lcopy(charset_start, CHARGEN_ADDRESS, 4096);

      // should we also fix the slot?
      if (flags & CHARGEN_FIXSLOT)
        for (i = 0; i < 8; i++) {
          lcopy(charset_start + 512L*i, (long)sector_buffer, 512);
          freeze_store_sector(CHARGEN_ADDRESS + 512L*i, NULL);
        }
    }
    else {
      // failed to load font, flash screen
      POKE(0xD020U, 2);
      POKE(0xD021U, 2);
      usleep(150000L);
      POKE(0xD020U, 6);
      POKE(0xD021U, 6);
    }
  }
}

void start_freezer_tool(char *toolfile)
{
  char x = 0, start_tool = 0;

  if (not_in_root) {
    copy_convert_to_screen(freeze_root_warn, TOOLS_MENU_OFFSET);

    while (!start_tool) {
      while (!(x = PEEK(0xD610U)));
      POKE(0xD610U, 0);
      switch (x) {
        case 'y':
        case 'Y':
          mega65_dos_cdroot();
          start_tool = 1;
          break;
        case 'n':
        case 'N':
        case 0x1b:
        case 0x03:
          draw_freeze_menu(UPDATE_TOP);
          return;
      }
    }
  }
  mega65_dos_exechelper(toolfile);
}

#ifdef __CC65__
void main(void)
#else
int main(int argc, char** argv)
#endif
{
  unsigned char drive_state;
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

  // check border for return codes from other helpers
  switch (PEEK(0xD020U)) {
    case 0x83:
      rom_changed = 1;
      break;
  }

  // Bank out BASIC ROM, leave KERNAL and IO in
  POKE(0x00, 0x3F);
  POKE(0x01, 0x36);

  // Disable Cartridge ROM
  lpoke(0xFFD37FDL, lpeek(0xFFD37FDL) | 0xC0); //Ensure forced exrom & game are high=disabled
  lpoke(0xFFD37FBL, lpeek(0xFFD37FBL) & 0xFD); //Disable cartridge (core will use forced values above for exrom/game)

  // No decimal mode!
  __asm__("cld");

  // Put $DD00 DDR back to default
  POKE(0xDD02, 0xFF);

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
  POKE(0xD458U, 0);
  POKE(0xD478U, 0);

  set_palette();
  make_colour_lookup();

  // assure we're viewing the sdcard's sector buffer (and not the floppy disk buffer)
  origD689 = PEEK(0xD689);
  POKE(0xD689, PEEK(0xD689) | 128);

  // Now find the start sector of the slot, and make a copy for safe keeping
  slot_number = 0;
  find_freeze_slot_start_sector(slot_number);
  freeze_slot_start_sector = *(uint32_t*)0xD681U;

  // SD or SDHC card?
  if (PEEK(0xD680U) & 0x10)
    sdhc_card = 1;
  else
    sdhc_card = 0;

  request_freeze_region_list();

  // BASIC65 unmount will just poke D6A1, and
  // not use hyppo, because we don't have a fucntion
  // for that! so we need to udpate the process
  // descriptor to show that we have the internal
  // drive mounted
  drive_state = lpeek(0xFFD36A1);
  if (drive_state & 0x1)
    store_selected_disk_image(0, INTERNAL_DRIVE_0);

  setup_menu_screen();
  predraw_freeze_menu();
  //chargen fix needs happen before the thumbnail frame is loaded as it clobbers
  //the thumbnail frame data.
  fix_chargen_area(CHARGEN_FIXMEM | CHARGEN_NOCHECK);
  draw_freeze_menu(UPDATE_ALL);


  // Flush input buffer
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
        case 0x13: // Home
          if (slot_number) {
            slot_number = 0;

            draw_freeze_menu(UPDATE_TOP | UPDATE_PROCESS | UPDATE_THUMB | UPDATE_CHGSLOT);
          }
          break;
        case ',':
          slot_number -= 90;
        case 0x11: // Cursor down
          slot_number -= 9;
        case 0x9D: // Cursor left
          slot_number--;
          if (slot_number >= get_freeze_slot_count()) // unsigned!
            slot_number = get_freeze_slot_count() - 1;

          draw_freeze_menu(UPDATE_TOP | UPDATE_PROCESS | UPDATE_THUMB | UPDATE_CHGSLOT);
          break;
        case '.':
          slot_number += 90;
        case 0x91: // Cursor up
          slot_number += 9;
        case 0x1D: // Cursor right
          slot_number++;
          if (slot_number >= get_freeze_slot_count())
            slot_number = 0;

          draw_freeze_menu(UPDATE_TOP | UPDATE_PROCESS | UPDATE_THUMB | UPDATE_CHGSLOT);
          break;

        case 'M':
        case 'm': // Monitor
          start_freezer_tool("MONITOR.M65");
          break;

        case 'A':
        case 'a': // Audio mixer
          start_freezer_tool("AUDIOMIX.M65");
          break;

        case 'S':
        case 's': // Sprite Editor
          start_freezer_tool("SPRITED.M65");
          break;

        case 'J':
        case 'j': // Toggle joystick swap
          POKE(0xD612L, (PEEK(0xD612L) ^ 0x20) & 0xEF);

          draw_freeze_menu(UPDATE_TOP);
          break;

        case 'T':
        case 't': // Toggle cartridge enable
          freeze_poke(0xFFD367dL, freeze_peek(0xFFD367dL) ^ 0x01);
          draw_freeze_menu(UPDATE_TOP);
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
          draw_freeze_menu(UPDATE_TOP);
          break;

        case 'F':
        case 'f': // Change CPU speed
          if (next_cpu_speed())
            draw_freeze_menu(UPDATE_FREQ | UPDATE_THUMB);
          else
            draw_freeze_menu(UPDATE_FREQ);
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
            freeze_poke(0xFFD3048L, 0x68);
            freeze_poke(0xFFD3049L, 0x0 | (freeze_peek(0xFFD3049L) & 0xf0));
            freeze_poke(0xFFD304AL, 0xF8);
            freeze_poke(0xFFD304BL, 0x1 | (freeze_peek(0xFFD304BL) & 0xf0));
            freeze_poke(0xFFD304EL, 0x68);
            freeze_poke(0xFFD304FL, 0x0 | (freeze_peek(0xFFD304FL) & 0xf0));
            freeze_poke(0xFFD3072L, 0);
            // CIA TOD
            freeze_poke(0xffd3c0el, freeze_peek(0xffd3c0el) | 0x80);
            freeze_poke(0xffd3d0el, freeze_peek(0xffd3d0el) | 0x80);
            // do it for the freezer itself
            lpoke(0xFFD306fL, 0x00);
            lpoke(0xFFD3072L, 0x00);
            lpoke(0xFFD3048L, 0x68);
            lpoke(0xFFD3049L, 0x0 | (lpeek(0xFFD3049L) & 0xf0));
            lpoke(0xFFD304AL, 0xF8);
            lpoke(0xFFD304BL, 0x1 | (lpeek(0xFFD304BL) & 0xf0));
            lpoke(0xFFD304EL, 0x68);
            lpoke(0xFFD304FL, 0x0 | (lpeek(0xFFD304FL) & 0xf0));
            lpoke(0xFFD3072L, 0);
            // CIA TOD
            lpoke(0xffd3c0el, lpeek(0xffd3c0el) | 0x80);
            lpoke(0xffd3d0el, lpeek(0xffd3d0el) | 0x80);
          }
          else {
            // Switch to NTSC
            freeze_poke(0xFFD306fL, 0x87);
            freeze_poke(0xFFD3072L, 0x18);
            freeze_poke(0xFFD3048L, 0x2A);
            freeze_poke(0xFFD3049L, 0x0 | (freeze_peek(0xFFD3049L) & 0xf0));
            freeze_poke(0xFFD304AL, 0xB9);
            freeze_poke(0xFFD304BL, 0x1 | (freeze_peek(0xFFD304BL) & 0xf0));
            freeze_poke(0xFFD304EL, 0x2A);
            freeze_poke(0xFFD304FL, 0x0 | (freeze_peek(0xFFD304FL) & 0xf0));
            freeze_poke(0xFFD3072L, 24);
            // CIA TOD
            freeze_poke(0xffd3c0el, freeze_peek(0xffd3c0el) & 0x7f);
            freeze_poke(0xffd3d0el, freeze_peek(0xffd3d0el) & 0x7f);
            // do it for the freezer itself
            lpoke(0xFFD306fL, 0x87);
            lpoke(0xFFD3072L, 0x18);
            lpoke(0xFFD3048L, 0x2A);
            lpoke(0xFFD3049L, 0x0 | (lpeek(0xFFD3049L) & 0xf0));
            lpoke(0xFFD304AL, 0xB9);
            lpoke(0xFFD304BL, 0x1 | (lpeek(0xFFD304BL) & 0xf0));
            lpoke(0xFFD304EL, 0x2A);
            lpoke(0xFFD304FL, 0x0 | (lpeek(0xFFD304FL) & 0xf0));
            lpoke(0xFFD3072L, 24);
            // CIA TOD
            lpoke(0xffd3c0el, lpeek(0xffd3c0el) & 0x7f);
            lpoke(0xffd3d0el, lpeek(0xffd3d0el) & 0x7f);
          }
          draw_freeze_menu(UPDATE_TOP);
          break;

        case '8':
        case '9':
          // Change drive number of internal drives
          freeze_poke(0x10113L - '8' + c, freeze_peek(0x10113L - '8' + c) ^ 2);
          draw_freeze_menu(UPDATE_DISK);
          break;
        case '0': // Select mounted disk image
          select_mounted_disk_image(0);
          break;
        case '1': // Select mounted disk image for 2nd drive
          select_mounted_disk_image(1);
          break;

        case 0xf5: // F5 = Reset
          // reset only works for slot 0!
          if (slot_number != 0)
            goto invalid_function;
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
          // Turn off extended graphics mode, only keep palemu
          freeze_poke(0xFFD3054U, freeze_peek(0xFFD3054U) & 0x20);
          // fall through
        case 0xf3: // F3 = resume
        case 0xf4: // RESUME even if ROM changed
          // if rom changed, slot 0 resume is disabled, reset is required
          if (c == 0xf3 && slot_number == 0 && rom_changed)
            goto invalid_function;
          // Doesn't seem to really help (probably needs to be done by the hypervisor unfreezing routine?)
          POKE(0xD689, origD689);

          // workaround for old freeze slots that have an empty chargen area
          fix_chargen_area(CHARGEN_FIXMEM | CHARGEN_FIXSLOT);

          unfreeze_slot(slot_number);

          // should never get here
          screen_of_death("unfreeze failed");

          break;

        case 0xf7: // F7 = save to slot
        {
          uint32_t i;
          uint32_t j;
          uint32_t dest_freeze_slot_start_sector;

          // can't save to slot 0
          if (slot_number == 0) {
            POKE(0xD020U, 2);
            POKE(0xD021U, 2);
            usleep(150000L);
            POKE(0xD020U, 6);
            POKE(0xD021U, 6);
            continue;
          }

          // Get start sectors of the source and destination slots
          // give visual feedback
          sdcard_visual_feedback(1);

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
          // stop giving visual feedback
          sdcard_visual_feedback(0);

          POKE(0xD020U, 6);

          draw_freeze_menu(UPDATE_TOP | UPDATE_PROCESS | UPDATE_THUMB);
        } break;

        case 0xfe: // F14 - restore CHARSET from FILE
          {
            // clear screen first
            predraw_freeze_menu();
            // don't check, just put font into chargen
            fix_chargen_area(CHARGEN_NOCHECK | CHARGEN_FIXMEM);
            // we need to redraw everything, because loading the ROM
            // will mess things up (thumbnail for example)
            last_thumb_frame = 255; // invalidate thumbnail
            draw_freeze_menu(UPDATE_ALL);
          }
          break;

        case 0x1f: // HELP MEGAINFO
          start_freezer_tool("MEGAINFO.M65");
          break;

        case 'R':
        case 'r': // switch CRT Emulation
          c = freeze_peek(0xFFD3054L);
          if (c & 0x20) {
            freeze_poke(0xFFD3054L, c & 0xdf);
            lpoke(0xFFD3054L, lpeek(0xFFD3054L) & 0xdf);
          }
          else {
            freeze_poke(0xFFD3054L, c | 0x20);
            lpoke(0xFFD3054L, lpeek(0xFFD3054L) | 0x20);
          }
          draw_freeze_menu(UPDATE_TOP);
          break;
        case 'L':
        case 'l':
          start_freezer_tool("ROMLOAD.M65");
          break;
        case 'X':
        case 'x': // Poke finder
        case 'E':
        case 'e': // Enter POKEs
        case 'k':
        case 'K': // Sprite killer
        default:
invalid_function:
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
