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


#ifdef __CC65__
void main(void)
#else
int main(int argc,char **argv)
#endif
{
#ifdef __CC65__
  mega65_fast();
#endif  

  while(1) {
    POKE(0x0400U,PEEK(0x0400U)+1);
  }
  
  return;
}
