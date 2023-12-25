#include <stdio.h>

#include "freezer.h"
#include "freezer_common.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

struct freeze_region_t freeze_region_list[MAX_REGIONS];
unsigned char freeze_region_count = 0;
unsigned char freeze_region_flags = 0;

unsigned long freeze_slot_start_sector = 0;

void request_freeze_region_list(void)
{
  // Ask hypervisor to copy out freeze region list, so we know where to look
  // in the slot for different parts of memory.
  // The transfer region MUST be in the lower 32KB of RAM, so we will copy it
  // to the screen in the first instance, and then DMA copy it where we want it
  unsigned short i;
  lfill(0x0400U, 0x20, 1000);
  fetch_freeze_region_list_from_hypervisor(0x0400);
  lcopy(0x0400U, (unsigned long)&freeze_region_list, 256);

  freeze_region_flags = 0;
  for (i = 0; i < MAX_REGIONS; i++) {
    if (freeze_region_list[i].address_base == CHARGEN_ADDRESS)
      freeze_region_flags |= FREEZE_REGION_HAS_CHARGEN;
    if (freeze_region_list[i].freeze_prep == 0xFF)
      break;
  }
  freeze_region_count = i;
}

uint32_t find_thumbnail_offset(void)
{
  uint32_t freeze_slot_offset = 1; // Skip the initial saved SD sector at the beginning of each slot
  uint32_t region_length = 0;
  char i;

  for (i = 0; i < freeze_region_count; i++) {
    region_length = freeze_region_list[i].region_length & REGION_LENGTH_MASK;
    /* Thumbnail freezing has changed recently:
       Previously the thumbnail was accessed indirectly, and had to be extracted to $1000 first,
       and then frozen from there, and thus appeared to be at $1000.  
       Now the thumbnail is direct mapped at $FFD4000, and is frozen directly from there.
       For now, we will check for both.
    */
    if (freeze_region_list[i].address_base == 0x1000L        // older bitstreams use relocated address
	|| freeze_region_list[i].address_base == 0xffd4000L  // newer bitstreams use real address
	) {
      // Found it
      return freeze_slot_offset;
    }

    // Skip this region if our address is not in it
    freeze_slot_offset += region_length >> 9;
    // If region is not an integer number of sectors long, don't forget to count the partial sector
    if (region_length & 0x1ff)
      freeze_slot_offset++;
  }
  return 0xFFFFFFFFL;
}

/* Convert a requested address to a location in the freeze slot,
   or to 0xFFFFFFFF if the address is not present.
*/
uint32_t address_to_freeze_slot_offset(uint32_t address)
{
  uint32_t freeze_slot_offset = 1; // Skip the initial saved SD sector at the beginning of each slot
  uint32_t relative_address = 0;
  uint32_t region_length = 0;
  char skip, i;

  for (i = 0; i < freeze_region_count; i++) {
    skip = 0;
    if (address < freeze_region_list[i].address_base)
      skip = 1;
    relative_address = address - freeze_region_list[i].address_base;
    if (freeze_region_list[i].address_base == 0x1000L) {
      // Thumbnail region: Treat specially so that we can examine it
      // We give the fictional mapping of $FF54xxx
      if ((address & 0xFFFF000L) == 0xFF54000L) {
        relative_address = address & 0xFFF;
        freeze_slot_offset = freeze_slot_offset << 9;
        freeze_slot_offset += (relative_address & 0xFFF);
        return freeze_slot_offset;
      }
    }
    region_length = freeze_region_list[i].region_length & REGION_LENGTH_MASK;
    if (relative_address >= region_length)
      skip = 1;
    if (skip) {
      // Skip this region if our address is not in it
      freeze_slot_offset += region_length >> 9;
      // If region is not an integer number of sectors long, don't forget to count the partial sector
      if (region_length & 0x1ff)
        freeze_slot_offset++;
    }
    else {
      // The address is in this region.

      // Firsts add the number of sectors to get to the one with the content we want
      freeze_slot_offset += relative_address >> 9;

      // Now multiply it by the length of a sector (512 bytes), and add the offset in the sector
      // This gives us the absolute byte position in the slot of the address we want.
      freeze_slot_offset = freeze_slot_offset << 9;
      freeze_slot_offset += (relative_address & 0x1FF);
      return freeze_slot_offset;
    }
  }
  return 0xFFFFFFFFL;
}

unsigned char freeze_peek(uint32_t addr)
{
  // Find sector
  uint32_t freeze_slot_offset = address_to_freeze_slot_offset(addr);
  unsigned short offset;

  if (freeze_slot_offset == 0xFFFFFFFFL) {
    // Invalid / unfrozen memory
    return 0x55;
  }

  offset = freeze_slot_offset & 0x1ff;
  freeze_slot_offset = freeze_slot_offset >> 9L;

  // XXX - We should cache sectors

  // Read the sector
  sdcard_readsector(freeze_slot_start_sector + freeze_slot_offset);

  // Return the byte
  return sector_buffer[offset & 0x1ff];
}

unsigned char freeze_fetch_sector(uint32_t addr, unsigned char* buffer)
{
  // Find sector
  uint32_t freeze_slot_offset = address_to_freeze_slot_offset(addr);
  unsigned short offset;

  if (freeze_slot_offset == 0xFFFFFFFFL) {
    // Invalid / unfrozen memory
    return 0x55;
  }

  offset = freeze_slot_offset & 0x1ff;
  freeze_slot_offset = freeze_slot_offset >> 9L;

  // XXX - We should cache sectors

  // Read the sector
  sdcard_readsector(freeze_slot_start_sector + freeze_slot_offset);

  // Copy the sector
  if (buffer != NULL)
    lcopy((long)&sector_buffer[offset], (long)buffer, 512 - offset);

  return 0;
}

unsigned char freeze_fetch_sector_partial(uint32_t addr, uint32_t dest, unsigned int count)
{
  // Find sector
  uint32_t freeze_slot_offset = address_to_freeze_slot_offset(addr);
  unsigned short offset;

  if (freeze_slot_offset == 0xFFFFFFFFL) {
    // Invalid / unfrozen memory
    return 0x55;
  }

  if (count > 512) {
    // sector size exceeded
    return 0x56;
  }
  offset = freeze_slot_offset & 0x1ff;
  freeze_slot_offset = freeze_slot_offset >> 9L;

  // XXX - We should cache sectors

  // Read the sector
  sdcard_readsector(freeze_slot_start_sector + freeze_slot_offset);

  // Copy fetched data to dest address
  lcopy((long)&sector_buffer[offset], dest, count);

  return 0;
}

unsigned char freeze_store_sector(uint32_t addr, unsigned char* buffer)
{
  // Find sector
  uint32_t freeze_slot_offset = address_to_freeze_slot_offset(addr);
  unsigned short offset;

  if (freeze_slot_offset == 0xFFFFFFFFL) {
    // Invalid / unfrozen memory
    return 0x55;
  }

  offset = freeze_slot_offset & 0x1ff;
  freeze_slot_offset = freeze_slot_offset >> 9L;

  // if this is no full sector store, we need to get that sector first
  if (offset > 0) {
    // Read the sector for modification
    sdcard_readsector(freeze_slot_start_sector + freeze_slot_offset);
  }

  if (buffer != NULL)
    lcopy((long)buffer, (long)&sector_buffer[offset], 512 - offset); // don't write behind the buffer!

  // Write the sector
  sdcard_writesector(freeze_slot_start_sector + freeze_slot_offset, 0);

  return 0;
}

unsigned char freeze_store_sector_partial(uint32_t addr, uint32_t src, unsigned int count)
{

  // Find sector
  uint32_t freeze_slot_offset = address_to_freeze_slot_offset(addr);
  unsigned short offset;

  if (freeze_slot_offset == 0xFFFFFFFFL) {
    // Invalid / unfrozen memory
    return 0x55;
  }

  if (count > 512) {
    // sector size exceeded
    return 0x56;
  }
  offset = freeze_slot_offset & 0x1ff;
  freeze_slot_offset = freeze_slot_offset >> 9L;

  // if this is no full sector store, we need to get that sector first
  if (count != 512 || offset != 0) {
    // Read the sector for modification
    sdcard_readsector(freeze_slot_start_sector + freeze_slot_offset);
  }

  lcopy(src, (long)&sector_buffer[offset], count);

  // Write the sector
  sdcard_writesector(freeze_slot_start_sector + freeze_slot_offset, 0);

  return 0;
}

void freeze_poke(uint32_t addr, unsigned char v)
{
  // Find sector
  uint32_t freeze_slot_offset = address_to_freeze_slot_offset(addr);
  unsigned short offset;

  if (freeze_slot_offset == 0xFFFFFFFFL) {
    // Invalid / unfrozen memory
    return;
  }

  offset = freeze_slot_offset & 0x1ff;
  freeze_slot_offset = freeze_slot_offset >> 9L;

  // XXX - We should cache sectors

  // Read the sector
  sdcard_readsector(freeze_slot_start_sector + freeze_slot_offset);

  // Set the byte
  sector_buffer[offset & 0x1ff] = v;

  // Write sector back
  sdcard_writesector(freeze_slot_start_sector + freeze_slot_offset, 0);
}
