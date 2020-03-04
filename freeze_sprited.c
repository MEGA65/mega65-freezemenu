#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

uint8_t c,value,select_row,select_column;
uint8_t sprite_width=24;
uint8_t sprite_height=21;
uint16_t i;
uint8_t x,y;


uint8_t nybl_to_hex(uint8_t v)
{
  if (v<0xa) return 0x30+v;
  return 0x41-0xa+v;
}

void draw_sprite_editor(void)
{
  // XXX - Doesn't clear the screen first.
  // If changing sprite size, then setup_screen() + display_footer() should be called
  // again first.
  
  // Draw box based on sprite size.
  // 64-pixel wide sprites obviously can't use 2 chars per pixel, but otherwise we do.
  if (sprite_width<64) {
    lfill(SCREEN_ADDRESS,0x62,sprite_width*2+2);
    if (sprite_height<22)
      // Whole sprite fits on screen
      lfill(SCREEN_ADDRESS+(1+sprite_height)*80,0x80+0x62,sprite_width*2+2);
    else
      // Only part of the sprite fits on the screen
      lfill(SCREEN_ADDRESS+22*80,0x080+0x62,sprite_width*2+2);
  } else {
    lfill(SCREEN_ADDRESS+1,0x62,sprite_width+2);
    if (sprite_height<22)
      // Whole sprite fits on screen
      lfill(SCREEN_ADDRESS+(1+sprite_height)*80,0x80+0x62,sprite_width+2);
    else
      // Only part of the sprite fits on the screen
      lfill(SCREEN_ADDRESS+22*80,0x80+0x62,sprite_width+2);
  }
  if (sprite_height<22) {
    for(i=1;i<=sprite_height;i++) POKE(SCREEN_ADDRESS+80*i,0x80+0x20);
    if (sprite_width<64)
      for(i=1;i<=sprite_height;i++) POKE(SCREEN_ADDRESS+1+2*sprite_width+80*i,0x80+0x20);
    else
      for(i=1;i<=sprite_height;i++) POKE(SCREEN_ADDRESS+1+sprite_width+80*i,0x80+0x20);      
  } else {
    for(i=1;i<22;i++) POKE(SCREEN_ADDRESS+80*i,0x80+0x20);
    if (sprite_width<64)
      for(i=1;i<22;i++) POKE(SCREEN_ADDRESS+1+2*sprite_width+80*i,0x80+0x20);
    else
      for(i=1;i<22;i++) POKE(SCREEN_ADDRESS+1+sprite_width+80*i,0x80+0x20);      
  }

  // pixels
  {
    uint16_t line_address=SCREEN_ADDRESS+80+1;
    uint16_t cursor_address;
    uint8_t plot_char=0x80+0x20;
    for(y=0;y<22;y++) {
      cursor_address=line_address;
      for(x=0;x<sprite_width;x++) {

	if (x==select_column&&y==select_row) plot_char=0x80+0x2a;
	if (sprite_width<64) {
	  cursor_address+=2;
	} else {
	  cursor_address++;
	}
	
      }
      line_address+=80;
    }
  }
  
}

void do_sprite_editor(void)
{
  setup_screen();
  display_footer(FOOTER_SPRITED);
  
  select_row=15; select_column=0;
  
  while(1) {
    draw_sprite_editor();

    if (PEEK(0xD610U)) {    

      unsigned char c=PEEK(0xD610U);
      
      // Flush char from input buffer
      POKE(0xD610U,0);

      // Process char
      switch(c) {
      case 0x03:
	// Return screen to normal first
	POKE(0xD054U,0);
	POKE(0xD018U,0x15); // VIC-II hot register, so should reset most display settings
	POKE(0xD016U,0xC8);
	POKE(0xDD00U,PEEK(0xDD00U)|3); // video bank 0
	POKE(0xD031U,PEEK(0xD031U)&0x7f); // 40 columns	
	return;
      case 0x11:
	select_row++;
	if (select_row>=sprite_height) select_row=0;
	break;
      case 0x1d:
	select_column++; 
	if (select_row>=sprite_width) select_column=0;
	break;
      case 0x91:
	select_row--;
	if (select_row>=sprite_height) select_row=sprite_height-1;
	break;
      case 0x9d:
	if (select_column>=sprite_width) select_column=sprite_width-1;
	break;
      case '1':
	// Set primary colour (C128 SPRDEF compatibility)
	break;
      case '2':
	// Make pixel transparent (C128 SPRDEF compatibility)
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

