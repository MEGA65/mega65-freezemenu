/*
  Based on mega65-fdisk program as a starting point.

*/

#include <stdio.h>
#include <string.h>

#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

struct freeze_region_t {
  unsigned long address_base;
  union {
    unsigned long region_length;  // only lower 24 bits are valid, space occupied rounded up to next 512 bytes
    struct {
      unsigned char skip[3];
      unsigned char freeze_prep;  
    };
  };
};

#define MAX_REGIONS (256 / sizeof(struct freeze_region_t) )
struct freeze_region_t freeze_region_list[MAX_REGIONS];
unsigned char freeze_region_count=0;

uint8_t sector_buffer[512];

void clear_sector_buffer(void)
{
#ifndef __CC65__DONTUSE
  int i;
  for(i=0;i<512;i++) sector_buffer[i]=0;
#else
  lfill((uint32_t)sector_buffer,0,512);
#endif
}

unsigned char *freeze_menu=
  "         MEGA65 FREEZE MENU V0.1        "
  "     (C) FLINDERS UNI, M.E.G.A. 2018    "
  " cccccccccccccccccccccccccccccccccccccc "
  " F1 - BACKUP  F3 - RESTART  F7 - SCREEN "
  " cccccccccccccccccccccccccccccccccccccc "
  "\0";


unsigned short i;
char *deadly_haiku[3]={"Error consumes all","As sand erodes rock and stone","Now also your mind"};

unsigned char ascii_to_screencode(char c)
{
  if (c>=0x60) return c-0x60;
  return c;
}

void screen_of_death(char *msg)
{
  POKE(0,0x41);
  POKE(0xD02FU,0x47); POKE(0xD02FU,0x53);

  // Reset video mode
  POKE(0xD05DU,0x01); POKE(0xD011U,0x1b); POKE(0xD016U,0xc8);
  POKE(0xD018U,0x17); // lower case
  POKE(0xD06FU,0x80); // NTSC 60Hz mode for monitor compatibility?

  // No sprites
  POKE(0xD015U,0x00);
  
  // Normal video mode
  POKE(0xD054U,0x00);

  // Reset colour palette to normal for black and white
  POKE(0xD100U,0x00);  POKE(0xD200U,0x00);  POKE(0xD300U,0x00);
  POKE(0xD101U,0xFF);  POKE(0xD201U,0xFF);  POKE(0xD301U,0xFF);
  
  POKE(0xD020U,0); POKE(0xD021U,0);

  // Clear screen
  POKE(1,0x3f); POKE(0,0x3F);
  for(i=1024;i<2024;i++) POKE(i,' ');
  for(i=0;i<1000;i++) POKE(0xD800U+i,1);

  for(i=0;deadly_haiku[0][i];i++) POKE(0x0400+10*40+11+i,ascii_to_screencode(deadly_haiku[0][i]));
  for(i=0;deadly_haiku[1][i];i++) POKE(0x0400+12*40+11+i,ascii_to_screencode(deadly_haiku[1][i]));
  for(i=0;deadly_haiku[2][i];i++) POKE(0x0400+14*40+11+i,ascii_to_screencode(deadly_haiku[2][i]));  
  
  while(1) continue;
  
}

void fetch_freeze_region_list_from_hypervisor(unsigned short);

#ifdef __CC65__
void main(void)
#else
int main(int argc,char **argv)
#endif
{
#ifdef __CC65__
  mega65_fast();
#endif  

  // No decimal mode!
  __asm__("cld");


  // Ask hypervisor to copy out freeze region list, so we know where to look
  // in the slot for different parts of memory.
  // The transfer region MUST be in the lower 32KB of RAM, so we will copy it
  // to the screen in the first instance, and then DMA copy it where we want it
  lfill(0x0400U,0x20,1000);
  fetch_freeze_region_list_from_hypervisor(0x0400);
  lcopy(0x0400U,(unsigned long)&freeze_region_list,256);
  for(i=0;i<MAX_REGIONS;i++) {
    if (freeze_region_list[i].freeze_prep==0xFF) break;
  }
  freeze_region_count=i;
  
  POKE(0xD018U,0x15); // upper case

  // NTSC 60Hz mode for monitor compatibility?
  POKE(0xD06FU,0x80);

  // No sprites
  POKE(0xD015U,0x00);
  
  // Normal video mode
  POKE(0xD054U,0x00);

  // XXX Reset colour palette to normal
  
  // Clear screen, blue background, white text, like Action Replay
  POKE(0xD020U,6); POKE(0xD021U,6);
  
  for(i=0x0400U;i<0x07E8U;i++) POKE(i,0x20);
  for(i=0xD800U;i<0xDBE8U;i++) POKE(i,0x01);
  
  // Freezer can't use printf() etc, because C64 ROM has not started, so ZP will be a mess
  // (in fact, most of memory contains what the frozen program had. Only our freezer program
  // itself has been loaded to replace some of RAM).
  for(i=0;freeze_menu[i];i++) {
    if ((freeze_menu[i]>='A')&&(freeze_menu[i]<='Z'))
      POKE(0x0400U+i,freeze_menu[i]-0x40);
    else if ((freeze_menu[i]>='a')&&(freeze_menu[i]<='z'))
      POKE(0x0400U+i,freeze_menu[i]-0x20);
    else
      POKE(0x0400U+i,freeze_menu[i]);
  }

  // Flush input buffer
  while (PEEK(0xD610U)) POKE(0xD610U,0);
  
  // Main keyboard input loop
  while(1) {
    //    POKE(0xD020U,PEEK(0xD020U)+1);
    if (PEEK(0xD610U)) {
      // Process char
      switch(PEEK(0xD610U)) {
      case 0xf1: // F1 = backup
	break;
      case 0xf3: // F3 = resume
	// Load memory from freeze slot $0000, i.e., the temporary save space
	// This implicitly restarts the frozen program
	__asm__("LDX #<$0000");
	__asm__("LDY #>$0000");
	__asm__("LDA #$12");
	__asm__("STA $D642");
	__asm__("NOP");

	// should never get here
	screen_of_death("unfreeze failed");
	
	break;
      case 0xf7: // F7 = show screen of frozen program
	// XXX for now just show we read the key
	POKE(0xD020U,PEEK(0xD020U)+1);
	break;
      }
      
      // Flush char from input buffer
      POKE(0xD610,0);
    }
  }
  
  return;
}
