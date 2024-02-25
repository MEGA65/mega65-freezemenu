#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freezer.h"
#include "freezer_common.h"
#include "infohelper.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

/*
 * Constants
 */
// clang-format off
static char SDessentials[][13] = {
  "FREEZER.M65",
  "ETHLOAD.M65",
  "MEGAINFO.M65",
  "MONITOR.M65",
  "MAKEDISK.M65",
  "ROMLOAD.M65",
  "AUDIOMIX.M65",
  "SPRITED.M65",
  ""
};
// clang-format on

/*
 * Global Variables
 */
#define BUFFER_LENGTH 254
#define BUFFER_COLOUR 255
static char buffer[BUFFER_LENGTH + 2], tempstr32[32], isNTSC = 0, hasRTC = 0, m65model, m65submodel;
static unsigned char code_buffer[512], ymd[3];

/*
 * write_text(x, y, colour, text)
 *
 *   x, y: screen position
 *   colour: colour top write in
 *   text: zero terminated string
 *
 * writes text to the screen using colour. converts to screencode (upper)
 */
void write_text(unsigned char x, unsigned char y, unsigned short colour, char* text)
{
  unsigned char i, c;
  for (i = 0; text[i]; i++) {
    c = text[i];
    if ((c >= 'A') && (c <= 'Z'))
      c -= 0x40;
    else if ((c >= 'a') && (c <= 'z'))
      c -= 0x20;
    else if (c == '~') // ~ become pi
      c = 94;
    if (colour & 0x100 && c < 128)
      c |= 0x80;
    lpoke(SCREEN_ADDRESS + y * 80 + x + i, c);
    lpoke(COLOUR_RAM_ADDRESS + y * 80 + x + i, (unsigned char)(colour & 0xff));
  }
}

/*
 * write_text_upper(x, y, colour, text)
 *
 *   x, y: screen position
 *   colour: colour top write in
 *   text: zero terminated string
 *
 * writes text to the screen using colour. converts to screencode,
 * and all lower is displayed as upper
 */
void write_text_upper(unsigned char x, unsigned char y, unsigned short colour, char* text)
{
  unsigned char i, c;
  for (i = 0; text[i]; i++) {
    c = text[i] & 0x7f;
    if ((c >= 'A') && (c <= 'Z'))
      c -= 0x40;
    else if ((c >= 'a') && (c <= 'z'))
      c -= 0x60;
    else if (c == '~') // ~ become pi
      c = 94;
    if (colour & 0x100 && c < 128)
      c |= 0x80;
    lpoke(SCREEN_ADDRESS + y * 80 + x + i, c);
    lpoke(COLOUR_RAM_ADDRESS + y * 80 + x + i, (unsigned char)(colour & 0xff));
  }
}

/*
 * copy_hw_version()
 *
 *   globals: code_buffer
 *
 * extracts hardware and FPGA version from $FFD3629 by
 * copying 32 bytes to global code_buffer, to make access
 * faster (lpeek is a dma_copy!)
 */
void copy_hw_version()
{
  lcopy(0xFFD3628L, (long)code_buffer, 33);
  m65model = code_buffer[1];
  m65submodel = (code_buffer[0] >> 4) & 0xf;
}

/*
 * ge_mega_model() -> char *
 *
 *   returns: string (tempstr32 or static)
 *   globals: buffer
 *
 * translate m65model + m65submodel to model string
 *
 * get_hw_version must have been called to fill code_buffer
 */
char* format_mega_model()
{
  switch (m65model) {
  case 0x01:
    return "MEGA65 R1";
  case 0x02:
    hasRTC = 1;
    return "MEGA65 R2";
  case 0x03:
  case 0x04:
  case 0x05:
  case 0x06:
    // format new boards with model/submodel scheme
    hasRTC = 1;
    strncpy(tempstr32, "MEGA65 R3 ", 31);
    tempstr32[8] = 0x30 + m65model;
    tempstr32[9] = m65submodel ? 0x40 + m65submodel : 32;
    break;
  case 0x21:
    return "MEGAPHONE R1 PROTOTYPE";
  case 0x22:
    return "MEGAPHONE R4 PROTOTYPE";
  case 0x40:
    return "NEXYS 4 PSRAM";
  case 0x41:
    return "NEXYS 4 DDR (NO WIDGET)";
  case 0x42:
    return "NEXYS 4 DDR (WIDGET)";
  case 0x60:
    return "QMTECH A100T";
  case 0x61:
    return "QMTECH A200T";
  case 0x62:
    return "QMTECH A325T";
  case 0xfd:
    return "QMTECH WUKONG BOARD";
  case 0xfe:
    return "SIMULATED MEGA65";
  default:
    snprintf(tempstr32, 31, "UNKNOWN MODEL $%02X.%01X", m65model, m65submodel);
    break;
  }
  return tempstr32;
}

/*
 * format_datestamp(offset, msbmask) -> char *
 *
 *   offset: offset to the start of the FPGA fields (1-KEYBD, 7-ARTIX, 13-MAX10)
 *   msbmask: mask msb byte (neeeded for MAX10, 0x3f)
 *
 *   returns: string (buffer)
 *   globals: tempstr32, buffer, ymd
 *
 * formats a FPGA datestamp (days since 2020-01-01, years full 366 days)
 * to a string YYYY-MM-DD.
 * stores year (without century), month, day as uchar in ymd[3]
 *
 * get_hw_version must have been called to fill code_buffer
 */
char* format_datestamp(unsigned char offset, unsigned char msbmask)
{
  unsigned char m = 1;
  unsigned short y = 2020, ds;

  ds = (((unsigned short)(code_buffer[offset + 1] & msbmask)) << 8) + (unsigned short)code_buffer[offset];

  // first remove years. years are always full 366 days!
  while (ds > 366) {
    y++;
    ds -= 366;
  }

  // then find out month and day, years divideable by 4 are jump years (no 100 or 400 in sight!)
  // clang-format off
  if (m==1 && ds>31) { m++; ds -= 31; }
  if (m==2 && !(y&3) && ds>29) { m++; ds-=29; }
  if (m==2 && (y&3) && ds>28) { m++; ds-=28; }
  if (m==3 && ds>31) { m++; ds-=31;}
  if (m==4 && ds>30) { m++; ds-=30;}
  if (m==5 && ds>31) { m++; ds-=31;}
  if (m==6 && ds>30) { m++; ds-=30;}
  if (m==7 && ds>31) { m++; ds-=31;}
  if (m==8 && ds>31) { m++; ds-=31;}
  if (m==9 && ds>30) { m++; ds-=30;}
  if (m==10 && ds>31) { m++; ds-=31;}
  if (m==11 && ds>30) { m++; ds-=30;}
  // clang-format on

  // snprintf can't do %d!
  itoa(y, tempstr32, 10);
  strcpy(buffer, tempstr32);
  if (m > 9)
    strcat(buffer, "-");
  else
    strcat(buffer, "-0");
  itoa(m, tempstr32, 10);
  strcat(buffer, tempstr32);
  if (ds > 9)
    strcat(buffer, "-");
  else
    strcat(buffer, "-0");
  itoa(ds, tempstr32, 10);
  strcat(buffer, tempstr32);

  // save date for external use
  ymd[0] = (unsigned char)(y - 2000);
  ymd[1] = m;
  ymd[2] = ds;

  return buffer;
}

/*
 * format_fpga_hash(offset, reverse) -> char *
 *
 *   offset: offset to the start of the FPGA fields (1-KEYBD, 7-ARTIX, 13-MAX10)
 *   msbmask: reverse byte order
 *
 *   returns: string (buffer)
 *   globals: buffer
 *
 * formats a FPGA commit hash to a 8 character hex number
 *
 * get_hw_version must have been called to fill code_buffer
 */
char* format_fpga_hash(unsigned char offset, unsigned char reverse)
{
  if (reverse)
    sprintf(buffer, "%02X%02X%02X%02X", code_buffer[offset + 2], code_buffer[offset + 3], code_buffer[offset + 4],
        code_buffer[offset + 5]);
  else
    sprintf(buffer, "%02X%02X%02X%02X", code_buffer[offset + 5], code_buffer[offset + 4], code_buffer[offset + 3],
        code_buffer[offset + 2]);

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
char* format_rom_version(void)
{
  // we want to display the version in freeze slot 0!
  find_freeze_slot_start_sector(0);
  freeze_slot_start_sector = *(uint32_t*)0xD681U;
  request_freeze_region_list();

  return detect_rom();
}

/*
 * format_hyppo_version -> char *
 *
 *   returns: string (buffer)
 *   globals: buffer, tempstr32
 *
 * fetches and formats hyppo and hdos version as a string '?.? / ?.?'
 */
char* format_hyppo_version(void)
{
  unsigned char hyppo_version[4] = { 0xff, 0xff, 0xff, 0xff };

  // hypervisor call, external
  hyppo_getversion(hyppo_version);

  if (hyppo_version[0] == hyppo_version[1] && hyppo_version[1] == hyppo_version[2] && hyppo_version[2] == hyppo_version[3]
      && hyppo_version[0] == 0xff)
    strcpy(buffer, "?.? / ?.?");
  else {
    itoa(hyppo_version[0], tempstr32, 10);
    strcpy(buffer, tempstr32);
    strcat(buffer, ".");
    itoa(hyppo_version[1], tempstr32, 10);
    strcat(buffer, tempstr32);
    strcat(buffer, " / ");
    itoa(hyppo_version[2], tempstr32, 10);
    strcat(buffer, tempstr32);
    strcat(buffer, ".");
    itoa(hyppo_version[3], tempstr32, 10);
    strcat(buffer, tempstr32);
  }

  return buffer;
}

/*
 * format_util_version(addr, date) -> uchar
 *
 *   addr: start address in memory
 *
 *   returns: 0 if ok, 1 if older or missing
 *   globals: buffer
 *
 * search the next 256 bytes from start address for a version string
 * starting with 'v:20' and returns the next characters until a zero byte
 * compares to date (which should by artix ymd) and returns 0 if equal or newer
 */
unsigned char format_util_version(long addr, unsigned char* date)
{
  unsigned short i, j = 0;
  unsigned char temp, result = 0, p;

  lcopy(addr, (long)code_buffer, 512);

  strncpy(buffer, "UNKNOWN VERSION", 64);
  for (i = 0; i < 512; i++) {
    if (code_buffer[i] == 0x56 && code_buffer[i + 1] == 0x3a && code_buffer[i + 2] == 0x32 && code_buffer[i + 3] == 0x30) {
      i += 4; // skip v:20
      while (j < 64 && code_buffer[i])
        buffer[j++] = code_buffer[i++];
      buffer[j] = 0;
      break;
    }
  }

  // parse date, starts at first char
  for (p = 0; p < 3; p++) {
    if (buffer[p * 2] >= '0' && buffer[p * 2] <= '9' && buffer[p * 2 + 1] >= '0' && buffer[p * 2 + 1] <= '9') {
      temp = (buffer[p * 2] - '0') * 10 + buffer[p * 2 + 1] - '0';
      /*
      snprintf(tempstr32, 5, "%02X", temp);
      write_text(10+p*3, 20, 12, tempstr32);
      snprintf(tempstr32, 5, "%02X", date[p]);
      write_text(10+p*3, 21, 12, tempstr32);
      */
      if (temp > date[p]) {
        result = 0;
        break;
      }
      else if (temp < date[p]) {
        result = 1;
        break;
      }
    }
    else
      result = 1;
  }

  // cuts on the left side, we always want the rightmost part with the commit hash
  i = strlen(buffer);
  if (i > 25) {
    i -= 25;
    for (j = 0; j < 25; j++)
      buffer[j] = buffer[i + j];
    buffer[j] = 0;
  }

  return result;
}

/*
 * format_hickup_version(addr, date) -> uchar
 *
 *   addr: start address in memory
 *   date: uchar[3] with date
 *
 *   returns: 1 on old version, 0 on actual or newer version
 *   globals: buffer
 *
 * search for GIT: in 40000 upwards
 * tries to parse date and compare to date[3]
 */
unsigned char format_hickup_version(long addr, unsigned char* date)
{
  unsigned short p, i, j = 0;
  char* needle = "GIT: ";
  char* needle2 = ",20";
#define NEEDLE_LEN 5
#define NEEDLE2_LEN 3
  unsigned char version_fail = 1, finished = 0, cmp_idx = 0, temp;

  for (p = 0; p < 64 && !finished; p++) {
    lcopy(addr + 512l * p, (long)code_buffer, 512);
    for (i = 0; i < 512; i++) {
      // looking for needle in the haystack
      if (cmp_idx < NEEDLE_LEN) {
        if (needle[cmp_idx] == code_buffer[i]) {
          cmp_idx++;
        }
        else
          cmp_idx = 0;
      }
      else {
        buffer[j++] = code_buffer[i];
        if (code_buffer[i] == 0) {
          finished = 1;
          break;
        }
      }
    }
  }

  if (!finished) {
    strcpy(buffer, "VERSION NOT FOUND");
    return 1;
  }

  // limit to 38 chars
  buffer[38] = 0;
  i = strlen(buffer);

  // try to parse date
  cmp_idx = 0;
  for (j = 0; j < i; j++) {
    if (cmp_idx < NEEDLE2_LEN) {
      if (needle2[cmp_idx] == buffer[j]) {
        cmp_idx++;
      }
      else
        cmp_idx = 0;
    }
    else {
      for (p = 0; p < 3; p++) {
        if (buffer[j + p * 2] >= '0' && buffer[j + p * 2] <= '9' && buffer[j + p * 2 + 1] >= '0'
            && buffer[j + p * 2 + 1] <= '9') {
          temp = (buffer[j + p * 2] - '0') * 10 + buffer[j + p * 2 + 1] - '0';
          /*
          snprintf(tempstr32, 5, "%02X", temp);
          write_text(p*3, 20, 12, tempstr32);
          snprintf(tempstr32, 5, "%02X", date[p]);
          write_text(p*3, 21, 12, tempstr32);
          */
          if (temp > date[p])
            return 0;
          else if (temp < date[p])
            return 1;
        }
        else
          return 1;
      }
    }
  }

  if (cmp_idx < NEEDLE2_LEN)
    return 2;

  return 0;
}

/*
 * RTC Globals
 */
static unsigned char clock_init = 1, tod_buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char rtc_state = 0, rtc_last_state = 0, rtc_settle = 0, no_extrtc = 0;
static unsigned char rtc_check = 1, rtc_ticking = 0, rtc_diff = 0, rtc_buf[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char rtc_pmu = 0xff;
static unsigned short tod_ov = 0, rtc_ov = 0;
static short tod_last = -1, tod_ticks = 0, rtc_ticks = 0;

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
unsigned char get_rtc_stats(unsigned char reinit)
{
  short pa, pb;

  // fetch external RTC state
  if (no_extrtc || lpeek(0xffd7400) == 0xff) { // external not installed
    if (hasRTC)
      rtc_state = 1;
    else
      rtc_state = 0;
  }
  else {
    if (lpeek(0xffd74fd) & 0x80)
      rtc_state = 3; // external installed & active
    else
      rtc_state = 2; // external installed but inactive
  }

  // clock changed, reinit
  if (rtc_state != rtc_last_state) {
    clock_init = 1;
    rtc_last_state = rtc_state;
    // if clock is active, give it a few seconds to settle
    if (rtc_state & 1)
      rtc_settle = 3;
  }

  if (clock_init || reinit) {
    lcopy(0xffd7110l, (long)rtc_buf, 6);
    lcopy(0xffd3c08l, (long)tod_buf, 4);
    if (m65model >= 0x04 && m65model <= 0x06) {
      rtc_pmu = lpeek(0xffd71d0UL);
      if (rtc_pmu != 0x22) {
        // disable eeprom refresh
        lpoke(0xffd7120UL, 0x04);
        usleep(20000L); // need to wait for slow RTC getting updated
        // set backup switchover mode to LSM, TCM 3V (Battery protection)
        lpoke(0xffd71d0UL, 0x22);
        usleep(20000L);
        // EECMD Update EEPROM
        lpoke(0xffd714fUL, 0x11);
        usleep(20000L);
        // enable eeprom refresh
        lpoke(0xffd7120UL, 0x00);
        usleep(20000L);
        rtc_pmu = lpeek(0xffd71d0UL);
      }
    }
    else
      rtc_pmu = 0xff;
    clock_init = 0;
    tod_ov = 0;
    rtc_ov = 0;
    rtc_check = 1;
    rtc_ticking = 0;
    rtc_diff = 0;
  }
  lcopy(0xffd7110l, (long)rtc_buf + 6, 6);
  lcopy(0xffd3c08l, (long)tod_buf + 4, 4);

  // only looking at seconds here, derived from minutes + seconds
  pa = ((tod_buf[1] >> 4) & 0x7) * 10 + (tod_buf[1] & 0xf) + (((tod_buf[2] >> 4) & 0x7) * 10 + (tod_buf[2] & 0xf)) * 60;
  pb = ((tod_buf[5] >> 4) & 0x7) * 10 + (tod_buf[5] & 0xf) + (((tod_buf[6] >> 4) & 0x7) * 10 + (tod_buf[6] & 0xf)) * 60;
  tod_ticks = pb - pa;
  if (tod_ticks < 0) {
    tod_ticks += 3600; // we can work with one hour overflow
    tod_ov++;
    if (tod_ov > 1) { // after that we reinitialise (who lets this run for hours?)
      clock_init = 1;
      tod_ticks = 0;
    }
  }

  if (tod_ticks == tod_last)
    return 0;
  // tod_ticks changed, update rtc

  // external rtc needs a moment to settle
  if (rtc_settle > 0) {
    rtc_settle--;
    clock_init = 1;
    return 0;
  }

  tod_last = tod_ticks;

  // handle RTC
  if (rtc_state & 1) {
    pa = ((rtc_buf[0] >> 4) & 0x7) * 10 + (rtc_buf[0] & 0xf) + (((rtc_buf[1] >> 4) & 0x7) * 10 + (rtc_buf[1] & 0xf)) * 60;
    pb = ((rtc_buf[6] >> 4) & 0x7) * 10 + (rtc_buf[6] & 0xf) + (((rtc_buf[7] >> 4) & 0x7) * 10 + (rtc_buf[7] & 0xf)) * 60;
    rtc_ticks = pb - pa;
    if (rtc_ticks < 0) {
      rtc_ticks += 3600; // we can work with one hour overflow
      rtc_ov++;
      if (rtc_ov > 1) {
        clock_init = 1;
        return 0;
      }
    }

    if (rtc_ticks > tod_ticks)
      rtc_diff = rtc_ticks - tod_ticks;
    else
      rtc_diff = tod_ticks - rtc_ticks;
  }

  return 1;
}

/*
 * format_extrtc_status(status)
 *
 *   status: status code from get_rtc_stats
 *
 *   globals: buffer
 *
 * formats status as a string in buffer+1
 * sets buffer+0 to the colour code 0-15
 */
void format_extrtc_status(unsigned char status)
{
  switch (status) {
  case 0:
    strcpy(buffer, "\10NO RTC AVAILABLE  ");
    break;
  case 1:
    strcpy(buffer, "\7INTERNAL          ");
    break;
  case 2:
    strcpy(buffer, "\10EXTERNAL, INACTIVE");
    break;
  case 3:
    strcpy(buffer, "\7EXTERNAL, ACTIVE  ");
    break;
  default:
    strcpy(buffer, "\11UNKNOWN           ");
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
void display_rtc_status(unsigned char x, unsigned char y)
{
  unsigned char colour, offs = 0xff;

  // write out rtc state
  format_extrtc_status(rtc_state);
  write_text(x, y, buffer[0], buffer + 1);

  if (rtc_state & 0x1) { // 1 and 3 are active
    if (rtc_check) {
      // we wait 20 seconds to see if we have ticks
      if (tod_ticks > 20) {
        rtc_check = 0;
        if (rtc_ticks > 2) {
          rtc_ticking = 1;
          if (rtc_diff > 1) {
            if (no_extrtc && isNTSC)
              strcpy(buffer, "SLOW TICK, SLOW CIA TOD!");
            else {
              strcpy(buffer, "SLOW TICK               ");
              offs = 9;
            }
            colour = 10;
          }
          else {
            strcpy(buffer, "TICKING                 ");
            offs = 7;
            colour = 7;
          }
        }
        else {
          strcpy(buffer, "NOT TICKING             ");
          offs = 11;
          colour = 10;
        }
        if (offs != 0xff && rtc_pmu != 0xff) {
          if ((rtc_pmu & 0x30) == 0x20)
            memcpy(buffer + offs, ", BACKUP ON", 11);
          else
            memcpy(buffer + offs, ", BACKUP OFF", 12);
        }
        write_text(x, y + 1, colour, buffer);
      }
      else
        write_text(x, y + 1, 7, "CHECKING                ");
    }
    if (!rtc_check) {
      sprintf(buffer, "20%02X-%02X-%02X %02X:%02X:%02X", rtc_buf[11], rtc_buf[10] & 0x1f, rtc_buf[9] & 0x3f,
          rtc_buf[8] & 0x3f, rtc_buf[7] & 0x7f, rtc_buf[6]);
      write_text(x, y + 2, 12, buffer);
    }
    else
      write_text(x, y + 2, 12, "                    ");
  }
}

void display_rtc_debug(unsigned char x, unsigned char y, unsigned char colour, unsigned char mode)
{
  // DEBUG output in the bottom line
  switch (mode) {
  case 1:
    sprintf(buffer, " RTC %02X:%02X %04X TOD %02X:%02X %04X DIFF %04X PMU %02X",
        rtc_buf[7] & 0x7f, rtc_buf[6], rtc_ticks, tod_buf[6] & 0x7f, tod_buf[5], tod_ticks, rtc_diff, rtc_pmu);
    if (rtc_state == 1)
      buffer[0] = 'I';
    else if (rtc_state == 2 || rtc_state == 3)
      buffer[0] = 'E';
    break;
  default:
    strcpy(buffer, "                                                     ");
  }
  write_text(x, y, colour, buffer);
  if (mode > 0) {
    if (isNTSC)
      write_text(strlen(buffer) + 1, y, colour, "NTSC");
    else
      write_text(strlen(buffer) + 1, y, colour, "PAL ");
  }
}

/*
 * draw_screen
 *
 * update the whole screen (except RTC tick stuff)
 */
void draw_screen(void)
{
  unsigned char row, col, i, fail, artix_ymd[3];

  // clear screen
  lfill(SCREEN_ADDRESS, 0x20, 2000);

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
  write_text(40, 3, 1, "SCREEN MODE:");
  if (isNTSC)
    write_text(54, 3, 7, "NTSC");
  else
    write_text(54, 3, 7, "PAL");

  // output fpga versions
  write_text(0, 5, 1, "ARTIX VERSION:");
  write_text(15, 5, 7, format_fpga_hash(8, 0));
  write_text(25, 5, 7, format_datestamp(8, 0xff));
  // save artix date for hickup date check
  artix_ymd[0] = ymd[0];
  artix_ymd[1] = ymd[1];
  artix_ymd[2] = ymd[2];
  if (ymd[0] < 22 || (ymd[0] == 22 && (ymd[1] < 6 || (ymd[1] == 6 && ymd[2] < 23)))) {
    no_extrtc = 1;
    write_text(19, 1, 256 + 24, "UPDATE CORE FOR EXTERNAL RTC SUPPORT!");
  }

  write_text(0, 6, 1, "KEYBD VERSION:");
  write_text(15, 6, 7, format_fpga_hash(2, 0));
  write_text(25, 6, 7, format_datestamp(2, 0xff));

  if (m65model > 0x00 && m65model < 0x04) {
    // only mega65r1-3 have a MAX10 FPGA, it was removed for mega65r4
    write_text(0, 7, 1, "MAX10 VERSION:");
    write_text(15, 7, 7, format_fpga_hash(14, 1));
    write_text(25, 7, 7, format_datestamp(14, 0x3f));
  }

  // RTC (labels only, rest is done in mainloop)
  write_text(40, 5, 1, "RTC STATUS:");

  // HYPPO/HDOS Version
  write_text(0, 9, 1, "HYPPO/HDOS:");
  write_text(15, 9, 7, format_hyppo_version());

  // check for HICKUP
  write_text(40, 9, 1, "HYPPO STATUS:");
  if (read_file_from_sdcard("HICKUP.M65", 0x40000L))
    write_text(54, 9, 7, "NORMAL");
  else {
    fail = format_hickup_version(0x40000L, artix_ymd);
    write_text_upper(41, 10, 7 + fail * 3, buffer);
    if (fail)
      write_text(54, 9, 10, "OUT OF DATE HICKUP.M65");
    else
      write_text(54, 9, 10, "LOCKED BY HICKUP.M65");
  }

  // ROM version
  write_text(0, 10, 1, "ROM VERSION:");
  write_text(15, 10, 7, format_rom_version());

  // Utility versions (need to load file to parse...)
  row = 12;
  col = 0;
  for (i = 0; SDessentials[i][0] != 0; i++) {
    fail = read_file_from_sdcard(SDessentials[i], 0x40000L);
    strcpy(buffer, SDessentials[i]);
    strcat(buffer, ":");
    write_text(col, row, 1, buffer);
    if (fail)
      write_text(col + 14, row, 10, "FILE NOT FOUND");
    else {
      fail = format_util_version(0x40000L, artix_ymd);
      write_text_upper(col + 14, row, 7 + fail * 3, buffer);
    }
    if (!col)
      col = 40;
    else {
      col = 0;
      row++;
    }
  }
}

/*
 * init_megainfo
 *
 * initialize basic structures and screen
 */
void init_megainfo()
{
  isNTSC = (lpeek(0xFFD306fL) & 0x80) == 0x80;
  // fix TOD frequency
  if (isNTSC)
    lpoke(0xffd3c0el, lpeek(0xffd3c0el) & 0x7f);
  else // is PAL, set 50Hz bit
    lpoke(0xffd3c0el, lpeek(0xffd3c0el) | 0x80);

  get_rtc_stats(1); // initialise rtc data cache
  draw_screen();
}

/*
 * do_megainfo
 */
void do_megainfo()
{
  unsigned char x, rtcDEBUG = 0;

  init_megainfo();

  // clear keybuffer
  while ((x = PEEK(0xD610U)))
    POKE(0xD610U, x);

  // mainloop
  while (1) {
    x = PEEK(0xD610U);

    // update clocks
    if (get_rtc_stats(0)) {
      display_rtc_status(54, 5);
      display_rtc_debug(0, 24, 12, rtcDEBUG);
    }

    if (x == 0)
      continue;
    POKE(0xD610U, x);

    switch (x) {
    case 0xF1: // F1 - Toggle DEBUG
      rtcDEBUG = 1 - rtcDEBUG;
      display_rtc_debug(0, 24, 12, rtcDEBUG);
      break;
    case 0xF5: // F5 - REFRESH
      init_megainfo();
      break;
    case 0xF3: // F3
    case 0x1b: // ESC
    case 0x03: // RUN-STOP
      return;  // EXIT!
    }
  }

  return;
}
