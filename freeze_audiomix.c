#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

unsigned char *audio_menu=
  "         MEGA65 AUDIO MIXER MENU        "
  "  (C) FLINDERS UNI, M.E.G.A. 2018-2019  "
  " cccccccccccccccccccccccccccccccccccccc "
  "        LFT RGT PH1 PH2 BTL BTR HDL HDR "
  "        cccccccccccccccccccccccccccccccc"
  "   SIDLb                                "
  "   SIDRb                                "
  " PHONE1b                                "
  " PHONE2b                                "
  "BTOOTHLb                                "
  "BTOOTHRb                                "
  "LINEINLb                                "
  "LINEINRb                                "
  "  DIGILb                                "
  "  DIGIRb                                "
  "  MIC0Lb                                "
  "  MIC0Rb                                "
  "  MIC1Lb                                "
  "  MIC1Rb                                "
  "  SPAREb                                "
  " MASTERb                                "
  " cccccccccccccccccccccccccccccccccccccc "
  " USE CURSOR KEYS TO SELECT COEFFICIENTS "
  " F1,F3 INCREASES VALUE, F5,F7 DECREASES "
  "  RUN/STOP - EXIT, M - TOGGLE MIC MUTE  "
  "\0";



void audioxbar_setcoefficient(uint8_t n,uint8_t value)
{
  // Select the coefficient
  POKE(0xD6F4,n);

  // Now wait at least 16 cycles for it to settle
  POKE(0xD020U,PEEK(0xD020U));
  POKE(0xD020U,PEEK(0xD020U));

  POKE(0xD6F5U,value); 
}

uint8_t audioxbar_getcoefficient(uint8_t n)
{
  // Select the coefficient
  POKE(0xD6F4,n);

  // Now wait at least 16 cycles for it to settle
  POKE(0xD020U,PEEK(0xD020U));
  POKE(0xD020U,PEEK(0xD020U));

  return PEEK(0xD6F5U); 
}

uint8_t c,value,select_row,select_column;
uint16_t i;

uint8_t nybl_to_hex(uint8_t v)
{
  if (v<0xa) return 0x30+v;
  return 0x41-0xa+v;
}

void draw_audio_mixer(void)
{
  uint16_t offset;
  uint8_t colour;

  audio_menu[38]=nybl_to_hex(select_column);
  audio_menu[39]=nybl_to_hex(select_row);
  
  c=0;
  do {

    // Work out address of where to draw the value
    offset=8+5*40;  // Start of first value location
    offset+=((c&0x1e)>>1)*40; // Low bits of number indicate Y position
    offset+=(c>>3)&0x1e; // High bits pick the column
    offset+=(c&1)+(c&1); // lowest bit picks LSB/MSB
    if (c&0x10) offset-=2; // XXX Why do we need this fudge factor?
      
    // And get the value to display
    value=audioxbar_getcoefficient(c);
    audio_menu[offset]=nybl_to_hex(value>>4);
    audio_menu[offset+1]=nybl_to_hex(value&0xf);

    // Now pick the colour to display
    // We want to make it easy to find values, so we should
    // have pairs of columns for odd and even rows, and a
    // highlight colour for the currently selected coefficient
    // (or just reverse video)

    colour=12;
    if (((c&0x1e)>>1)==select_row) colour=13;
    audio_menu[37]=nybl_to_hex(c>>5);
    if ((c>>5)==select_column) {
      if (colour==13) colour=1; else colour=13;
    }

    lpoke(COLOUR_RAM_ADDRESS+offset+offset+1,colour);
    lpoke(COLOUR_RAM_ADDRESS+offset+offset+3,colour);
    
  } while(++c);
  
  // Update the coefficients in the audio_menu display, then
  // display it after, so that we have no flicker
  
  
    // Freezer can't use printf() etc, because C64 ROM has not started, so ZP will be a mess
  // (in fact, most of memory contains what the frozen program had. Only our freezer program
  // itself has been loaded to replace some of RAM).
  for(i=0;audio_menu[i];i++) {
    if ((audio_menu[i]>='A')&&(audio_menu[i]<='Z'))
      POKE(SCREEN_ADDRESS+i*2+0,audio_menu[i]-0x40);
    else if ((audio_menu[i]>='a')&&(audio_menu[i]<='z'))
      POKE(SCREEN_ADDRESS+i*2+0,audio_menu[i]-0x20);
    else
      POKE(SCREEN_ADDRESS+i*2+0,audio_menu[i]);
    POKE(SCREEN_ADDRESS+i*2+1,0);
  }

}

void do_audio_mixer(void)
{
  select_row=15; select_column=0;
  
  while(1) {
    draw_audio_mixer();

    if (PEEK(0xD610U)) {    

      unsigned char c=PEEK(0xD610U);
      
      // Flush char from input buffer
      POKE(0xD610U,0);
      
      // Process char
      switch(c) {
      case 0x03:
	return;
      case 0x11:
	select_row++; select_row&=0x0f;
	break;
      case 0x1d:
	select_column++; select_column&=0x7;
	break;
      case 0x91:
	select_row--; select_row&=0x0f;
	break;
      case 0x9d:
	select_column--; select_column&=0x7;
	break;
      case 'm': case 'M':
	if (audioxbar_getcoefficient(0x14)) {
	  for(i=0x20;i<0x100;i+=20) {
	    audioxbar_setcoefficient(i+0x14,0);
	    audioxbar_setcoefficient(i+0x15,0);
	    audioxbar_setcoefficient(i+0x16,0);
	    audioxbar_setcoefficient(i+0x17,0);
	  }
	} else {
	  for(i=0x20;i&0x100;i+=20) {
	    audioxbar_setcoefficient(i+0x14,0xff);
	    audioxbar_setcoefficient(i+0x15,0xff);
	    audioxbar_setcoefficient(i+0x16,0xff);
	    audioxbar_setcoefficient(i+0x17,0xff);
	  }
	}
	break;
      default:
	// For invalid or unimplemented functions flash the border and screen
	POKE(0xD020U,1); POKE(0xD021U,1);
	usleep(150000L);
	POKE(0xD020U,6); POKE(0xD021U,6);
	break;
      }
    }
    
  }
  
}
