#include "freezer.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

struct freeze_region_t freeze_region_list[MAX_REGIONS];
unsigned char freeze_region_count=0;

unsigned long freeze_slot_start_sector=0;

void request_freeze_region_list(void)
{
  // Ask hypervisor to copy out freeze region list, so we know where to look
  // in the slot for different parts of memory.
  // The transfer region MUST be in the lower 32KB of RAM, so we will copy it
  // to the screen in the first instance, and then DMA copy it where we want it
  unsigned short i;
  lfill(0x0400U,0x20,1000);
  fetch_freeze_region_list_from_hypervisor(0x0400);
  lcopy(0x0400U,(unsigned long)&freeze_region_list,256);
  for(i=0;i<MAX_REGIONS;i++) {
    if (freeze_region_list[i].freeze_prep==0xFF) break;
  }
  freeze_region_count=i;
}

/* Convert a requested address to a location in the freeze slot,
   or to 0xFFFFFFFF if the address is not present.
*/
uint32_t address_to_freeze_slot_offset(uint32_t address)
{
  uint32_t freeze_slot_offset=1;  // Skip the initial saved SD sector at the beginning of each slot
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

unsigned char freeze_peek(uint32_t addr)
{
  // Find sector
  uint32_t freeze_slot_offset=address_to_freeze_slot_offset(addr);
  unsigned short value;

  freeze_slot_offset=freeze_slot_offset>>9L;
  
  if (freeze_slot_offset==0xFFFFFFFFL) {
    // Invalid / unfrozen memory
    return 0x55;
  }

  // XXX - We should cache sectors
  
  // Read the sector
  sdcard_readsector(freeze_slot_start_sector+freeze_slot_offset);

  // Return the byte
  return sector_buffer[freeze_slot_offset&0x1ff];

}

