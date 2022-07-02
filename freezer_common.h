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

void set_palette(void);
char* detect_rom(void);
unsigned char detect_cpu_speed(void);

#endif /* __FREEZER_COMMON_H__ */