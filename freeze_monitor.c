/*
  Memory monitor for the freezer.

  We know where the freeze regions are from freeze_region_list[],
  so we can convert requested addresses into offsets in the freeze slot.

  We can also allow viewing/modification of the raw freeze slot data itself.

*/

#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

/* Convert a requested address to a location in the freeze slot,
   or to 0xFFFFFFFF if the address is not present.
*/
uint32_t address_to_freeze_slot_offset(uint32_t address)
{
  uint32_t freeze_slot_offset=0;
  uint32_t relative_address=0;
  uint32_t region_length=0;
  char skip,i;
  
  for(i=0;i<freeze_region_count;i++) {
    skip=0;
    if (address<freeze_region_list[i].address_base) skip=1;
    relative_address=address-freeze_region_list[i].address_base;
    region_length=freeze_region_list[i].region_length&REGION_LENGTH_MASK;
    if (relative_address>=region_length) skip=1;
    if (skip) {
      // Skip this region if our address is not in it
      freeze_slot_offset+=region_length>>9;
      // If region is not an integer number of sectors long, don't forget to count the partial sector
      if (region_length&0x1ff) freeze_slot_offset++;
    } else {
      // The address is in this region.

      // Firsts add the number of sectors to get to the one with the content we want
      freeze_slot_offset+=relative_address>>9;

      // Now multiply it by the length of a sector (512 bytes), and add the offset in the sector
      // This gives us the absolute byte position in the slot of the address we want.
      freeze_slot_offset=freeze_slot_offset<<9;
      freeze_slot_offset+=(relative_address&0x1FF);
      return freeze_slot_offset;
    }
  }
  return 0xFFFFFFFFL;
}

unsigned char screen_line=0;
unsigned char screen_line_buffer[80];
unsigned char screen_line_length=0;

void freeze_monitor(void)
{

  setup_screen();

  // Flush input buffer
  while (PEEK(0xD610U)) POKE(0xD610U,0);
  
  while(1)
    {
      read_line(screen_line_buffer,80);
      screen_line_buffer[79]=0;
      write_line(screen_line_buffer,0);
    }
  
  
}
