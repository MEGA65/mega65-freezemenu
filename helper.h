#ifndef __HELPER_H__
#define __HELPER_H__

#include <stdint.h>

extern uint8_t hdos_new_attach;

unsigned char mega65_geterrorcode(void);

void init_nmi(void);

// initializes variables for attach call
// returns 0 if new dos_attach is not available, 1 otherwise
uint8_t mega65_dos_init(void);

char cdecl mega65_dos_attach(char *image_name, uint8_t driveid);
#define M65_DOS_ATTACH_NODRIVE 0b01000000
char mega65_dos_detach(uint8_t driveid_and_flags);

char cdecl mega65_dos_chdir(unsigned char *dirname);
char cdecl mega65_dos_cdroot();

char cdecl mega65_dos_exechelper(char *filename);

uint8_t mega65_dos_getprocdesc(uint8_t pagemsb);

unsigned short get_freeze_slot_count(void);
void fetch_freeze_region_list_from_hypervisor(unsigned short);
unsigned char find_freeze_slot_start_sector(unsigned short);
void unfreeze_slot(unsigned short);

char cdecl read_file_from_sdcard(char *filename, uint32_t load_address);

unsigned char opendir(void);
struct m65_dirent *readdir(unsigned char);
void closedir(unsigned char);
void closeall(void);

#endif /* __HELPER_H__ */