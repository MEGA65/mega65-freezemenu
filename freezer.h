#include <sys/types.h>
#include <ctype.h>
#include <stdint.h>

#define WITH_AUDIOMIXER
// #define WITH_TOUCH

char cdecl mega65_dos_attachd81(char *image_name);
void fetch_freeze_region_list_from_hypervisor(unsigned short);
unsigned char fastcall find_freeze_slot_start_sector(unsigned short);
void cdecl read_file_from_sdcard(char *filename,uint32_t load_address);
void unfreeze_slot(unsigned short);
unsigned char opendir(void);
struct m65_dirent *readdir(unsigned char);
void closedir(unsigned char);

void freeze_monitor(void);
char *freeze_select_disk_image(void);

void request_freeze_region_list(void);
uint32_t address_to_freeze_slot_offset(uint32_t address);
uint32_t find_thumbnail_offset(void);
unsigned char freeze_peek(uint32_t addr);
void freeze_poke(uint32_t addr,unsigned char v);
unsigned char freeze_fetch_page(uint32_t addr,unsigned char *buffer);
unsigned short get_freeze_slot_count(void);
void do_audio_mixer(void);


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
#define REGION_LENGTH_MASK 0xFFFFFF
    unsigned long region_length;  // only lower 24 bits are valid, space occupied rounded up to next 512 bytes
    struct {
      unsigned char skip[3];
      unsigned char freeze_prep;  
    };
  };
};

#define MAX_REGIONS (256 / sizeof(struct freeze_region_t) )

extern struct freeze_region_t freeze_region_list[MAX_REGIONS];
extern unsigned char freeze_region_count;
extern unsigned long freeze_slot_start_sector;


struct file_descriptor_t {
#define FD_DISK_ID_FILE_CLOSED 0xFF
  unsigned char disk_id;
  unsigned int start_cluster;
  unsigned int current_cluster;
  unsigned char sector_in_cluster;
  unsigned int file_length;
  unsigned int buffer_position;
  unsigned int directory_cluster;
  unsigned short entry_in_directory;
  unsigned int buffer_address;
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
  char filler[0x80-(1+16+1+1+1+1+32+32)];
  struct file_descriptor_t file_descriptors[4];

  // Pad out to whole sector size, so we can load it easily
  char padding[256];
};
