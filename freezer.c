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
  
  POKE(0xD018U,0x15); // upper case

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
  
  
  while(1) {
    POKE(0x0400U,PEEK(0x0400U)+1);
  }
  
  return;
}
