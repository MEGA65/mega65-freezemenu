#include <string.h>

#include "freezer.h"
#include "freezer_common.h"
#include "fdisk_memory.h"
#include "ascii.h"

uint8_t sector_buffer[512];
unsigned short slot_number = 0;
char mega65_rom_type = 0;
char mega65_rom_name[12];

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

char *detect_rom(void)
{
  unsigned char sector[32];

  // fetch memory from current slot (so change this before calling the function!)
  freeze_fetch_sector_partial(0x20000L, (long)sector, 32);

  // Check for C65 ROM via version string
  memcpy(mega65_rom_name + 4, sector + 0x16, 7);
  if ((mega65_rom_name[4] == 'V') && (mega65_rom_name[5] == '9')) {
    if (mega65_rom_name[6] >= '2') {
      mega65_rom_name[0] = 'M';
      mega65_rom_type = MEGA65_ROM_M65;
    }
    else {
      mega65_rom_name[0] = 'C';
      mega65_rom_type = MEGA65_ROM_C65;
    }
    mega65_rom_name[1] = '6';
    mega65_rom_name[2] = '5';
    mega65_rom_name[3] = ' ';
    mega65_rom_name[11] = 0;
    return mega65_rom_name;
  }

  // OpenROM - 16 characters "OYYMMDDCC       "
  memcpy(mega65_rom_name + 4, sector + 0x10, 16);
  if ((mega65_rom_name[4] == 'O') && (mega65_rom_name[11] == '2') && (mega65_rom_name[12] == '0')
      && (mega65_rom_name[13] == ' ')) {
    mega65_rom_type = MEGA65_ROM_OPENROM;
    mega65_rom_name[0] = 'O';
    mega65_rom_name[1] = 'P';
    mega65_rom_name[2] = 'E';
    mega65_rom_name[3] = 'N';
    mega65_rom_name[4] = ' ';
    mega65_rom_name[11] = 0;
    return mega65_rom_name;
  }

#define COPY_AND_RETURN_ROM(X) { strcpy(mega65_rom_name, X); return mega65_rom_name; }

/*
  The C64 ROM part can't really work without a real C64 cpu,
  so it is save to return UNKNOWN for now

  // entering C64 region
  mega65_rom_type = MEGA65_ROM_C64;

  if (freeze_peek(0x2e47dL) == 'J') {
    // Probably jiffy dos
    if (freeze_peek(0x2e535L) == 0x06)
      COPY_AND_RETURN_ROM("SX64 JIFFY ")
    else
      COPY_AND_RETURN_ROM("C64 JIFFY  ")
  }

  // Else guess using detection routines from detect_roms.c
  // These were built using a combination of the ROMs from zimmers.net/pub/c64/firmware,
  // the RetroReplay ROM collection, and the JiffyDOS ROMs
  if (freeze_peek(0x2e449L) == 0x2e)
    COPY_AND_RETURN_ROM("C64GS      ")
  if (freeze_peek(0x2e119L) == 0xc9)
    COPY_AND_RETURN_ROM("C64 REV1   ")
  if (freeze_peek(0x2e67dL) == 0xb0)
    COPY_AND_RETURN_ROM("C64 REV2 JP")
  if (freeze_peek(0x2ebaeL) == 0x5b)
    COPY_AND_RETURN_ROM("C64 REV3 DK")
  if (freeze_peek(0x2e0efL) == 0x28)
    COPY_AND_RETURN_ROM("C64 SCAND  ")
  if (freeze_peek(0x2ebf3L) == 0x40)
    COPY_AND_RETURN_ROM("C64 SWEDEN ")
  if (freeze_peek(0x2e461L) == 0x20)
    COPY_AND_RETURN_ROM("CYCLONE 1.0")
  if (freeze_peek(0x2e4a4L) == 0x41)
    COPY_AND_RETURN_ROM("DOLPHIN 1.0")
  if (freeze_peek(0x2e47fL) == 0x52)
    COPY_AND_RETURN_ROM("DOLPHIN 2AU")
  if (freeze_peek(0x2eed7L) == 0x2c)
    COPY_AND_RETURN_ROM("DOLPHIN 2P1")
  if (freeze_peek(0x2e7d2L) == 0x6b)
    COPY_AND_RETURN_ROM("DOLPHIN 2P2")
  if (freeze_peek(0x2e4a6L) == 0x32)
    COPY_AND_RETURN_ROM("DOLPHIN 2P3")
  if (freeze_peek(0x2e0f9L) == 0xaa)
    COPY_AND_RETURN_ROM("DOLPHIN 3.0")
  if (freeze_peek(0x2e462L) == 0x45)
    COPY_AND_RETURN_ROM("DOSROM V1.2")
  if (freeze_peek(0x2e472L) == 0x20)
    COPY_AND_RETURN_ROM("MERCRY3 PAL")
  if (freeze_peek(0x2e16dL) == 0x84)
    COPY_AND_RETURN_ROM("MERCRY NTSC")
  if (freeze_peek(0x2e42dL) == 0x4c)
    COPY_AND_RETURN_ROM("PET 4064   ")
  if (freeze_peek(0x2e1d9L) == 0xa6)
    COPY_AND_RETURN_ROM("SX64 CROACH")
  if (freeze_peek(0x2eba9L) == 0x2d)
    COPY_AND_RETURN_ROM("SX64 SCAND ")
  if (freeze_peek(0x2e476L) == 0x2a)
    COPY_AND_RETURN_ROM("TRBOACS 2.6")
  if (freeze_peek(0x2e535L) == 0x07)
    COPY_AND_RETURN_ROM("TRBOACS 3P1")
  if (freeze_peek(0x2e176L) == 0x8d)
    COPY_AND_RETURN_ROM("TRBOASC 3P2")
  if (freeze_peek(0x2e42aL) == 0x72)
    COPY_AND_RETURN_ROM("TRBOPROC US")
  if (freeze_peek(0x2e4acL) == 0x81)
    COPY_AND_RETURN_ROM("C64C 251913")
  if (freeze_peek(0x2e479L) == 0x2a)
    COPY_AND_RETURN_ROM("C64 REV2   ")
  if (freeze_peek(0x2e535L) == 0x06)
    COPY_AND_RETURN_ROM("SX64 REV4  ")
*/

  // set some flags
  mega65_rom_type = MEGA65_ROM_UNKNOWN;
  COPY_AND_RETURN_ROM("UNKNOWN    ")
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

/*
 * uint8_t nybl_to_screen(uint8_t v)
 *
 * converts the lower 4 bits of a byte to a screen code
 * hexadecimal number digit.
 */
uint8_t nybl_to_screen(uint8_t v)
{
  v &= 0xf;
  if (v < 0xa)
    return 0x30 + v;
  return v - 0x9;
}

unsigned char petscii_to_screen(unsigned char petscii)
{
  // control characters => space
  if ((petscii & 0x7f) < 0x20)
    return 0x20;
  if (petscii < 0x40)
    return petscii;
  if (petscii < 0x60)
    return petscii & 0x3f;
  if (petscii < 0x80)
    return petscii & 0x5f;
  if (petscii < 0xc0)
    return petscii ^ 0xc0;
  if (petscii < 0xe0)
    return petscii & 0x5f;
  if (petscii < 0xff)
    return petscii & 0x7f;
  // want some pi?
  return 0x5e;
}

// static char* deadly_haiku[3] = { "Error consumes all", "As sand erodes rock and stone", "Now also your mind" };

void screen_of_death(char* msg)
{
  // TODO: This is broken, obviously...
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
  
  // Normal video mode (but preserve CRT emulation etc)
  POKE(0xD054U,PEEK(0xD054)&0xA8);

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
  while (1 || msg)
    continue;
}
