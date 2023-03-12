#ifndef __FDISK_FAT32_H__
#define __FDISK_FAT32_H__

extern unsigned long root_dir_sector;
extern unsigned long fat1_sector;
extern unsigned long fat2_sector;

long fat32_create_contiguous_file(char* name, long size, long root_dir_sector, long fat1_sector, long fat2_sector);
unsigned char fat32_open_file_system(void);

#endif /* __FDISK_FAT32_H__ */