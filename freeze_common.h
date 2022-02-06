/*
 * common freezer like tools stuff
 */
#include <sys/types.h>
#include <ctype.h>
#include <stdint.h>

#include "fdisk_memory.h"

/*
 * MACROS
 */

// Used to quickly return from functions if a navigation key has been pressed
// (used to avoid delays when navigating through the list of freeze slots
#define NAVIGATION_KEY_CHECK()                                                                                              \
  {                                                                                                                         \
    if (((PEEK(0xD610U) & 0x7f) == 0x11) || ((PEEK(0xD610U) & 0x7f) == 0x1D))                                               \
      return;                                                                                                               \
  }

extern char *deadly_haiku[3];

extern uint8_t sector_buffer[512];
extern unsigned short slot_number;
void clear_sector_buffer(void);

extern unsigned char c64_palette[64];
extern unsigned char colour_table[256];
void set_palette(void);

unsigned char ascii_to_screencode(char c);

/*
 * Touch UI stuff
 */
extern signed char swipe_dir;
