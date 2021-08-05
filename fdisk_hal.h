#include <sys/types.h>
#include <ctype.h>
#include <stdint.h>

extern unsigned char sdhc_card;
extern uint8_t sector_buffer[512];

uint32_t sdcard_getsize(void);
void sdcard_open(void);
void sdcard_writesector(const uint32_t sector_number, uint8_t is_multi);
void sdcard_readsector(const uint32_t sector_number);
void sdcard_erase(const uint32_t first_sector, const uint32_t last_sector);
void mega65_fast(void);
void sdcard_map_sector_buffer(void);
void usleep(uint32_t micros);
void sdcard_writenextsector(void);
void sdcard_writemultidone(void);
