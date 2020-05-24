#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

#ifdef WITH_AUDIOMIXER

unsigned char *audio_menu=
  "         MEGA65 AUDIO MIXER MENU        "
  "  (C) FLINDERS UNI, M.E.G.A. 2018-2020  "
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
  " T - TEST SOUND, CURSOR KEYS - NAVIGATE "
  " +/- ADJUST VALUE,    0/* - FAST ADJUST "
  " F3 - EXIT,        M - TOGGLE MIC MUTE  "
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
    if ((c>>5)==select_column) {
      if (colour==13) colour=1; else colour=13;
    }
    if (colour==1) {
      audio_menu[33]=nybl_to_hex(c>>4);
      audio_menu[34]=nybl_to_hex(c&0xf);
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

unsigned char frames;
unsigned char note;
unsigned char sid_num;
unsigned int sid_addr;
unsigned int notes[5]={5001,5613,4455,2227,3338};

void test_audio(unsigned char advanced_view)
{
  /*
    Play notes and samples through 4 SIDs and left/right digi
  */

  // Reset all sids
  lfill(0xffd3400,0,0x100);
  
  // Full volume on all SIDs
  POKE(0xD418U,0x0f);
  POKE(0xD438U,0x0f);
  POKE(0xD458U,0x0f);
  POKE(0xD478U,0x0f);

  for(note=0;note<5;note++)
    {
      switch(note) {
      case 0: sid_num=0; break;
      case 1: sid_num=2; break;
      case 2: sid_num=1; break;
      case 3: sid_num=3; break;
      case 4: sid_num=0; break;
      }
	
      sid_addr=0xd400+(0x20*sid_num);

      // Play note
      POKE(sid_addr+0,notes[note]&0xff);
      POKE(sid_addr+1,notes[note]>>8);
      POKE(sid_addr+4,0x10);
      POKE(sid_addr+5,0x0c);
      POKE(sid_addr+6,0x00);
      POKE(sid_addr+4,0x11);

      if (advanced_view) {
	// Highlight the appropriate part of the screen
	for(i=5*80;i<7*80;i+=2) lpoke(0xff80001L+i,lpeek(0xff80001L+i)&0x0f);
	switch(sid_num) {
	case 0: 
	  for(i=0;i<80;i+=2) lpoke(0xff80001L+6*80+i,lpeek(0xff80001L+6*80+i)|0x20);
	  break;
	case 1:
	  for(i=0;i<80;i+=2) lpoke(0xff80001L+6*80+i,lpeek(0xff80001L+6*80+i)|0x60);
	  break;
	case 2: 
	  for(i=0;i<80;i+=2) lpoke(0xff80001L+5*80+i,lpeek(0xff80001L+5*80+i)|0x20);
	  break;
	case 3: 
	  for(i=0;i<80;i+=2) lpoke(0xff80001L+5*80+i,lpeek(0xff80001L+5*80+i)|0x60);
	  break;
	}
      }
      
      // Wait 1/2 second before next note
      // (==25 frames)
      /* 
	 So the trick here, is that we need to decide if we are doing 4-SID mode,
	 where all SIDs are 1/2 volume (gain can of course be increased to compensate),
	 or whether we allow the primary pair of SIDs to be louder.
	 We have to write to 4-SID registers at least every couple of frames to keep them active
      */
      for(frames=0;frames<35;frames++) {
	// Make sure all 4 SIDs remain active
	// by proding while waiting
	while(PEEK(0xD012U)!=0x80) {
	  POKE(0xD438U,0x0f);
	  POKE(0xD478U,0x0f);
	  continue;
	}
	
	while(PEEK(0xD012U)==0x80) continue;
      }
	 
    }

  // Clear highlight
  if (advanced_view) {
    for(i=5*80;i<7*80;i+=2) lpoke(0xff80001L+i,lpeek(0xff80001L+i)&0x0f);
  }
  // Silence SIDs gradually to avoid pops
  for(frames=15;frames<=0;frames--) {
    while(PEEK(0xD012U)!=0x80) {
      POKE(0xD418U,frames);
      POKE(0xD438U,frames);
      POKE(0xD458U,frames);
      POKE(0xD478U,frames);
      continue;
    }
    
    while(PEEK(0xD012U)==0x80) continue;
  }

  // Reset all sids
  lfill(0xffd3400,0,0x100);

  
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

      // Get coefficient number ready
      i=(select_column<<5);
      i+=(select_row<<1);
      i++;
      value=audioxbar_getcoefficient(i);
      
      // Process char
      switch(c) {
      case 0x03: case 0xf3: // RUN/STOP or F3 to exit
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
      case '+':
	value++;
	audioxbar_setcoefficient(i-1,value);
	audioxbar_setcoefficient(i,value);
	break;
      case '0':
	value+=0x10;
	audioxbar_setcoefficient(i-1,value);
	audioxbar_setcoefficient(i,value);
	break;
      case '-':
	value--;
	audioxbar_setcoefficient(i-1,value);
	audioxbar_setcoefficient(i,value);
	break;
      case '*':
	value-=0x10;
	audioxbar_setcoefficient(i-1,value);
	audioxbar_setcoefficient(i,value);
	break;
      case 't': case 'T':
	test_audio(1);
	break;
      case 'm': case 'M':
	if (audioxbar_getcoefficient(0x14)) {
	  for(i=0x00;i<0x100;i+=0x20) {
	    audioxbar_setcoefficient(i+0x14,0);
	    audioxbar_setcoefficient(i+0x15,0);
	    audioxbar_setcoefficient(i+0x16,0);
	    audioxbar_setcoefficient(i+0x17,0);
	  }
	} else {
	  for(i=0x00;i<0x100;i+=0x20) {
	    audioxbar_setcoefficient(i+0x14,0x30);
	    audioxbar_setcoefficient(i+0x15,0x30);
	    audioxbar_setcoefficient(i+0x16,0x30);
	    audioxbar_setcoefficient(i+0x17,0x30);
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

#endif
