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

  XXX -- Should allow creation of files > 128 clusters long
  XXX -- Should allow creation of files in sub-directories
  XXX -- Should allow multi-cluster directories
  XXX -- Should allow extending of directory if last cluster of directory is already full

*/
long fat32_create_contiguous_file(char* name, long size, long root_dir_sector, long fat1_sector, long fat2_sector)
{
  unsigned char i,sn,len;
  unsigned short offset,j;
  unsigned short clusters;
  unsigned long start_cluster = 0;
  unsigned long dir_cluster = 2;
  unsigned long last_dir_cluster = 2;
  unsigned long next_cluster;
  unsigned long contiguous_clusters = 0;
  unsigned long fat_sector_num=0;
  
  unsigned long free_dir_sector_num=0;
  unsigned short free_dir_sector_ofs=0;
  
  char message[40] = "Found file: ????????.???";
  
  clusters = size / (512 * sectors_per_cluster);
  if (size%(512*sectors_per_cluster)) clusters++;
      
  // Look for a free directory slot.
  // Also complain if the file already exists
  while(dir_cluster>=2&&dir_cluster<0xf0000000) {
    for (sn=0;sn<sectors_per_cluster;sn++) {
      sdcard_readsector(root_dir_sector+((dir_cluster-2)*sectors_per_cluster)+sn);
      for (offset = 0; offset < 512; offset += 32) {
	for (i = 0; i < 8; i++)
	  message[i] = sector_buffer[offset + i];
	len=8;
	while(len&&(message[len]==' '||message[len]==0)) len--;
	message[len++]='.';
	for (i = 0; i < 3; i++)
	  message[len + i] = sector_buffer[offset + 8 + i];
	len+=3;
	while(len&&(message[len]==' '||message[len]==0)) len--;
	if (!strcmp(message,name)) {
	  // ERROR: Name already exists
	  return 0;
	}
	if (sector_buffer[offset]==0) {
	  free_dir_sector_num=root_dir_sector+sn;
	  free_dir_sector_ofs=offset;
	  break;
	}
      }
    }
    // Stop once we have found a free directory slot
    if (free_dir_sector_num) break;

    // Chain to next directory cluster, and extend directory
    // if required.
    last_dir_cluster=dir_cluster;
    dir_cluster = fat32_follow_cluster(dir_cluster);
    if ((!dir_cluster)||(dir_cluster>=0xf0000000)) {
      // End of directory -- allocate new cluster
      // XXX - Not implemented
      dir_cluster=fat32_allocate_cluster(last_dir_cluster);

      if ((!dir_cluster)||(dir_cluster>=0xf0000000)) {
	// Disk full
	return 0;
      } else {
	// Zero out new directory cluster
	lfill(sector_buffer,0,512);
	for (sn=0;sn<sectors_per_cluster;sn++) {
	  sdcard_readsector(root_dir_sector+((dir_cluster-2)*sectors_per_cluster)+sn);
	}
      }
    }
  }
      
  // Find where we have enough contiguous space
  contiguous_clusters=0;
  start_cluster=0;
  for(fat_sector_num=0;fat_sector_num <= (fat2_sector-fat1_sector); fat_sector_num++) {

    // This can take a while if the disk is full, because we use a naive search.
    // So show the user that something is happening.
    POKE(0xD020,PEEK(0xD020+1));
    
    sdcard_readsector(fat1_sector+fat_sector_num);

    // Skip any FAT sectors with allocated clusters
    for(j=0;j<512;j++) if (sector_buffer[j]) break;
    if (j!=512) {
      // Reset count of contiguous clusters
      contiguous_clusters=0;
      continue;
    } else {
      // Start from here
      if (!contiguous_clusters) start_cluster=fat_sector_num*128;
      contiguous_clusters+=128;
    }
    if (contiguous_clusters>=clusters) break;
  }

  // Abort if the disk is full
  if (contiguous_clusters<clusters) return 0;

  // Write cluster chain into both FATs
  fat_sector_num=start_cluster/128;
  next_cluster=start_cluster+1;
  while(contiguous_clusters) {
    contiguous_clusters--;
    lfill(sector_buffer,0,512);
    for (offset = 0; offset < 512; offset += 4) {
      if (!next_cluster) {
	if (!start_cluster) {
	  start_cluster = offset / 4;
	}
	contiguous_clusters++;
	if (!contiguous_clusters) {
	  // End of chain marker
	  sector_buffer[offset + 0] = 0xff;
	  sector_buffer[offset + 1] = 0xff;
	  sector_buffer[offset + 2] = 0xff;
	  sector_buffer[offset + 3] = 0x0f;
	  break;
	}
	else {
	  // Point to next cluster
	  sector_buffer[offset + 0] = (next_cluster>>0);
	  sector_buffer[offset + 1] = (next_cluster>>8);
	  sector_buffer[offset + 2] = (next_cluster>>16);
	  sector_buffer[offset + 3] = (next_cluster>>24);
	}
      }
    }
    sdcard_writesector(fat1_sector+fat_sector_num, 0);
    sdcard_writesector(fat2_sector+fat_sector_num, 0);    
  }
  //  write_line("First free cluster is ", 0);
  //  screen_decimal(screen_line_address - 80 + 22, start_cluster);


  // Build directory entry
  sdcard_readsector(free_dir_sector_num);
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

  sdcard_writesector(free_dir_sector_num,0);

  return root_dir_sector + (start_cluster - 2) * 8;
}
