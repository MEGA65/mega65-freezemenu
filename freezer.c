/*
  Based on mega65-fdisk program as a starting point.

*/

#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

uint8_t sector_buffer[512];

unsigned short slot_number=0;

void clear_sector_buffer(void)
{
#ifndef __CC65__DONTUSE
  int i;
  for(i=0;i<512;i++) sector_buffer[i]=0;
#else
  lfill((uint32_t)sector_buffer,0,512);
#endif
}

unsigned char *freeze_menu=
  "         MEGA65 FREEZE MENU V0.1        "
  "  (C) FLINDERS UNI, M.E.G.A. 2018-2019  "
  " cccccccccccccccccccccccccccccccccccccc "
  " F1 - BACKUP  F3 - RESTART  F7 - SWITCH "
  " cccccccccccccccccccccccccccccccccccccc "
#define CPU_MODE_OFFSET (5*40+13)
#define ROM_PROTECT_OFFSET (5*40+36)
  " (C)PU MODE:   4510  (P)ROTECT ROM: YES "
#define ROM_NAME_OFFSET (6*40+8)
#define CART_ENABLE_OFFSET (6*40+36)
  " (R)OM:  C65 911101  C(A)RT ENABLE: YES "
#define CPU_FREQ_OFFSET (7*40+13)
#define VIDEO_MODE_OFFSET (7*40+33)
  " CPU (F)REQ: 40 MHZ  (V)IDEO:    NTSC60 "
  " cccccccccccccccccccccccccccccccccccccc "
  " M - MONITOR         E - POKES          "
  " D - DISK SELECT     S - VIEW SPRITES   "
  " X - POKE FINDER     K - SPRITE KILLER  "
  " cccccccccccccccccccccccccccccccccccccc "
  "                                        "
  "                                        "
  "                                        "
  "                                        "
  "                                        "
  "                                        "
  "                                        "
  "                                        "
  "                                        "
  "                                        "
  "                                        "
  "                                        "
  "\0";


unsigned short i;
char *deadly_haiku[3]={"Error consumes all","As sand erodes rock and stone","Now also your mind"};

unsigned char c64_palette[64]={
  0x00, 0x00, 0x00, 0x00,
  0xff, 0xff, 0xff, 0x00,
  0xba, 0x13, 0x62, 0x00,
  0x66, 0xad, 0xff, 0x00,
  0xbb, 0xf3, 0x8b, 0x00,
  0x55, 0xec, 0x85, 0x00,
  0xd1, 0xe0, 0x79, 0x00,
  0xae, 0x5f, 0xc7, 0x00,
  0x9b, 0x47, 0x81, 0x00,
  0x87, 0x37, 0x00, 0x00,
  0xdd, 0x39, 0x78, 0x00,
  0xb5, 0xb5, 0xb5, 0x00,
  0xb8, 0xb8, 0xb8, 0x00,
  0x0b, 0x4f, 0xca, 0x00,
  0xaa, 0xd9, 0xfe, 0x00,
  0x8b, 0x8b, 0x8b, 0x00
};

unsigned char colour_table[256];

void set_palette(void)
{
  // First set the 16 C64 colours
  unsigned char c;
  POKE(0xD070U,0xFF);
  for(c=0;c<16;c++) {
    POKE(0xD100U+c,c64_palette[c*4+0]);
    POKE(0xD200U+c,c64_palette[c*4+1]);
    POKE(0xD300U+c,c64_palette[c*4+2]);
  }

  // Then prepare a colour cube in the rest of the palette
  for(c=0x10;c;c++) {
    // 3 bits for red
    POKE(0xD100U+c,(c>>4)&0xe);
    // 3 bits for green
    POKE(0xD200U+c,(c>>1)&0xe);
    // 2 bits for blue
    POKE(0xD300U+c,(c<<2)&0xf);
  }

  // Make colour lookup table
  c=0;
  do {
    colour_table[c]=c;
  } while(++c);

  // Now map C64 colours directly
  colour_table[0x00]=0x00;  // black
  colour_table[0xff]=0x01;  // white
  colour_table[0xe0]=0x02;  // red
  colour_table[0x1f]=0x03;  // cyan
  colour_table[0xe3]=0x04;  // purple
  colour_table[0x1c]=0x05;  // green
  colour_table[0x03]=0x06;  // blue
  colour_table[0xfc]=0x07;  // yellow
  colour_table[0xec]=0x08;  // orange
  colour_table[0xa8]=0x09;  // brown
  colour_table[0xad]=0x0a;  // pink
  colour_table[0x49]=0x0b;  // grey1
  colour_table[0x92]=0x0c;  // grey2
  colour_table[0x9e]=0x0d;  // lt.green
  colour_table[0x93]=0x0e;  // lt.blue
  colour_table[0xb6]=0x0f;  // grey3
  

  // We should also map colour cube colours 0x00 -- 0x0f to
  // somewhere sensible.
  // 0x00 = black, so can stay
  #if 0
  colour_table[0x01]=0x06;  // dim blue -> blue
  // colour_table[0x02]=0x06;  // medium dim blue -> blue
  // colour_table[0x03]=0x06;  // bright blue -> blue
  colour_table[0x04]=0x00;  // dim green + no blue
  colour_table[0x05]=0x25;  
  colour_table[0x06]=0x26;  
  colour_table[0x07]=0x27;  
  colour_table[0x08]=0x28;  
  colour_table[0x09]=0x29;  
  colour_table[0x0A]=0x2a;  
  colour_table[0x0B]=0x2b;  
  colour_table[0x0C]=0x2c;  
  colour_table[0x0D]=0x2d;  
  colour_table[0x0E]=0x2e;  
  colour_table[0x0F]=0x2f;  
  #endif

};

void setup_menu_screen(void)
{
  POKE(0xD018U,0x15); // upper case

  // NTSC 60Hz mode for monitor compatibility?
  POKE(0xD06FU,0x80);

  // No sprites
  POKE(0xD015U,0x00);

  // Move screen to SCREEN_ADDRESS
  POKE(0xD018U,(((CHARSET_ADDRESS-0x8000U)>>11)<<1)+(((SCREEN_ADDRESS-0x8000U)>>10)<<4));
  POKE(0xDD00U,(PEEK(0xDD00U)&0xfc)|0x01);

  // 16-bit text mode with full colour for chars >$FF
  // (which we will use for showing the thumbnail)
  POKE(0xD054U,0x05);
  POKE(0xD058U,80); POKE(0xD059U,0); // 80 bytes per row
}  


unsigned char ascii_to_screencode(char c)
{
  if (c>=0x60) return c-0x60;
  return c;
}

void screen_of_death(char *msg)
{
  POKE(0,0x41);
  POKE(0xD02FU,0x47); POKE(0xD02FU,0x53);

  // Reset video mode
  POKE(0xD05DU,0x01); POKE(0xD011U,0x1b); POKE(0xD016U,0xc8);
  POKE(0xD018U,0x17); // lower case
  POKE(0xD06FU,0x80); // NTSC 60Hz mode for monitor compatibility?

  // No sprites
  POKE(0xD015U,0x00);
  
  // Normal video mode
  POKE(0xD054U,0x00);

  // Reset colour palette to normal for black and white
  POKE(0xD100U,0x00);  POKE(0xD200U,0x00);  POKE(0xD300U,0x00);
  POKE(0xD101U,0xFF);  POKE(0xD201U,0xFF);  POKE(0xD301U,0xFF);
  
  POKE(0xD020U,0); POKE(0xD021U,0);

  // Reset CPU IO ports
  POKE(1,0x3f); POKE(0,0x3F);
  lfill(0x0400U,' ',1000);
  lfill(0xd800U,1,1000);

  for(i=0;deadly_haiku[0][i];i++) POKE(0x0400+10*40+11+i,ascii_to_screencode(deadly_haiku[0][i]));
  for(i=0;deadly_haiku[1][i];i++) POKE(0x0400+12*40+11+i,ascii_to_screencode(deadly_haiku[1][i]));
  for(i=0;deadly_haiku[2][i];i++) POKE(0x0400+14*40+11+i,ascii_to_screencode(deadly_haiku[2][i]));  
  
  while(1) continue;
  
}

unsigned char detect_cpu_speed(void)
{
  if (freeze_peek(0xffd367dL)&0x10) return 40;
  if (freeze_peek(0xffd3054L)&0x40) return 40;
  if (freeze_peek(0xffd3031L)&0x40) return 3;
  if (freeze_peek(0xffd0030L)&0x01) return 2;
  return 1;  
}

void next_cpu_speed(void)
{
  switch(detect_cpu_speed()) {
  case 1:
    // Make it 2MHz
    freeze_poke(0xffd0030L,1);
    break;
  case 2:
    // Make it 3.5MHz
    freeze_poke(0xffd0030L,0);
    freeze_poke(0xffd3031L,freeze_peek(0xffd3031L)|0x40);
    break;
  case 3:
    // Make it 40MHz
    freeze_poke(0xffd367dL,freeze_peek(0xffd367dL)|0x10);
    break;
  case 40: default:
    // Make it 1MHz
    freeze_poke(0xffd0030L,0);
    freeze_poke(0xffd3031L,freeze_peek(0xffd3031L)&0xbf);
    freeze_poke(0xffd3054L,freeze_peek(0xffd3054L)&0xbf);
    freeze_poke(0xffd367dL,freeze_peek(0xffd367dL)&0xef);
    break;
  }
}

char c65_rom_name[12];
char *detect_rom(void)
{
  // Check for C65 ROM via version string
  if ((freeze_peek(0x20016L)=='V')
      &&(freeze_peek(0x20017L)=='9')) {
    c65_rom_name[0]=' ';
    c65_rom_name[1]='C';
    c65_rom_name[2]='6';
    c65_rom_name[3]='5';
    c65_rom_name[4]=' ';
    for(i=0;i<6;i++)
      c65_rom_name[5+i]=freeze_peek(0x20017L+i);
    c65_rom_name[11]=0;
    return c65_rom_name;
    
  }

  if (freeze_peek(0x2e47dL)=='J') {
    // Probably jiffy dos
    if (freeze_peek(0x2e535L)==0x06)
      return "sx64 jiffy ";
    else
      return "c64 jiffy  ";
  }
  
  // Else guess using detection routines from detect_roms.c
  // These were built using a combination of the ROMs from zimmers.net/pub/c64/firmware,
  // the RetroReplay ROM collection, and the JiffyDOS ROMs
  if (freeze_peek(0x2e449L)==0x2e) return "C64gs      ";
  if (freeze_peek(0x2e119L)==0xc9) return "c64 rev1   ";
  if (freeze_peek(0x2e67dL)==0xb0) return "c64 rev2 JP";
  if (freeze_peek(0x2ebaeL)==0x5b) return "c64 rev3 DK";
  if (freeze_peek(0x2e0efL)==0x28) return "c64 scand  ";
  if (freeze_peek(0x2ebf3L)==0x40) return "c64 sweden ";
  if (freeze_peek(0x2e461L)==0x20) return "cyclone 1.0";
  if (freeze_peek(0x2e4a4L)==0x41) return "dolphin 1.0";
  if (freeze_peek(0x2e47fL)==0x52) return "dolphin 2AU";
  if (freeze_peek(0x2eed7L)==0x2c) return "dolphin 2p1";
  if (freeze_peek(0x2e7d2L)==0x6b) return "dolphin 2p2";
  if (freeze_peek(0x2e4a6L)==0x32) return "dolphin 2p3";
  if (freeze_peek(0x2e0f9L)==0xaa) return "dolphin 3.0";
  if (freeze_peek(0x2e462L)==0x45) return "dosrom v1.2";
  if (freeze_peek(0x2e472L)==0x20) return "mercry3 pal";
  if (freeze_peek(0x2e16dL)==0x84) return "mercry ntsc";
  if (freeze_peek(0x2e42dL)==0x4c) return "pet 4064   ";
  if (freeze_peek(0x2e1d9L)==0xa6) return "sx64 croach";
  if (freeze_peek(0x2eba9L)==0x2d) return "sx64 scand ";
  if (freeze_peek(0x2e476L)==0x2a) return "trboacs 2.6";
  if (freeze_peek(0x2e535L)==0x07) return "trboacs 3p1";
  if (freeze_peek(0x2e176L)==0x8d) return "trboacs 3p2";
  if (freeze_peek(0x2e42aL)==0x72) return "trboproc us";
  if (freeze_peek(0x2e4acL)==0x81) return "c64c 251913";
  if (freeze_peek(0x2e479L)==0x2a) return "c64 rev2   ";
  if (freeze_peek(0x2e535L)==0x06) return "sx64 rev4  ";
  return "unknown rom";
}  

void draw_thumbnail(void)
{
  // Take the 4K of thumbnail data and render it to the display
  // area at $50000.
  // This requires a bit of fiddling:
  // First, the thumbnail data has a nominal address of $0010000
  // in the frozen memory, which overlaps with the main RAM,
  // so we can't use our normal routine to find the start of freeze
  // memory. Instead, we will find that region directly, and then
  // process the 8 sectors of data in a linear fashion.
  // The thumbnail bytes themselves are arranged linearly, so we
  // have to work out the right place to store them in the thumbnail
  // data.  We would really like to avoid having to use lpoke for
  // this all the time, because lpoke() uses a DMA for every memory
  // access, which really slows things down. This would be bad, since
  // we want users to be able to very quickly and smoothly flip between
  // the freeze slots and see what is there.
  // So we will instead copy the sectors down to $8800, and then
  // render the thumbnail at $9000, and then copy it into place with
  // a single DMA.
  unsigned char x,y,i;
  uint32_t thumbnail_sector=find_thumbnail_offset();
  // Can't find thumbnail area?  Then show no thumbnail
  if (thumbnail_sector==0xFFFFFFFFL) {
    lfill(0x50000L,0,10*6*64);
    return;
  }
  // Copy thumbnail memory to $08800
  for(i=0;i<8;i++) {
    sdcard_readsector(freeze_slot_start_sector+thumbnail_sector+i);
    lcopy((long)sector_buffer,0x8800L+(i*0x200),0x200);
  }
  // Rearrange pixels
  for(x=0;x<80;x++)
    for(y=0;y<60;y++) {
      // Image is off centre as produced by thumbnail hardware, so just
      // don't draw the left few pixels
//      if (x<6) {
//	POKE(0xA000U+(x&7)+(x>>3)*(64*6L)+((y&7)<<3)+(y>>3)*64,0);
	//     } else
	// Also the whole thing is rotated by one byte, so add that on as we plot the pixel
	POKE(0xA000U+(x&7)+(x>>3)*(64*6L)+((y&7)<<3)+(y>>3)*64,
	     colour_table[PEEK(0x8800U+8+1+x+(y*80))]);
    }
  // Copy to final area
  lcopy(0xA000U,0x50000U,4096);
}

void draw_freeze_menu(void)
{
  unsigned char x,y;
  // Wait until we are in vertical blank area before redrawing, so that we don't have flicker

  // Update messages based on the settings we allow to be easily changed

  // CPU MODE
  if (freeze_peek(0xffd367dL)&0x20)
    lcopy("  4502",&freeze_menu[CPU_MODE_OFFSET],6);
  else
    lcopy("  AUTO",&freeze_menu[CPU_MODE_OFFSET],6);

  // ROM area write protect
  lcopy((freeze_peek(0xffd367dL)&0x04)?"YES":" NO",
	&freeze_menu[ROM_PROTECT_OFFSET],3);

  // ROM version
  lcopy((long)detect_rom(),&freeze_menu[ROM_NAME_OFFSET],11);
  
  // Cartridge enable
  lcopy((freeze_peek(0xffd367dL)&0x01)?"YES":" NO",
	&freeze_menu[CART_ENABLE_OFFSET],3);
  
  // CPU frequency
  switch(detect_cpu_speed()) {
  case 1:  lcopy("  1",&freeze_menu[CPU_FREQ_OFFSET],3); break;
  case 2:  lcopy("  2",&freeze_menu[CPU_FREQ_OFFSET],3); break;
  case 3:  lcopy("3.5",&freeze_menu[CPU_FREQ_OFFSET],3); break;
  case 40: lcopy(" 40",&freeze_menu[CPU_FREQ_OFFSET],3); break;
  default: lcopy("???",&freeze_menu[CPU_FREQ_OFFSET],3); break;
  }
  

  
  if (freeze_peek(0xffd306fL)&0x80) {
    // NTSC60
    lcopy("NTSC60",&freeze_menu[VIDEO_MODE_OFFSET],6);
  } else {
    // PAL50
    lcopy(" PAL50",&freeze_menu[VIDEO_MODE_OFFSET],6);
  }
  
  while(PEEK(0xD012U)<0xf8) continue;
  
  // Clear screen, blue background, white text, like Action Replay
  POKE(0xD020U,6); POKE(0xD021U,6);

  lfill(SCREEN_ADDRESS,0,2000);
  lfill(0xFF80000L,1,2000);
  
  // Freezer can't use printf() etc, because C64 ROM has not started, so ZP will be a mess
  // (in fact, most of memory contains what the frozen program had. Only our freezer program
  // itself has been loaded to replace some of RAM).
  for(i=0;freeze_menu[i];i++) {
    if ((freeze_menu[i]>='A')&&(freeze_menu[i]<='Z'))
      POKE(SCREEN_ADDRESS+i*2+0,freeze_menu[i]-0x40);
    else if ((freeze_menu[i]>='a')&&(freeze_menu[i]<='z'))
      POKE(SCREEN_ADDRESS+i*2+0,freeze_menu[i]-0x20);
    else
      POKE(SCREEN_ADDRESS+i*2+0,freeze_menu[i]);
    POKE(SCREEN_ADDRESS+i*2+1,0);
  }

  // Now draw the 10x6 character block for thumbnail display
  // This sits in the region below the menu where we will also have left and right arrows,
  // the program name etc, so you can easily browse through the freeze slots.
  for(x=0;x<10;x++)
    for(y=0;y<6;y++) {
      POKE(SCREEN_ADDRESS+(80*16)+(6*2)+(x*2)+(y*80)+0,x*6+y); // $50000 base address
      POKE(SCREEN_ADDRESS+(80*16)+(6*2)+(x*2)+(y*80)+1,0x14); // $50000 base address
    }  
}  

#ifdef __CC65__
void main(void)
#else
int main(int argc,char **argv)
#endif
{
#ifdef __CC65__
  mega65_fast();
#endif  

  // Disable interrupts and interrupt sources
  __asm__("sei");
  POKE(0xDC0DU,0x7F);
  POKE(0xDD0DU,0x7F);
  POKE(0xD01AU,0x00);
  // XXX add missing C65 AND M65 peripherals
  // C65 UART, ethernet etc
  
  // No decimal mode!
  __asm__("cld");

  set_palette();
  
  request_freeze_region_list();

  // Now find the start sector of the slot, and make a copy for safe keeping
  slot_number=0;
  find_freeze_slot_start_sector(slot_number);
  freeze_slot_start_sector = *(uint32_t *)0xD681U;

  // SD or SDHC card?
  if (PEEK(0xD680U)&0x10) sdhc_card=1; else sdhc_card=0;

  setup_menu_screen();
  draw_freeze_menu();
  draw_thumbnail();
  
  // Flush input buffer
  mega65_fast();
  while (PEEK(0xD610U)) POKE(0xD610U,0);
  
  // Main keyboard input loop
  while(1) {
    if (PEEK(0xD610U)) {
      
      // Process char
      switch(PEEK(0xD610U)) {
      case 0xf3: // F3 = resume
	// Load memory from freeze slot $0000, i.e., the temporary save space
	// This implicitly restarts the frozen program
	unfreeze_slot(slot_number);

	// should never get here
	screen_of_death("unfreeze failed");
	
	break;

      case 'M': case 'm': // Monitor
	freeze_monitor();
	setup_menu_screen();
	draw_freeze_menu();
	break;

      case 'A': case 'a': // Toggle cartridge enable
	freeze_poke(0xFFD367dL,freeze_peek(0xFFD367dL)^0x01);
	draw_freeze_menu();
	break;

      case 'P': case 'p': // Toggle ROM area write-protect
	freeze_poke(0xFFD367dL,freeze_peek(0xFFD367dL)^0x04);
	draw_freeze_menu();
	break;

      case 'c': case 'C': // Toggle CPU mode
	freeze_poke(0xFFD367dL,freeze_peek(0xFFD367dL)^0x20);
	draw_freeze_menu();
	break;

      case 'F': case 'f': // Change CPU speed
	next_cpu_speed();
	draw_freeze_menu();
	break;

      case 'V': case 'v': // Toggle video mode
	freeze_poke(0xFFD306fL,freeze_peek(0xFFD306fL)^0x80);
	draw_freeze_menu();
	break;
	
      case 0xf1: // F1 = backup
      case 0xf7: // F7 = Switch tasks

      case 'R': case 'r': // Switch ROMs
	
      case 'D': case 'd': // Select mounted disk image
      case 'X': case 'x': // Poke finder
      case 'E': case 'e': // Enter POKEs
      case 'S': case 's': // View sprites
      case 'k': case 'K': // Sprite killer
      default:
	// For invalid or unimplemented functions flash the border and screen
	POKE(0xD020U,1); POKE(0xD021U,1);
	usleep(150000L);
	POKE(0xD020U,6); POKE(0xD021U,6);
	break;
      }
      
      // Flush char from input buffer
      POKE(0xD610,0);
    }
  }
  
  return;
}
