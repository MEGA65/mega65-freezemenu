#include "freezer.h"
#include "fdisk_memory.h"
#include "ascii.h"

// clang-format off
static unsigned char c64_palette[64]={
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

void set_palette(void)
{
  unsigned char c;

  // set palette selector
  POKE(0xD070U, 0xFF);

  // First set the 16 C64 colours
  for (c = 0; c < 16; c++) {
    POKE(0xD100U + c, c64_palette[c * 4 + 0]);
    POKE(0xD200U + c, c64_palette[c * 4 + 1]);
    POKE(0xD300U + c, c64_palette[c * 4 + 2]);
  }

  // Then prepare a colour cube in the rest of the palette
  for (c = 16; c; c++) {
    // 3 bits for red
    POKE(0xD100U + c, (c >> 4) & 0xe);
    // 3 bits for green
    POKE(0xD200U + c, (c >> 1) & 0xe);
    // 2 bits for blue
    POKE(0xD300U + c, (c << 2) & 0xf);
  }
}

static char mega65_rom_name[12];

char* detect_rom(void)
{
  // Check for C65 ROM via version string
  lcopy(0x20016L, (long)mega65_rom_name + 4, 7);
  if ((mega65_rom_name[4] == 'V') && (mega65_rom_name[5] == '9')) {
    if (mega65_rom_name[6] >= '2')
      mega65_rom_name[0] = 'M';
    else
      mega65_rom_name[0] = 'C';
    mega65_rom_name[1] = '6';
    mega65_rom_name[2] = '5';
    mega65_rom_name[3] = ' ';
    mega65_rom_name[11] = 0;
    return mega65_rom_name;
  }

  // OpenROM - 16 characters "OYYMMDDCC       "
  lcopy(0x20010L, (long)mega65_rom_name + 4, 16);
  if ((mega65_rom_name[4] == 'O') && (mega65_rom_name[11] == '2') && (mega65_rom_name[12] == '0')
      && (mega65_rom_name[13] == ' ')) {
    mega65_rom_name[0] = 'O';
    mega65_rom_name[1] = 'P';
    mega65_rom_name[2] = 'E';
    mega65_rom_name[3] = 'N';
    mega65_rom_name[4] = ' ';
    mega65_rom_name[11] = 0;
    return mega65_rom_name;
  }

  if (freeze_peek(0x2e47dL) == 'J') {
    // Probably jiffy dos
    if (freeze_peek(0x2e535L) == 0x06)
      return "SX64 JIFFY ";
    else
      return "C64 JIFFY  ";
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

uint8_t sector_buffer[512];
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
