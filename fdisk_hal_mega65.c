#include <stdio.h>
#include <stdlib.h>

#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "ascii.h"

#define POKE(X,Y) (*(unsigned char*)(X))=Y
#define PEEK(X) (*(unsigned char*)(X))

const long sd_sectorbuffer=0xffd6e00L;
const uint16_t sd_ctl=0xd680L;
const uint16_t sd_addr=0xd681L;
const uint16_t sd_errorcode=0xd6daL;

unsigned char sdhc_card=0;

// Tell utilpacker what our display name is
const char *prop_m65u_name="PROP.M65U.NAME=SDCARD FDISK+FORMAT UTILITY";

void usleep(uint32_t micros)
{
  // Sleep for desired number of micro-seconds.
  // Each VIC-II raster line is ~64 microseconds
  // this is not totally accurate, but is a reasonable approach
  while(micros>64) {    
    uint8_t b=PEEK(0xD012);
    while(PEEK(0xD012)==b) continue;
    micros-=64;
  }
  return;
}

void sdcard_reset(void)
{
  // Reset and release reset
  //  write_line("Resetting SD card...",0);

  POKE(sd_ctl,0);
  POKE(sd_ctl,1);

  // Now wait for SD card reset to complete
  while (PEEK(sd_ctl)&3) {
    POKE(0xd020,(PEEK(0xd020)+1)&15);
  }
  
  if (sdhc_card) {
    // Set SDHC flag (else writing doesnt work for some reason)
    //    write_line("Setting SDHC mode",0);
    POKE(sd_ctl,0x41);
  }
}

void mega65_fast(void)
{
  // Fast CPU
  POKE(0,65);
  // MEGA65 IO registers
  POKE(0xD02FU,0x47);
  POKE(0xD02FU,0x53);
}

void sdcard_open(void)
{
  sdcard_reset();
}


uint32_t write_count=0;

void sdcard_map_sector_buffer(void)
{
  m65_io_enable();
  
  POKE(sd_ctl,0x81);
}

void sdcard_unmap_sector_buffer(void)
{
  m65_io_enable();
  
  POKE(sd_ctl,0x82);
}

unsigned short timeout;

void sdcard_readsector(const uint32_t sector_number)
{
  char tries=0;
  
  uint32_t sector_address=sector_number*512;
  if (sdhc_card) sector_address=sector_number;
  else {
    if (sector_number>=0x7fffff) {
      //      write_line("ERROR: Asking for sector @ >= 4GB on SDSC card.",0);
      while(1) continue;
    }
  }

  POKE(sd_addr+0,(sector_address>>0)&0xff);
  POKE(sd_addr+1,(sector_address>>8)&0xff);
  POKE(sd_addr+2,((uint32_t)sector_address>>16)&0xff);
  POKE(sd_addr+3,((uint32_t)sector_address>>24)&0xff);

  // write_line("Reading sector @ $",0);
  //  screen_hex(screen_line_address-80+18,sector_address);
  
  while(tries<10) {

    // Wait for SD card to be ready
    timeout=50000U;
    while (PEEK(sd_ctl)&0x3)
      {
	timeout--;
	if (!timeout) {
	  // Time out -- so reset SD card
	  POKE(sd_ctl,0);
	  POKE(sd_ctl,1);
	  timeout=50000U;
	}
	if (PEEK(sd_ctl)&0x40)
	  {
	    return;
	  }
	// Sometimes we see this result, i.e., sdcard.vhdl thinks it is done,
	// but sdcardio.vhdl thinks not. This means a read error
	if (PEEK(sd_ctl)==0x01) return;
      }

    // Command read
    POKE(sd_ctl,2);
    
    // Wait for read to complete
    timeout=50000U;
    while (PEEK(sd_ctl)&0x3) {
      timeout--; if (!timeout) return;
	//      write_line("Waiting for read to complete",0);
      if (PEEK(sd_ctl)&0x40)
	{
	  return;
	}
      // Sometimes we see this result, i.e., sdcard.vhdl thinks it is done,
      // but sdcardio.vhdl thinks not. This means a read error
      if (PEEK(sd_ctl)==0x01) return;
    }

      // Note result
    // result=PEEK(sd_ctl);

    if (!(PEEK(sd_ctl)&0x67)) {
      // Copy data from hardware sector buffer via DMA
      lcopy(sd_sectorbuffer,(long)sector_buffer,512);
  
      return;
    }
    
    POKE(0xd020,(PEEK(0xd020)+1)&0xf);

    // Reset SD card
    sdcard_open();

    tries++;
  }
  
}

uint8_t verify_buffer[512];

void sdcard_writesector(const uint32_t sector_number,uint8_t is_multi)
{
  // Copy buffer into the SD card buffer, and then execute the write job
  uint32_t sector_address;
  int i;
  char tries=0,result;
  uint16_t counter=0;
  
  // Set address to read/write
  POKE(sd_ctl,1); // end reset
  if (!sdhc_card) sector_address=sector_number*512;
  else sector_address=sector_number;
  POKE(sd_addr+0,(sector_address>>0)&0xff);
  POKE(sd_addr+1,(sector_address>>8)&0xff);
  POKE(sd_addr+2,(sector_address>>16)&0xff);
  POKE(sd_addr+3,(sector_address>>24)&0xff);

  // Read the sector and see if it already has the correct contents.
  // If so, nothing to write

  POKE(sd_ctl,2); // read the sector we just wrote
  
  while (PEEK(sd_ctl)&3) {
    continue;
  }
  
  // Copy the read data to a buffer for verification
  lcopy(sd_sectorbuffer,(long)verify_buffer,512);
  
  // VErify that it matches the data we wrote
  for(i=0;i<512;i++) {
    if (sector_buffer[i]!=verify_buffer[i]) break;
  }
  if (i==512) {
    return;
  } 
  
  while(tries<10) {

    // Copy data to hardware sector buffer via DMA
    lcopy((long)sector_buffer,sd_sectorbuffer,512);
  
    // Wait for SD card to be ready
    counter=0;
    while (PEEK(sd_ctl)&3)
      {
	counter++;
	if (!counter) {
	  // SD card not becoming ready: try reset
	  POKE(sd_ctl,0); // begin reset
	  usleep(500000);
	  POKE(sd_ctl,1); // end reset
	  if (is_multi) POKE(sd_ctl,4);
	  else POKE(sd_ctl,3); // retry write

	}
	// Show we are doing something
	//	POKE(0x804f,1+(PEEK(0x804f)&0x7f));
      }
    
    // Command write
    if (is_multi) POKE(sd_ctl,4);
    else POKE(sd_ctl,3); 
    
    // Wait for write to complete
    counter=0;
    while (PEEK(sd_ctl)&3)
      {
	counter++;
	if (!counter) {
	  // SD card not becoming ready: try reset
	  POKE(sd_ctl,0); // begin reset
	  usleep(500000);
	  POKE(sd_ctl,1); // end reset
	  // Retry write
	  if (is_multi) POKE(sd_ctl,4);
	  else POKE(sd_ctl,3); 

	}
	// Show we are doing something
	//	POKE(0x809f,1+(PEEK(0x809f)&0x7f));
      }

    write_count++;
    POKE(0xD020,write_count&0x0f);

    // Note result
    result=PEEK(sd_ctl);
    
    if (!(PEEK(sd_ctl)&0x67)) {
      write_count++;
      
      POKE(0xD020,write_count&0x0f);

      // There is a bug in the SD controller: You have to read between writes, or it
      // gets really upset.

      // But sometimes even that doesn't work, and we have to reset it.

      // Does it just need some time between accesses?
      
      POKE(sd_ctl,2); // read the sector we just wrote

      while (PEEK(sd_ctl)&3) {
      	continue;
      }

      // Copy the read data to a buffer for verification
      lcopy(sd_sectorbuffer,(long)verify_buffer,512);

      // VErify that it matches the data we wrote
      for(i=0;i<512;i++) {
	if (sector_buffer[i]!=verify_buffer[i]) break;
      }
      if (i!=512) {
	// VErify error has occurred
	//	write_line("Verify error for sector $$$$$$$$",0);
	screen_hex(screen_line_address-80+24,sector_number);
      }
      else {
      //      write_line("Wrote sector $$$$$$$$, result=$$",2);      
      //      screen_hex(screen_line_address-80+2+14,sector_number);
      //      screen_hex(screen_line_address-80+2+30,result);

	return;
      }
    }

    POKE(0xd020,(PEEK(0xd020)+1)&0xf);

  }

  //  write_line("Write error @ $$$$$$$$$",2);      
  //  screen_hex(screen_line_address-80+2+16,sector_number);
  
}

void sdcard_writenextsector(void)
{
  // Copy data to hardware sector buffer via DMA
  lcopy((long)sector_buffer,sd_sectorbuffer,512);

  // Command write of follow-on block in multi-block write job
  while (PEEK(sd_ctl)&3) {
    continue;
  }
  POKE(0xD680U,5);
  while (!(PEEK(sd_ctl)&3)) {
    continue;
  }
  while (PEEK(sd_ctl)&3) {
    continue;
  }
}

void sdcard_writemultidone(void)
{
  while (PEEK(sd_ctl)&3) {
    continue;
  }
  POKE(0xD680U,6);
  while (!(PEEK(sd_ctl)&3)) {
    continue;
  }
  while (PEEK(sd_ctl)&3) {
    continue;
  }
}


void sdcard_erase(const uint32_t first_sector,const uint32_t last_sector)
{
  uint32_t n;
  lfill((uint32_t)sector_buffer,0,512);

  //  fprintf(stderr,"ERASING SECTORS %d..%d\r\n",first_sector,last_sector);

#ifndef NOFAST_ERASE
  POKE(sd_addr+0,(first_sector>>0)&0xff);
  POKE(sd_addr+1,(first_sector>>8)&0xff);
  POKE(sd_addr+2,(first_sector>>16)&0xff);
  POKE(sd_addr+3,(first_sector>>24)&0xff);
#endif   
  
  for(n=first_sector;n<=last_sector;n++) {

#ifndef NOFAST_ERASE
    // Wait for SD card to go ready
    while (PEEK(sd_ctl)&3) continue;

    if (n==first_sector) {
      // First sector of multi-sector write
      POKE(sd_ctl,0x04);
    } else
      // All other sectors
      POKE(sd_ctl,0x05);

    // Wait for SD card to go busy
    while (!(PEEK(sd_ctl)&3)) continue;

    // Wait for SD card to go ready
    while (PEEK(sd_ctl)&3) continue;
       
#else
    sdcard_writesector(n);
#endif
    
    // Show count-down
    screen_decimal(screen_line_address,last_sector-n);
    //    fprintf(stderr,"."); fflush(stderr);
  }

#ifndef NOFAST_ERASE
  // Then say when we are done
  POKE(sd_ctl,0x06);
  
  // Wait for SD card to go busy
  while (!(PEEK(sd_ctl)&3)) continue;
  
  // Wait for SD card to go ready
  while (PEEK(sd_ctl)&3) continue;
#endif    
  
}
