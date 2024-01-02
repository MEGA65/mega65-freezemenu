#ifndef __FREEZER_H__
#define __FREEZER_H__

#include <sys/types.h>
#include <ctype.h>
#include <stdint.h>

#define WITH_AUDIOMIXER
// #define WITH_TOUCH

unsigned char mega65_geterrorcode(void);
char cdecl mega65_dos_chdir(unsigned char* dirname);
char cdecl mega65_dos_cdroot();
char cdecl mega65_dos_d81attach0(char* image_name);
char cdecl mega65_dos_d81attach1(char* image_name);
char cdecl mega65_dos_exechelper(char* filename);
void fetch_freeze_region_list_from_hypervisor(unsigned short);
unsigned char find_freeze_slot_start_sector(unsigned short);
char cdecl read_file_from_sdcard(char* filename, uint32_t load_address);
void unfreeze_slot(unsigned short);
unsigned char opendir(void);
struct m65_dirent* readdir(unsigned char);
void closedir(unsigned char);
void closeall(void);

void freeze_monitor(void);

#define INTERNAL_DRIVE_0 "- INTERNAL 3.5\" -   "
#define INTERNAL_DRIVE_1 "- 1565 DRIVE 1 -    "
char* freeze_select_disk_image(unsigned char drive_id);

void request_freeze_region_list(void);
uint32_t address_to_freeze_slot_offset(uint32_t address);
uint32_t find_thumbnail_offset(void);
unsigned char freeze_peek(uint32_t addr);
void freeze_poke(uint32_t addr, unsigned char v);
unsigned char freeze_fetch_sector(uint32_t addr, unsigned char* buffer);
unsigned char freeze_fetch_sector_partial(uint32_t addr, uint32_t dest, unsigned int count);
unsigned char freeze_store_sector(uint32_t addr, unsigned char* buffer);
unsigned char freeze_store_sector_partial(uint32_t addr, uint32_t src, unsigned int count);
unsigned short get_freeze_slot_count(void);
void do_audio_mixer(void);
void do_sprite_editor(void);
unsigned char do_rom_loader(void);
void do_megainfo(void);

struct m65_dirent {
  uint32_t d_ino;
  uint16_t d_off;
  uint32_t d_reclen;
  uint16_t d_type;
  char d_name[256];
};

struct freeze_region_t {
  unsigned long address_base;
  union {
#define REGION_LENGTH_MASK 0x7FFFFF
    unsigned long region_length; // only lower 24 bits are valid, space occupied rounded up to next 512 bytes
    struct {
      unsigned char skip[3];
      unsigned char freeze_prep;
    };
  };
};

#define MAX_REGIONS (256 / sizeof(struct freeze_region_t))

extern unsigned char not_in_root;
extern struct freeze_region_t freeze_region_list[MAX_REGIONS];
extern unsigned char freeze_region_count;

#define FREEZE_REGION_HAS_CHARGEN 0x01
extern unsigned char freeze_region_flags;

extern unsigned long freeze_slot_start_sector;

struct file_descriptor_t {
#define FD_DISK_ID_FILE_CLOSED 0xFF
  unsigned char disk_id;
  unsigned long start_cluster;
  unsigned long current_cluster;
  unsigned char sector_in_cluster;
  unsigned long file_length;
  unsigned long buffer_position;
  unsigned long directory_cluster;
  unsigned short entry_in_directory;
  unsigned long buffer_address;
  unsigned short bytes_in_buffer;
  unsigned short offset_in_buffer;
};

struct process_descriptor_t {
  unsigned char task_id;
  char process_name[16];
  unsigned char d81_image0_flags;
  unsigned char d81_image1_flags;
  unsigned char d81_image0_namelen;
  unsigned char d81_image1_namelen;
  char d81_image0_name[32];
  char d81_image1_name[32];
  char filler[0x80 - (1 + 16 + 1 + 1 + 1 + 1 + 32 + 32)];
  struct file_descriptor_t file_descriptors[4];

  // Pad out to whole sector size, so we can load it easily
  char padding[256];
};

#endif /* __FREEZER_H__ */