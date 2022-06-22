#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freezer.h"
#include "infohelper.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

/*
 * Constants
 */
static char SDessentials[][13] = {
  "FREEZER.M65",
  "MEGAINFO.M65",
  "ONBOARD.M65",
  "MONITOR.M65",
  "MAKEDISK.M65",
  "ROMLOAD.M65",
  "AUDIOMIX.M65",
  "SPRITED.M65",
  ""
};

/*
 * Global Variables
 */
#define BUFFER_LENGTH 254
#define BUFFER_COLOUR 255
static char buffer[BUFFER_LENGTH+2], tempstr32[32], isNTSC = 0, hasRTC = 0, hasExtRTC = 0;
static unsigned char version_buffer[256];

/*
 * write_text(x, y, colour, text)
 *
 *   x, y: screen position
 *   colour: colour top write in
 *   text: zero terminated string
 *
 * writes text to the screen using colour. converts to screencode (upper)
 */
void write_text(unsigned char x, unsigned char y, unsigned char colour, char *text) {
  unsigned char i, c;
  for (i = 0; text[i]; i++) {
    c = text[i];
    if ((c >= 'A') && (c <= 'Z'))
      c -= 0x40;
    else if ((c >= 'a') && (c <= 'z'))
      c -= 0x20;
    lpoke(SCREEN_ADDRESS + y * 80 + x + i, c);
    lpoke(COLOUR_RAM_ADDRESS + y * 80 + x + i, colour);
  }
}

/*
 * copy_hw_version()
 *
 *   globals: version_buffer
 *
 * extracts hardware and FPGA version from $FFD3629 by
 * copying 32 bytes to global version_buffer, to make access
 * faster (lpeek is a dma_copy!)
 */
void copy_hw_version() {
  lcopy(0xFFD3629L, (long)version_buffer, 32);
}

/*
 * ge_mega_model() -> char *
 *
 *   returns: string (tempstr32 or static)
 *   globals: buffer
 *
 * translate version_buffer[0] to model string
 *
 * get_hw_version must have been called to fill version_buffer
 */
char *format_mega_model() {
  switch (version_buffer[0]) {
    case 1:
      return "MEGA65 R1";
    case 2:
      hasRTC = 1;
      return "MEGA65 R2";
    case 3:
      hasRTC = 1;
      return "MEGA65 R3";
    case 33:
      return "MEGAPHONE R1 PROTOTYPE";
    case 64:
      return "NEXYS 4 PSRAM";
    case 65:
      return "NEXYS 4 DDR (NO WIDGET)";
    case 66:
      return "NEXYS 4 DDR (WIDGET)";
    case 253:
      return "QMTECH WUKONG BOARD";
    case 254:
      return "SIMULATED MEGA65";
    case 255:
      return "HARDWARE NOT SPECIFIED";
    default:
      snprintf(tempstr32, 31, "UNKNOWN MODEL $%02X", version_buffer[0]);
      return tempstr32;
  }
}

/*
 * format_datestamp(offset, msbmask) -> char *
 *
 *   offset: offset to the start of the FPGA fields (1-KEYBD, 7-ARTIX, 13-MAX10)
 *   msbmask: mask msb byte (neeeded for MAX10, 0x3f)
 *
 *   returns: string (buffer)
 *   globals: tempstr32, buffer
 *
 * formats a FPGA datestamp (days since 2020-01-01, years full 366 days)
 * to a string YYYY-MM-DD.
 *
 * get_hw_version must have been called to fill version_buffer
 */
char *format_datestamp(unsigned char offset, unsigned char msbmask) {
  unsigned char m=1;
  unsigned short y=2020, ds;

  ds = (((unsigned short)(version_buffer[offset+1]&msbmask)) << 8) + (unsigned short)version_buffer[offset];

  // first remove years. years are always full 366 days!
  while (ds>366) {
    y++;
    ds-=366;
  }

  // then find out month and day, years divideable by 4 are jump years (no 100 or 400 in sight!)
  if (m==1 && ds>31) { m++; ds -= 31; }
  if (m==2 && !(y&3) && ds>29) { m++; ds-=29; }
  if (m==2 && (y&3) && ds>28) { m++; ds-=28; }
  if (m==3 && ds>31) { m++; ds-=31;}
  if (m==4 && ds>30) { m++; ds-=30;}
  if (m==5 && ds>31) { m++; ds-=31;}
  if (m==6 && ds>30) { m++; ds-=30;}
  if (m==7 && ds>31) { m++; ds-=31;}
  if (m==8 && ds>30) { m++; ds-=30;}
  if (m==9 && ds>31) { m++; ds-=31;}
  if (m==10 && ds>30) { m++; ds-=30;}
  if (m==11 && ds>31) { m++; ds-=31;}

  // snprintf can't do %d!
  itoa(y, tempstr32, 10); strcpy(buffer, tempstr32);
  if (m>9)
    strcat(buffer, "-");
  else
    strcat(buffer, "-0");
  itoa(m, tempstr32, 10); strcat(buffer, tempstr32);
  if (ds>9)
    strcat(buffer, "-");
  else
    strcat(buffer, "-0");
  itoa(ds, tempstr32, 10); strcat(buffer, tempstr32);

  return buffer;
}

/*
 * format_fpha_hash(offset, reverse) -> char *
 *
 *   offset: offset to the start of the FPGA fields (1-KEYBD, 7-ARTIX, 13-MAX10)
 *   msbmask: reverse byte order
 *
 *   returns: string (buffer)
 *   globals: buffer
 *
 * formats a FPGA commit hash to a 8 character hex number
 *
 * get_hw_version must have been called to fill version_buffer
 */
char *format_fpga_hash(unsigned char offset, unsigned char reverse) {
  if (reverse)
    sprintf(buffer, "%02X%02X%02X%02X", version_buffer[offset+2], version_buffer[offset+3], version_buffer[offset+4], version_buffer[offset+5]);
  else
    sprintf(buffer, "%02X%02X%02X%02X", version_buffer[offset+5], version_buffer[offset+4], version_buffer[offset+3], version_buffer[offset+2]);

  return buffer;
}

/*
 * format_rom_version() -> char *
 *
 *   returns: string (buffer or static)
 *   globals: buffer
 *
 * formats the ROM version. Tries to detect C64 ROMS.
 */
char *format_rom_version(void) {
  // Check for C65 ROM via version string
  lcopy(0x20016L, (long)buffer+4, 7);
  if ((buffer[4] == 'V') && (buffer[5] == '9')) {
    if (buffer[6] >= '2')
      buffer[0] = 'M';
    else
      buffer[0] = 'C';
    buffer[1] = '6';
    buffer[2] = '5';
    buffer[3] = ' ';
    buffer[11] = 0;
    return buffer;
  }

  // OpenROM - 16 characters "OYYMMDDCC       "
  lcopy(0x20010L, (long)buffer+4, 16);
  if ((buffer[4] == 'O') && (buffer[11] == '2') && (buffer[12] == '0') && (buffer[13] == ' ')) {
    buffer[0] = 'O';
    buffer[1] = 'P';
    buffer[2] = 'E';
    buffer[3] = 'N';
    buffer[4] = ' ';
    buffer[11] = 0;
    return buffer;
  }

  if (freeze_peek(0x2e47dL) == 'J') {
    // Probably jiffy dos
    if (freeze_peek(0x2e535L) == 0x06)
      return "SX64 JIFFY";
    else
      return "C64 JIFFY";
  }

  if (freeze_peek(0x2e449L) == 0x2e) {
    return "C64GS";
  }
  if (freeze_peek(0x2e119L) == 0xc9) {
    return "C64 REV1";
  }
  if (freeze_peek(0x2e67dL) == 0xb0) {
    return "C64 REV2 JP";
  }
  if (freeze_peek(0x2ebaeL) == 0x5b) {
    return "C64 REV3 DK";
  }
  if (freeze_peek(0x2e0efL) == 0x28) {
    return "C64 SCAND";
  }
  if (freeze_peek(0x2ebf3L) == 0x40) {
    return "C64 SWEDEN";
  }
  if (freeze_peek(0x2e461L) == 0x20) {
    return "CYCLONE 1.0";
  }
  if (freeze_peek(0x2e4a4L) == 0x41) {
    return "DOLPHIN 1.0";
  }
  if (freeze_peek(0x2e47fL) == 0x52) {
    return "DOLPHIN 2AU";
  }
  if (freeze_peek(0x2eed7L) == 0x2c) {
    return "DOLPHIN 2P1";
  }
  if (freeze_peek(0x2e7d2L) == 0x6b) {
    return "DOLPHIN 2P2";
  }
  if (freeze_peek(0x2e4a6L) == 0x32) {
    return "DOLPHIN 2P3";
  }
  if (freeze_peek(0x2e0f9L) == 0xaa) {
    return "DOLPHIN 3.0";
  }
  if (freeze_peek(0x2e462L) == 0x45) {
    return "DOSROM V1.2";
  }
  if (freeze_peek(0x2e472L) == 0x20) {
    return "MERCRY3 PAL";
  }
  if (freeze_peek(0x2e16dL) == 0x84) {
    return "MERCRY NTSC";
  }
  if (freeze_peek(0x2e42dL) == 0x4c) {
    return "PET 4064";
  }
  if (freeze_peek(0x2e1d9L) == 0xa6) {
    return "SX64 CROACH";
  }
  if (freeze_peek(0x2eba9L) == 0x2d) {
    return "SX64 SCAND";
  }
  if (freeze_peek(0x2e476L) == 0x2a) {
    return "TRBOACS 2.6";
  }
  if (freeze_peek(0x2e535L) == 0x07) {
    return "TRBOACS 3P1";
  }
  if (freeze_peek(0x2e176L) == 0x8d) {
    return "TRBOASC 3P2";
  }
  if (freeze_peek(0x2e42aL) == 0x72) {
    return "TRBOPROC US";
  }
  if (freeze_peek(0x2e4acL) == 0x81) {
    return "C64C 251913";
  }
  if (freeze_peek(0x2e479L) == 0x2a) {
    return "C64 REV2";
  }
  if (freeze_peek(0x2e535L) == 0x06) {
    return "SX64 REV4";
  }

  return "UNKNOWN ROM";
}

/*
 * format_hyppo_version -> char *
 *
 *   returns: string (buffer)
 *   globals: buffer, tempstr32
 *
 * fetches and formats hyppo and hdos version as a string '?.? / ?.?'
 */
char *format_hyppo_version(void) {
  unsigned char hyppo_version[4] = { 0xff, 0xff, 0xff, 0xff };

  // hypervisor call, external
  hyppo_getversion(hyppo_version);

  if (hyppo_version[0] == hyppo_version[1] &&
      hyppo_version[1] == hyppo_version[2] &&
      hyppo_version[2] == hyppo_version[3] &&
      hyppo_version[0] == 0xff)
    strcpy(buffer, "?.? / ?.?");
  else {
    itoa(hyppo_version[0], tempstr32, 10); strcpy(buffer, tempstr32); strcat(buffer, ".");
    itoa(hyppo_version[1], tempstr32, 10); strcat(buffer, tempstr32); strcat(buffer, " / ");
    itoa(hyppo_version[2], tempstr32, 10); strcat(buffer, tempstr32); strcat(buffer, ".");
    itoa(hyppo_version[3], tempstr32, 10); strcat(buffer, tempstr32);
  }

  return buffer;
}

/*
 * format_util_version(addr) -> char *
 *
 *   addr: start address in memory
 *
 *   returns: string (buffer)
 *   globals: buffer
 *
 * search the next 256 bytes from start address for a version string
 * starting with 'v:20' and returns the next characters until a zero byte
 */
char *format_util_version(long addr) {
  static unsigned char code_buf[512];
  unsigned short i, j=0;

  lcopy(addr, (long)code_buf, 512);

  strncpy(buffer, "UNKNOWN VERSION", 64);
  for (i=0; i<512; i++) {
    if (code_buf[i]==0x56 && code_buf[i+1]==0x3a && code_buf[i+2]==0x32 && code_buf[i+3]==0x30) {
      i += 4; // skip v:20
      while (j<64 && code_buf[i])
        buffer[j++] = code_buf[i++];
      buffer[j] = 0;
      break;
    }
  }

  // cuts on the left side, we always want the rightmost part with the commit hash
  i = strlen(buffer);
  if (i > 42)
    j = i-42;
  else
    j = 0;

  return buffer + 0;
}

/*
 * RTC Globals
 */
static unsigned char clock_init = 1, tod_buf[8] = { 0,0,0,0,0,0,0,0 };
static unsigned char rtc_check = 1, rtc_ticking = 0, rtc_diff = 0, rtc_buf[12] = { 0,0,0,0,0,0,0,0,0,0,0,0 };
static unsigned short tod_ov = 0, rtc_ov = 0, extrtc_ov = 0;
static short tod_last = -1, tod_ticks = 0, rtc_ticks = 0, extrtc_ticks = 0;
static unsigned char extrtc_check = 1, extrtc_ticking = 0, extrtc_diff = 0, extrtc_buf[14] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

/*
 * get_rtc_stats(reinit) -> uchar
 *
 *   reinit: if 1, restart everything
 *
 *   returns: 1 if ticked
 *   globals: rtc global vars
 *
 * makes one copy of tod and rtc data to a buffer (clock_init == 1)
 * makes only a second copy if first copy was made (clock_init == 0)
 * if tod ticks have changed, also updates all RTCs
 * returns 0 if tod seconds have not changed, otherwise 1
 */
unsigned char get_rtc_stats(unsigned char reinit) {
  short pa, pb;

  if (clock_init==1 || reinit==1) {
    // fix TOD frequency
    if (lpeek(0xffd306fl)&0x80) // is NTSC, clear 0Hz bit
      lpoke(0xffd3c0el, lpeek(0xffd3c0el)&0x7f);
    else // is PAL, set 50Hz bit
      lpoke(0xffd3c0el, lpeek(0xffd3c0el)|0x80);
    lcopy(0xffd7110l, (long)rtc_buf, 6);
    lcopy(0xffd3c08l, (long)tod_buf, 4);
    if (hasExtRTC)
      lcopy(0xffd7400l, (long)extrtc_buf, 7);
    clock_init = 0;
    tod_drift = 0;
    tod_ov = 0;
    rtc_ov = 0;
    extrtc_ov = 0;
  }
  lcopy(0xffd7110l, (long)rtc_buf+6, 6);
  lcopy(0xffd3c08l, (long)tod_buf+4, 4);
  if (hasExtRTC)
    lcopy(0xffd7400l, (long)extrtc_buf+7, 7);

  // only looking at seconds here, derived from minutes + seconds
  pa = ((tod_buf[1]>>4)&0x7)*10 + (tod_buf[1]&0xf) + (((tod_buf[2]>>4)&0x7)*10 + (tod_buf[2]&0xf))*60;
  pb = ((tod_buf[5]>>4)&0x7)*10 + (tod_buf[5]&0xf) + (((tod_buf[6]>>4)&0x7)*10 + (tod_buf[6]&0xf))*60;
  tod_ticks = pb-pa;
  if (tod_ticks<0) {
    tod_ticks += 3600; // we can work with one hour overflow
    tod_ov++;
    if (tod_ov>1) { // after that we reinitialise (who lets this run for hours?)
      clock_init = 1;
      tod_ticks = 0;
    }
  }
  
  if (tod_ticks == tod_last) return 0;
  // tod_ticks changed, update rtc

  tod_last = tod_ticks;

  // handle internal RTC
  if (hasRTC) {
    pa = ((rtc_buf[0]>>4)&0x7)*10 + (rtc_buf[0]&0xf) + (((rtc_buf[1]>>4)&0x7)*10 + (rtc_buf[1]&0xf))*60;
    pb = ((rtc_buf[6]>>4)&0x7)*10 + (rtc_buf[6]&0xf) + (((rtc_buf[7]>>4)&0x7)*10 + (rtc_buf[7]&0xf))*60;
    rtc_ticks = pb-pa;
    if (rtc_ticks<0) {
      rtc_ticks += 3600;   // we can work with one hour overflow
      rtc_ov++;
      if (rtc_ov>1) {
        clock_init = 1;
      }
    }

    if (rtc_ticks>tod_ticks)
      rtc_diff = rtc_ticks-tod_ticks;
    else
      rtc_diff = tod_ticks-rtc_ticks;
  }

  // handle External Grove RTC
  if (hasExtRTC == 2) {
    pa = ((extrtc_buf[0]>>4)&0x7)*10 + (extrtc_buf[0]&0xf) + (((extrtc_buf[1]>>4)&0x7)*10 + (extrtc_buf[1]&0xf))*60;
    pb = ((extrtc_buf[7]>>4)&0x7)*10 + (extrtc_buf[7]&0xf) + (((extrtc_buf[8]>>4)&0x7)*10 + (extrtc_buf[8]&0xf))*60;
    extrtc_ticks = pb-pa;
    if (extrtc_ticks<0) {
      extrtc_ticks += 3600;   // we can work with one hour overflow
      extrtc_ov++;
      if (extrtc_ov>1) {
        clock_init = 1;
      }
    }

    if (extrtc_ticks>tod_ticks)
      extrtc_diff = extrtc_ticks-tod_ticks;
    else
      extrtc_diff = tod_ticks-extrtc_ticks;
  }

  if (clock_init == 1) return 0;

  return 1;
}

/*
 * get_extrtc_status -> uchar
 *
 *   returns: external rtc status code
 *            0 = not installed
 *            1 = inactive
 *            2 = active
 *
 * checks if the external Grove RTC is present and if it is
 * active or inactive.
 */
unsigned char get_extrtc_status() {
  if (lpeek(0xffd7400) == 0xff) // not installed
    return 0;
  // where does this come from?
  if (lpeek(0xffd74fd) & 0x80) // active
    return 2;
  return 1; // inactive
}

/*
 * format_extrtc_status(status)
 *
 *   status: status code from get_extrtc_status
 *
 *   globals: buffer
 *
 * formats hasExtRTC as a string in buffer+1
 * sets buffer+0 to the colour code 0-15
 */
void format_extrtc_status(unsigned char status) {
  switch (status) {
    case 0:
      strcpy(buffer, "\12NOT INSTALLED");
      break;
    case 1:
      strcpy(buffer, "\10INACTIVE     ");
      break;
    case 2:
      strcpy(buffer, "\7ACTIVE       ");
      break;
    default:
      strcpy(buffer, "\11UNKNOWN      ");
      break;
  }
}

/*
 * display_rtc_status(x, y, clock)
 *
 *   x, y: where to print
 *   clock: 0 = internal, 1 = external
 *
 * write rtc status to screen
 */
void display_rtc_status(unsigned char x, unsigned char y, unsigned char clock) {
  unsigned char colour;

  if (clock == 0) {
    if (!hasRTC)
      write_text(x, y, 12, "NOT INSTALLED");
    else {
      if (rtc_check) {
        // we wait 20 seconds to see if we have ticks
        if (tod_ticks>20) {
          rtc_check = 0;
          if (rtc_ticks > 2) {
            rtc_ticking = 1;
            if ((isNTSC && rtc_diff > 2) || (!isNTSC && rtc_diff > 1)) {
              strcpy(buffer, "SLOW TICK    ");
              colour = 8;
            } else {
              strcpy(buffer, "TICKING      ");
              colour = 7;
            }
          } else {
            strcpy(buffer, "NOT TICKING  ");
            colour = 10;
          }
          write_text(x, y, colour, buffer);
        } else
          write_text(x, y, 7, "CHECKING");
      }
      if (!rtc_check) {
        sprintf(buffer, "20%02X-%02X-%02X %02X:%02X:%02X",
                rtc_buf[11], rtc_buf[10]&0x1f, rtc_buf[9]&0x3f, rtc_buf[8]&0x3f, rtc_buf[7]&0x7f, rtc_buf[6]);
        write_text(x, y+1, 12, buffer);
      }
    }
  } else if (clock == 1) {
    if (hasExtRTC<2) {
      format_extrtc_status(hasExtRTC);
      write_text(x, y, buffer[0], buffer+1);
    } else {
      if (extrtc_check) {
        // we wait 20 seconds to see if we have ticks
        if (tod_ticks>20) {
          extrtc_check = 0;
          if (extrtc_ticks > 2) {
            extrtc_ticking = 1;
            if ((isNTSC && extrtc_diff > 2) || (!isNTSC && extrtc_diff > 1)) {
              strcpy(buffer, "SLOW TICK    ");
              colour = 8;
            } else {
              strcpy(buffer, "TICKING      ");
              colour = 7;
            }
          } else {
            strcpy(buffer, "NOT TICKING  ");
            colour = 10;
          }
          write_text(x, y, colour, buffer);
        } else
          write_text(x, y, 7, "CHECKING");

      }
      if (!rtc_check) {
        sprintf(buffer, "20%02X-%02X-%02X %02X:%02X:%02X",
                extrtc_buf[13], extrtc_buf[12]&0x1f, extrtc_buf[11]&0x3f, extrtc_buf[9]&0x3f, extrtc_buf[8]&0x7f, extrtc_buf[7]);
        write_text(x, y+1, 12, buffer);
      }
    }
  }
}

void display_rtc_debug(unsigned char x, unsigned char y, unsigned char colour, unsigned char mode) {
  // DEBUG output in the bottom line
  switch (mode) {
    case 1:
      sprintf(buffer, "IRTC %02X:%02X %04X TOD %02X:%02X %04X DIFF %04X",
               rtc_buf[7]&0x7f, rtc_buf[6], rtc_ticks, tod_buf[6]&0x7f, tod_buf[5], tod_ticks, rtc_diff);
      break;
    case 2:
      sprintf(buffer, "ERTC %02X:%02X %04X TOD %02X:%02X %04X DIFF %04X",
               extrtc_buf[8]&0x7f, extrtc_buf[9], extrtc_ticks, tod_buf[6]&0x7f, tod_buf[5], tod_ticks, extrtc_diff);
      break;
    default:
      strcpy(buffer, "                                                     ");
  }
  write_text(x, y, colour, buffer);
  if (mode>0) {
    if (isNTSC)
      write_text(strlen(buffer)+1, y, colour, "NTSC");
    else
      write_text(strlen(buffer)+1, y, colour, "PAL ");
  }
}

/*
 * draw_screen
 *
 * update the whole screen (except RTC tick stuff)
 */
void draw_screen(void)
{
  unsigned char row, col, i;

  // clear screen
  lfill(SCREEN_ADDRESS, 0x20,2000);

  // write header
  write_text(0, 0, 1, "MEGA65 INFORMATION");
  write_text(54, 0, 12, "(C) 2022 MEGA - MUSEUM OF");
  write_text(54, 1, 12, "   ELECTRONIC GAMES & ART");
  write_text(0, 1, 1, "cccccccccccccccccc");
  write_text(62, 24, 1, "F3-EXIT F5-RESTART");

  // get Hardware information
  copy_hw_version();

  // write model
  write_text(0, 3, 1, "MEGA65 MODEL:");
  write_text(15, 3, 7, format_mega_model());

  // output fpga versions
  write_text(0, 5, 1, "ARTIX VERSION:");
  write_text(15, 5, 7, format_fpga_hash(7, 0));
  write_text(25, 5, 7, format_datestamp(7, 0xff));
  write_text(0, 6, 1, "MAX10 VERSION:");
  write_text(15, 6, 7, format_fpga_hash(13, 1));
  write_text(25, 6, 7, format_datestamp(13, 0x3f));
  write_text(0, 7, 1, "KEYBD VERSION:");
  write_text(15, 7, 7, format_fpga_hash(1, 0));
  write_text(25, 7, 7, format_datestamp(1, 0xff));

  // RTC (labels only, rest is done in mainloop)
  write_text(40, 5, 1, "INTERNAL RTC:");
  write_text(40, 7, 1, "EXTERNAL RTC:");

  // HYPPO/HDOS Version
  write_text(0, 9, 1, "HYPPO/HDOS:");
  write_text(15, 9, 7, format_hyppo_version());

  // ROM version
  write_text(0, 10, 1, "ROM VERSION:");
  write_text(15, 10, 7, format_rom_version());

  // Utility versions (need to load file to parse...)
  row = 12; col = 0;
  for (i=0; SDessentials[i][0] != 0; i++) {
    read_file_from_sdcard(SDessentials[i], 0x40000L);
    strcpy(buffer, SDessentials[i]);
    strcat(buffer, ":");
    write_text(col, row, 1, buffer);
    if (PEEK(0xd021U)>6) { // not found increments background, stupid!
      POKE(0xd021U, 6); // restore blue!
      write_text(col + 14, row, 10, "FILE NOT FOUND");
    } else
      write_text(col + 14, row, 7, format_util_version(0x40000L));
    if (!col)
      col=40;
    else {
      col=0;
      row++;
    }
  }

}

/*
 * init_megainfo
 *
 * initialize basic structures and screen
 */
void init_megainfo() {
  // in NTSC mode the TOD clock ticks 60 times in 50 seconds
  // detect NTSC mode to compensate for this
  isNTSC = (lpeek(0xFFD306fL) & 0x80) == 0x80;
  hasExtRTC = get_extrtc_status();
  get_rtc_stats(1); // initialise rtc data cache
  draw_screen();
}

/*
 * do_megainfo
 */
void do_megainfo()
{
  unsigned char x, rtcDEBUG = 1;

  init_megainfo();

  // clear keybuffer
  while ((x = PEEK(0xD610U))) POKE(0xD610U, x);

  // mainloop
  while (1) {
    x = PEEK(0xD610U);

    // update clocks
    if (get_rtc_stats(0)) {
      display_rtc_status(54, 5, 0);
      display_rtc_status(54, 7, 1);
      display_rtc_debug(0, 24, 12, rtcDEBUG);
    }

    if (x==0) continue;
    POKE(0xD610U, x);

    switch (x) {
      case 0xF1: // F1 - Toggle DEBUG
        rtcDEBUG++;
        if (rtcDEBUG==3) rtcDEBUG = 0;
        display_rtc_debug(0, 24, 12, rtcDEBUG);
        break;
      case 0xF5: // F5 - REFRESH
        init_megainfo();
        break;
      case 0xF3: // F3
      case 0x1b: // ESC
      case 0x03: // RUN-STOP
        return; // EXIT!
    }
  }

  return;
}
