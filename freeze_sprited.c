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

uint8_t nybl_to_hex(uint8_t v)
{
  if (v<0xa) return 0x30+v;
  return 0x41-0xa+v;
}

void draw_sprite_editor(void)
{
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

