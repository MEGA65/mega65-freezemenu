#ifndef __FREEZER_COMMON_H__
#define __FREEZER_COMMON_H__

#include "ascii.h"

// Used to quickly return from functions if a navigation key has been pressed
// (used to avoid delays when navigating through the list of freeze slots
#define NAVIGATION_KEY_CHECK()                                                                                              \
  {                                                                                                                         \
    if (((PEEK(0xD610U) & 0x7f) == 0x11) || ((PEEK(0xD610U) & 0x7f) == 0x1D))                                               \
      return;                                                                                                               \
  }

extern uint8_t sector_buffer[512];
extern unsigned short slot_number;

#define CHARGEN_ADDRESS 0xFF7E000L

// this variable is set by detect_rom()
#define MEGA65_ROM_UNKNOWN 0
#define MEGA65_ROM_C64     1
#define MEGA65_ROM_C65     2
#define MEGA65_ROM_M65     3
#define MEGA65_ROM_OPENROM 4
extern char mega65_rom_type;
extern char mega65_rom_name[];

void set_palette(void);
char* detect_rom(void);
unsigned char detect_cpu_speed(void);
uint8_t nybl_to_screen(uint8_t v);
unsigned char petscii_to_screen(unsigned char petscii);

#endif /* __FREEZER_COMMON_H__ */