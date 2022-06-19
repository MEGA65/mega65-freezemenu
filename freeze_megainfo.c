#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freezer.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

void write_text(unsigned char x1, unsigned char y1, unsigned char colour, char *t)
{
  unsigned char ofs = 0, x, c;
  for (x = x1; t[x - x1]; x++) {
    c = t[x - x1];
    if ((c >= 'A') && (c <= 'Z'))
      c -= 0x40;
    else if ((c >= 'a') && (c <= 'z'))
      c -= 0x20;
    lpoke(SCREEN_ADDRESS + y1 * 80 + x * 2 + 0, c);
    lpoke(SCREEN_ADDRESS + y1 * 80 + x * 2 + 1, 0);
    lpoke(COLOUR_RAM_ADDRESS + y1 * 80 + x * 2 + 0, 0x00);
    lpoke(COLOUR_RAM_ADDRESS + y1 * 80 + x * 2 + 1, colour);
  }
}

static char buffer[256], numval[32];
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

void output_fpga_version(unsigned char y, unsigned char off, char *prefix) {
  unsigned char plen = strlen(prefix) + 1;

  write_text(0, y, 1, prefix);
  write_text(plen, y, 1, "VERSION:");

  snprintf(buffer, 79, "%02X%02X%02X%02X", version_buffer[off+5], version_buffer[off+4], version_buffer[off+3], version_buffer[off+2]);
  write_text(16, y, 7, buffer);

  wval = (((unsigned short)version_buffer[off+1]) << 8) + (unsigned short)version_buffer[off+0];
  snprintf(buffer, 79, "%04X", wval);
  write_text(26, y, 7, format_date(wval));
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

static unsigned char hyppo_ver[32] = {
  0x78, 0xa9, 0x00,
  0x8d, 0x40, 0xd6,
  0xea,
  0x8d, 0x80, 0x03,
  0x8e, 0x81, 0x03,
  0x8c, 0x82, 0x03,
  0x9c, 0x83, 0x03,
  0x60
};
void get_hyppo_version(void) {
  lcopy((long)hyppo_ver, (long)0x0340, 32);
  
  __asm__("jsr $0340");

  buffer[0] = 0;
  itoa(lpeek(0x0380L), numval, 10); strcat(buffer, numval); strcat(buffer, ".");
  itoa(lpeek(0x0381L), numval, 10); strcat(buffer, numval); strcat(buffer, " / ");
  itoa(lpeek(0x0382L), numval, 10); strcat(buffer, numval); strcat(buffer, ".");
  itoa(lpeek(0x0383L), numval, 10); strcat(buffer, numval);
}

void output_util_version(unsigned char y, unsigned char col, long addr) {
  unsigned short i, j=0;

  lcopy(addr, (long)code_buf, 512);

  strncpy(buffer, "UNKNOWN VERSION", 64);
  for (i=0; i<512; i++) {
    if (code_buf[i]==0x56 && code_buf[i+1]==0x3a) {
      i += 2;
      while (j<64 && code_buf[i])
        buffer[j++] = code_buf[i++];
      buffer[j] = 0;
      break;
    }
  }

  wval = strlen(buffer);
  wval2 = 0;
  if (wval > 38) {
    wval2 = wval-38;
    wval = 38;
  }
  write_text(39-wval, y, col, buffer+wval2);
}

void draw_screen(void)
{
  // clear screen
  lpoke(SCREEN_ADDRESS, 0x20);
  lpoke(SCREEN_ADDRESS + 1, 0x00);
  lpoke(SCREEN_ADDRESS + 2, 0x20);
  lpoke(SCREEN_ADDRESS + 3, 0x00);
  lcopy(SCREEN_ADDRESS, SCREEN_ADDRESS + 4, 2000 - 4);

  // write header
  write_text(0, 0, 1, "MEGA65 INFORMATION");
  write_text(0, 1, 1, "cccccccccccccccccc");
  write_text(17, 24, 1, "F3/ESC/RUNSTOP TO EXIT");

  // get FPGA information
  lcopy(0xFFD3629L, (long)version_buffer, 32);

  // write model
  write_text(0, 3, 1, "MEGA65 MODEL:");
  output_mega_model(16, 3, 7, version_buffer[0]);

  // output fpga versions
  output_fpga_version(5, 7, "XILINX"); // uses version buffer
  output_fpga_version(6, 13, "MAX10"); // uses version buffer
  output_fpga_version(7, 1, "KEYBD");  // uses version buffer

  // hyppo/hdos
  write_text(0, 9, 1, "HYPPO/HDOS:");
  //get_hyppo_version(); -- does not work...
  strcpy(buffer, "?.? / ?.?");
  write_text(16, 9, 7, buffer);

  // ROM version
  write_text(0, 10, 1, "ROM VERSION:");
  write_text(16, 10, 7, get_rom_version());

  // Utility versions (need to load file to parse...)
  read_file_from_sdcard("FREEZER.M65", 0x40000L);
  write_text(0, 11, 1, "FREEZER VERSION:");
  output_util_version(12, 7, 0x40000L);
  
  write_text(0, 13, 1, "MEGAINFO VERSION:");
  output_util_version(14, 7, 0x801L); // we are running MEGAINFO, so we can look at the loaded code
}

void do_megainfo(void)
{
  unsigned char x;

  draw_screen();

  while (1) {
    x = PEEK(0xD610U);
    POKE(0xD610U,x);

    switch (x) {
      case 0xF3: // F3
      case 0x1b: // ESC
      case 0x03: // RUN-STOP
        return; // exit
    }
  }

  return;
}
