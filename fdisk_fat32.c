#include <stdio.h>
#include <string.h>

#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "ascii.h"

unsigned long root_dir_sector=0;
unsigned long fat1_sector=0;
unsigned long fat2_sector=0;
unsigned short reserved_sectors=0;
unsigned char sectors_per_cluster=0;
unsigned char fat_copies=0;
unsigned long sectors_per_fat=0;
unsigned long root_dir_cluster=0;

extern unsigned char sector_buffer[512];

void sdcard_readsector(const uint32_t sector_number);


void parse_partition_entry(const char i)
{
  char j;
  
  int offset=0x1be + (i<<4);

  char active=sector_buffer[offset+0];
  char shead=sector_buffer[offset+1];
  char ssector=sector_buffer[offset+2]&0x1f;
  int scylinder=((sector_buffer[offset+2]<<2)&0x300)+sector_buffer[offset+3];
  char id=sector_buffer[offset+4];
  char ehead=sector_buffer[offset+5];
  char esector=sector_buffer[offset+6]&0x1f;
  int ecylinder=((sector_buffer[offset+6]<<2)&0x300)+sector_buffer[offset+7];
  uint32_t lba_start,lba_size;

  for(j=0;j<4;j++) ((char *)&lba_start)[j]=sector_buffer[offset+8+j];
  for(j=0;j<4;j++) ((char *)&lba_size)[j]=sector_buffer[offset+12+j];

  switch (id) {
  case 0x0b: case 0x0c:
    // FAT32
    // lba_start has start of partition
    sdcard_readsector(lba_start);
    // reserved sectors @ $00e-$00f
    reserved_sectors=sector_buffer[0x0e]+(sector_buffer[0x0f]<<8L);
    // sectors per cluster @ $00d
    sectors_per_cluster=sector_buffer[0x0d];
    // number of FATs @ $010
    fat_copies=sector_buffer[0x10];
    // hidden sectors @ $01c-$01f
    // sectors per FAT @ $024-$027
    for(j=0;j<4;j++) ((char *)&sectors_per_fat)[j]=sector_buffer[0x24+j];
    // cluster of root directort @ $02c-$02f
    for(j=0;j<4;j++) ((char *)&root_dir_cluster)[j]=sector_buffer[0x2c+j];
    // $55 $AA signature @ $1fe-$1ff    
    
    // FATs begin at partition + reserved sectors
    // root dir = cluster 2 begins after 2nd FAT
    root_dir_sector=lba_start + reserved_sectors + sectors_per_fat * fat_copies;
    fat1_sector=lba_start + reserved_sectors;
    fat2_sector=lba_start + reserved_sectors + sectors_per_fat;
    
  }
  
#if 0
  printf("%02X%c : Start=%3d/%2d/%4d or %08X / End=%3d/%2d/%4d or %08X\n",
	 id,active&80?'*':' ',
	 shead,ssector,scylinder,lba_start,ehead,esector,ecylinder,lba_end);
#endif
  
}


unsigned char fat32_open_file_system(void)
{
  unsigned char i;
  sdcard_readsector(0);
  if ((sector_buffer[0x1fe]!=0x55)||(sector_buffer[0x1ff]!=0xAA)) {
    return 255;
  } else {  
    for(i=0;i<4;i++) {
      parse_partition_entry(i);
    }
  }
  
}

/*
  Create a file in the root directory of the new FAT32 filesystem
  with the indicated name and size.

  The file will be created contiguous on disk, and the first
  sector of the created file returned.

  The root directory is the start of cluster 2, and clusters are
  assumed to be 4KB in size, to keep things simple.
*/
long fat32_create_contiguous_file(char* name, long size, long root_dir_sector, long fat1_sector, long fat2_sector)
{
  unsigned char i;
  unsigned short offset;
  unsigned short clusters;
  unsigned long start_cluster = 0;
  unsigned long next_cluster;
  unsigned long contiguous_clusters = 0;
  char message[40] = "Found file: ????????.???";

  clusters = size / 4096;

  sdcard_readsector(fat1_sector);
  for (offset = 0; offset < 512; offset += 4) {
    next_cluster = sector_buffer[offset];
    next_cluster |= ((long)sector_buffer[offset + 1] << 8L);
    next_cluster |= ((long)sector_buffer[offset + 2] << 16L);
    next_cluster |= ((long)sector_buffer[offset + 3] << 24L);
    screen_decimal(screen_line_address - 80 + 8, offset / 4);
    screen_hex(screen_line_address - 80 + 32, next_cluster);
    if (!next_cluster) {
      if (!start_cluster) {
        start_cluster = offset / 4;
      }
      contiguous_clusters++;
      if (contiguous_clusters == clusters) {
        // End of chain marker
        sector_buffer[offset + 0] = 0xff;
        sector_buffer[offset + 1] = 0xff;
        sector_buffer[offset + 2] = 0xff;
        sector_buffer[offset + 3] = 0x0f;
        break;
      }
      else {
        // Point to next cluster
        sector_buffer[offset + 0] = (offset / 4) + 1;
        sector_buffer[offset + 1] = 0;
        sector_buffer[offset + 2] = 0;
        sector_buffer[offset + 3] = 0;
      }
    }
    else {
      if (start_cluster) {
        write_line("ERROR: Disk space is fragmented. File not created.", 0);
        return 0;
      }
    }
  }
  if ((!start_cluster) || (contiguous_clusters != clusters)) {
    write_line("ERROR: Could not find enough free clusters early in file system", 0);
    return 0;
  }
  write_line("First free cluster is ", 0);
  screen_decimal(screen_line_address - 80 + 22, start_cluster);

  // Commit sector to disk (in both copies of FAT)
  sdcard_writesector(fat1_sector, 0);
  sdcard_writesector(fat2_sector, 0);

  sdcard_readsector(root_dir_sector);

  for (offset = 0; offset < 512; offset += 32) {
    for (i = 0; i < 8; i++)
      message[12 + i] = sector_buffer[offset + i];
    for (i = 0; i < 3; i++)
      message[21 + i] = sector_buffer[offset + 8 + i];
    if (message[12] > ' ')
      write_line(message, 0);
    else
      break;
  }
  if (offset == 512) {
    write_line("ERROR: First sector of root directory already full.", 0);
    return 0;
  }

  // Build directory entry
  for (i = 0; i < 32; i++)
    sector_buffer[offset + i] = 0x00;
  for (i = 0; i < 12; i++)
    sector_buffer[offset + i] = name[i];
  sector_buffer[offset + 0x0b] = 0x20; // Archive bit set
  sector_buffer[offset + 0x1A] = start_cluster;
  sector_buffer[offset + 0x1B] = start_cluster >> 8;
  sector_buffer[offset + 0x14] = start_cluster >> 16;
  sector_buffer[offset + 0x15] = start_cluster >> 24;
  sector_buffer[offset + 0x1C] = (size >> 0) & 0xff;
  sector_buffer[offset + 0x1D] = (size >> 8L) & 0xff;
  sector_buffer[offset + 0x1E] = (size >> 16L) & 0xff;
  sector_buffer[offset + 0x1F] = (size >> 24l) & 0xff;

  sdcard_writesector(root_dir_sector, 0);

  return root_dir_sector + (start_cluster - 2) * 8;
}
