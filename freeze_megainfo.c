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

// these are the SD Card Essentials we expect
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

void write_text(unsigned char x1, unsigned char y1, unsigned char colour, char *t)
{
  unsigned char ofs = 0, x, c;
  for (x = x1; t[x - x1]; x++) {
    c = t[x - x1];
    if ((c >= 'A') && (c <= 'Z'))
      c -= 0x40;
    else if ((c >= 'a') && (c <= 'z'))
      c -= 0x20;
    lpoke(SCREEN_ADDRESS + y1 * 80 + x , c);
    lpoke(COLOUR_RAM_ADDRESS + y1 * 80 + x, colour);
  }
}

static char buffer[256], numval[32], isNTSC = 0;
static unsigned short wval, wval2;
static unsigned char version_buffer[256];
static unsigned char code_buf[512];

void output_mega_model(unsigned char x, unsigned char y, unsigned char colour, unsigned char val) {
  switch (val) {
    case 1:
      strncpy(buffer, "MEGA65 R1", 80);
      break;
    case 2:
      strncpy(buffer, "MEGA65 R2", 80);
      break;
    case 3:
      strncpy(buffer, "MEGA65 R3", 80);
      break;
    case 33:
      strncpy(buffer, "MEGAPHONE R1 PROTOTYPE", 80);
      break;
    case 64:
      strncpy(buffer, "NEXYS 4 PSRAM", 80);
      break;
    case 65:
      strncpy(buffer, "NEXYS 4 DDR (NO WIDGET)", 80);
      break;
    case 66:
      strncpy(buffer, "NEXYS 4 DDR (WIDGET)", 80);
      break;
    case 253:
      strncpy(buffer, "QMTECH WUKONG BOARD", 80);
      break;
    case 254:
      strncpy(buffer, "SIMULATED MEGA65", 80);
      break;
    case 255:
      strncpy(buffer, "HARDWARE NOT SPECIFIED", 80);
      break;
    default:
      snprintf(buffer, 79, "UNKNOWN MODEL $%02X", version_buffer[0]);
      break;
  }

  write_text(x, y, colour, buffer);
}

char *format_date(unsigned short ts) {
  unsigned char m=1;
  unsigned short y=2020;

  while (ts>366) {
    y++;
    ts-=366;
  }

  if (m==1 && ts>31) { m++; ts -= 31; }
  if (m==2 && !(y&3) && ts>29) { m++; ts-=29; }
  if (m==2 && (y&3) && ts>28) { m++; ts-=28; }
  if (m==3 && ts>31) { m++; ts-=31;}
  if (m==4 && ts>30) { m++; ts-=30;}
  if (m==5 && ts>31) { m++; ts-=31;}
  if (m==6 && ts>30) { m++; ts-=30;}
  if (m==7 && ts>31) { m++; ts-=31;}
  if (m==8 && ts>30) { m++; ts-=30;}
  if (m==9 && ts>31) { m++; ts-=31;}
  if (m==10 && ts>30) { m++; ts-=30;}
  if (m==11 && ts>31) { m++; ts-=31;}

  itoa(y, numval, 10); strcpy(buffer, numval);
  if (m>9)
    strcat(buffer, "-");
  else
    strcat(buffer, "-0");
  itoa(m, numval, 10); strcat(buffer, numval);
  if (ts>9)
    strcat(buffer, "-");
  else
    strcat(buffer, "-0");
  itoa(ts, numval, 10); strcat(buffer, numval);

  return buffer;
}

void output_fpga_version(unsigned char x, unsigned char y, unsigned char off, unsigned char msbmask, unsigned char reverse, char *prefix) {
  unsigned char plen = strlen(prefix) + 1;

  write_text(0, y, 1, prefix);
  write_text(plen, y, 1, "VERSION:");

  if (reverse)
    snprintf(buffer, 79, "%02X%02X%02X%02X", version_buffer[off+2], version_buffer[off+3], version_buffer[off+4], version_buffer[off+5]);
  else
    snprintf(buffer, 79, "%02X%02X%02X%02X", version_buffer[off+5], version_buffer[off+4], version_buffer[off+3], version_buffer[off+2]);
  write_text(x, y, 7, buffer);

  wval = (((unsigned short)(version_buffer[off+1]&msbmask)) << 8) + (unsigned short)version_buffer[off];
  //snprintf(buffer, 79, "%04X", wval);
  write_text(x + 10, y, 7, format_date(wval));
}

char *get_rom_version(void) {
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
      strncpy(buffer, "SX64 JIFFY", 40);
    else
      strncpy(buffer, "C64 JIFFY", 40);
    return buffer;
  }

  if (freeze_peek(0x2e449L) == 0x2e) {
    strncpy(buffer, "C64GS", 40);
    return buffer;
  }
  if (freeze_peek(0x2e119L) == 0xc9) {
    strncpy(buffer, "C64 REV1", 40);
    return buffer;
  }
  if (freeze_peek(0x2e67dL) == 0xb0) {
    strncpy(buffer, "C64 REV2 JP", 40);
    return buffer;
  }
  if (freeze_peek(0x2ebaeL) == 0x5b) {
    strncpy(buffer, "C64 REV3 DK", 40);
    return buffer;
  }
  if (freeze_peek(0x2e0efL) == 0x28) {
    strncpy(buffer, "C64 SCAND", 40);
    return buffer;
  }
  if (freeze_peek(0x2ebf3L) == 0x40) {
    strncpy(buffer, "C64 SWEDEN", 40);
    return buffer;
  }
  if (freeze_peek(0x2e461L) == 0x20) {
    strncpy(buffer, "CYCLONE 1.0", 40);
    return buffer;
  }
  if (freeze_peek(0x2e4a4L) == 0x41) {
    strncpy(buffer, "DOLPHIN 1.0", 40);
    return buffer;
  }
  if (freeze_peek(0x2e47fL) == 0x52) {
    strncpy(buffer, "DOLPHIN 2AU", 40);
    return buffer;
  }
  if (freeze_peek(0x2eed7L) == 0x2c) {
    strncpy(buffer, "DOLPHIN 2P1", 40);
    return buffer;
  }
  if (freeze_peek(0x2e7d2L) == 0x6b) {
    strncpy(buffer, "DOLPHIN 2P2", 40);
    return buffer;
  }
  if (freeze_peek(0x2e4a6L) == 0x32) {
    strncpy(buffer, "DOLPHIN 2P3", 40);
    return buffer;
  }
  if (freeze_peek(0x2e0f9L) == 0xaa) {
    strncpy(buffer, "DOLPHIN 3.0", 40);
    return buffer;
  }
  if (freeze_peek(0x2e462L) == 0x45) {
    strncpy(buffer, "DOSROM V1.2", 40);
    return buffer;
  }
  if (freeze_peek(0x2e472L) == 0x20) {
    strncpy(buffer, "MERCRY3 PAL", 40);
    return buffer;
  }
  if (freeze_peek(0x2e16dL) == 0x84) {
    strncpy(buffer, "MERCRY NTSC", 40);
    return buffer;
  }
  if (freeze_peek(0x2e42dL) == 0x4c) {
    strncpy(buffer, "PET 4064", 40);
    return buffer;
  }
  if (freeze_peek(0x2e1d9L) == 0xa6) {
    strncpy(buffer, "SX64 CROACH", 40);
    return buffer;
  }
  if (freeze_peek(0x2eba9L) == 0x2d) {
    strncpy(buffer, "SX64 SCAND", 40);
    return buffer;
  }
  if (freeze_peek(0x2e476L) == 0x2a) {
    strncpy(buffer, "TRBOACS 2.6", 40);
    return buffer;
  }
  if (freeze_peek(0x2e535L) == 0x07) {
    strncpy(buffer, "TRBOACS 3P1", 40);
    return buffer;
  }
  if (freeze_peek(0x2e176L) == 0x8d) {
    strncpy(buffer, "TRBOASC 3P2", 40);
    return buffer;
  }
  if (freeze_peek(0x2e42aL) == 0x72) {
    strncpy(buffer, "TRBOPROC US", 40);
    return buffer;
  }
  if (freeze_peek(0x2e4acL) == 0x81) {
    strncpy(buffer, "C64C 251913", 40);
    return buffer;
  }
  if (freeze_peek(0x2e479L) == 0x2a) {
    strncpy(buffer, "C64 REV2", 40);
    return buffer;
  }
  if (freeze_peek(0x2e535L) == 0x06) {
    strncpy(buffer, "SX64 REV4", 40);
    return buffer;
  }

  strncpy(buffer, "UNKNOWN ROM", 40);
  return buffer;
}

static unsigned char hyppo_version[4] = { 0xff, 0xff, 0xff, 0xff };

char *get_hyppo_version(void) {
  
  hyppo_getversion(hyppo_version);

  if (hyppo_version[0] == hyppo_version[1] && hyppo_version[1] == hyppo_version[2] && hyppo_version[2] == hyppo_version[3] && hyppo_version[0] == 0xff)
    strcpy(buffer, "?.? / ?.?");
  else {
    itoa(hyppo_version[0], numval, 10); strcpy(buffer, numval); strcat(buffer, ".");
    itoa(hyppo_version[1], numval, 10); strcat(buffer, numval); strcat(buffer, " / ");
    itoa(hyppo_version[2], numval, 10); strcat(buffer, numval); strcat(buffer, ".");
    itoa(hyppo_version[3], numval, 10); strcat(buffer, numval);
  }

  return buffer;
}

void output_util_version(unsigned char x, unsigned char y, unsigned char colour, long addr) {
  unsigned short i, j=0;

  if (addr == 0) {
    write_text(x, y, 10, "NOT FOUND");
    return;
  }

  lcopy(addr, (long)code_buf, 512);

  strncpy(buffer, "  UNKNOWN VERSION", 64);
  for (i=0; i<512; i++) {
    if (code_buf[i]==0x56 && code_buf[i+1]==0x3a) {
      i += 2;
      while (j<64 && code_buf[i])
        buffer[j++] = code_buf[i++];
      buffer[j] = 0;
      break;
    }
  }

  // strips the 20 from the start of the string, and cuts on the left side
  wval = strlen(buffer)-2;
  wval2 = 2;
  if (wval > 42) {
    wval2 = wval-40; // add the two
    wval = 42;
  }
  write_text(x, y, colour, buffer+wval2);
}

static unsigned char tod_init = 1, tod_buf[8] = { 0,0,0,0,0,0,0,0 }, rtc_buf[8] = { 0,0,0,0,0,0,0,0 };
unsigned short get_rtcstats() {
  short offset, pa, pb;

  if (tod_init == 1) {
    lcopy(0xffd7110, (long)rtc_buf, 4);
    lcopy(0xffd3c08, (long)tod_buf, 4);
    tod_init = 0;
  }
  lcopy(0xffd7110, (long)rtc_buf+4, 4);
  lcopy(0xffd3c08, (long)tod_buf+4, 4);

  // only looking at seconds here
  pa = ((tod_buf[1]>>4)&0x7)*10 + (tod_buf[1]&0xf) + (((tod_buf[2]>>4)&0x7)*10 + (tod_buf[2]&0xf))*60;
  pb = ((tod_buf[5]>>4)&0x7)*10 + (tod_buf[5]&0xf) + (((tod_buf[6]>>4)&0x7)*10 + (tod_buf[6]&0xf))*60;
  offset = pb-pa;
  if (offset<0) {
    tod_init = 1;
    offset = 0;
  }

  return offset;
}

void update_rtc(unsigned short ticks) {
  short offset, pa, pb, diff;

  pa = ((rtc_buf[0]>>4)&0x7)*10 + (rtc_buf[0]&0xf) + (((rtc_buf[1]>>4)&0x7)*10 + (rtc_buf[1]&0xf))*60;
  pb = ((rtc_buf[4]>>4)&0x7)*10 + (rtc_buf[4]&0xf) + (((rtc_buf[5]>>4)&0x7)*10 + (rtc_buf[5]&0xf))*60;
  offset = pb-pa;
  if (offset<0) {
    tod_init = 1;
    return;
  }
  if (offset>ticks)
    diff = offset-ticks;
  else
    diff = ticks-offset;

  POKE(0xD020U, PEEK(0xd020u)+1);
  snprintf(buffer, 44, "RTC %02X:%02X TOD %02X:%02X ELAPSED %04X DIFF %04X", rtc_buf[5], rtc_buf[4], tod_buf[6], tod_buf[5], ticks, diff);
  write_text(0, 24, 12, buffer);
}

void draw_screen(void)
{
  unsigned char row, col, i;

  // clear screen
  lfill(SCREEN_ADDRESS, 0x20,2000);

  // write header
  write_text(0, 0, 1, "MEGA65 INFORMATION");
  write_text(0, 1, 1, "cccccccccccccccccc");
  write_text(57, 24, 1, "F3/ESC/RUNSTOP TO EXIT");

  // get FPGA information
  lcopy(0xFFD3629L, (long)version_buffer, 32);

  // write model
  write_text(0, 3, 1, "MEGA65 MODEL:");
  output_mega_model(15, 3, 7, version_buffer[0]);

  // output fpga versions
  output_fpga_version(15, 5, 7,  0xff, 0, "XILINX"); // uses version buffer
  output_fpga_version(15, 6, 13, 0x3f, 1, "MAX10");  // uses version buffer
  output_fpga_version(15, 7, 1,  0xff, 0, "KEYBD");  // uses version buffer

  // hyppo/hdos
  write_text(0, 9, 1, "HYPPO/HDOS:");
  write_text(15, 9, 7, get_hyppo_version());

  // ROM version
  write_text(0, 10, 1, "ROM VERSION:");
  write_text(15, 10, 7, get_rom_version());

  // Utility versions (need to load file to parse...)
  row = 12; col = 0;
  for (i=0; SDessentials[i][0] != 0; i++) {
    read_file_from_sdcard(SDessentials[i], 0x40000L);
    strcpy(buffer, SDessentials[i]);
    strcat(buffer, ":");
    write_text(col, row, 1, buffer);
    if (PEEK(0xd021U)>6) {
      POKE(0xd021U, 6);
      output_util_version(col + 14, row, 7, 0L);
    } else {
      output_util_version(col + 14, row, 7, 0x40000L);
    }
    if (!col) col=40;
    else { col=0; row++;}
  }

}

void do_megainfo(void)
{
  unsigned char x;
  unsigned short s1=0, s2=-1;

  isNTSC = freeze_peek(0xFFD306fL) & 0x80;
  get_rtcstats();
  draw_screen();

  // clear keybuffer
  while ((x=PEEK(0xD610U))) POKE(0xD610U, x);

  while (1) {
    x = PEEK(0xD610U);

    // get clock stats
    s1 = get_rtcstats();
    if (s1!=s2) { // only update if changed
      update_rtc(s1);
      s2=s1;
    }

    if (x==0) continue;
    POKE(0xD610U, x);

    switch (x) {
      case 0xF3: // F3
      case 0x1b: // ESC
      case 0x03: // RUN-STOP
        return; // exit
    }
  }

  return;
}
