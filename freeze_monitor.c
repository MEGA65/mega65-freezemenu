/*
  Memory monitor for the freezer.

  We know where the freeze regions are from freeze_region_list[],
  so we can convert requested addresses into offsets in the freeze slot.

  We can also allow viewing/modification of the raw freeze slot data itself.

*/

#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"

/* Convert a requested address to a location in the freeze slot,
   or to 0xFFFFFFFF if the address is not present.
*/
uint32_t address_to_freeze_slot_offset(uint32_t address)
{
  uint32_t freeze_slot_offset=1;  // Skip the initial saved SD sector at the beginning of each slot
  uint32_t relative_address=0;
  uint32_t region_length=0;
  char skip,i;

  for(i=0;i<freeze_region_count;i++) {
    skip=0;
    if (address<freeze_region_list[i].address_base) skip=1;
    relative_address=address-freeze_region_list[i].address_base;
    region_length=freeze_region_list[i].region_length&REGION_LENGTH_MASK;
    if (relative_address>=region_length) skip=1;
    if (skip) {
      // Skip this region if our address is not in it
      freeze_slot_offset+=region_length>>9;
      // If region is not an integer number of sectors long, don't forget to count the partial sector
      if (region_length&0x1ff) freeze_slot_offset++;
    } else {
      // The address is in this region.

      // Firsts add the number of sectors to get to the one with the content we want
      freeze_slot_offset+=relative_address>>9;

      // Now multiply it by the length of a sector (512 bytes), and add the offset in the sector
      // This gives us the absolute byte position in the slot of the address we want.
      freeze_slot_offset=freeze_slot_offset<<9;
      freeze_slot_offset+=(relative_address&0x1FF);
      return freeze_slot_offset;
    }
  }
  return 0xFFFFFFFFL;
}

unsigned char screen_line=0;
unsigned char screen_line_buffer[80];
unsigned char screen_line_length=0;
unsigned char screen_line_offset=0;

uint32_t hex_value=0;
uint32_t mon_address=0;

char output_buffer[80];

unsigned char mon_sector[512];
uint32_t mon_sector_num=0xffffffff;

void show_memory_line(uint32_t addr)
{  
  uint32_t freeze_slot_offset=address_to_freeze_slot_offset(addr);
  unsigned char i;

  lfill((long)output_buffer,0,80);
  output_buffer[0]=':';
  format_hex((long)&output_buffer[1],addr,7);
  output_buffer[8]=' ';
  
  if (freeze_slot_offset==0xFFFFFFFFL) {
    // Memory that isn't saved
    for(i=0;i<65;i++)
      output_buffer[9+i]="<UNMAPPED OR UNFROZEN MEMORY>                                    "[i]&0x3f;
    output_buffer[9]='<';
    output_buffer[9+28]='>';    
  } else {
    // Only fetch sector if we haven't already got it cached
    if (mon_sector_num!=(freeze_slot_offset>>9)) {
      mon_sector_num=(freeze_slot_offset>>9);
      sdcard_readsector(freeze_slot_start_sector+mon_sector_num);
      lcopy((long)sector_buffer,(long)mon_sector,512);
    }
    for(i=0;i<16;i++) {
      // Space before hex
      output_buffer[8+i*3]=' ';
      // hex digits
      format_hex((long)&output_buffer[8+1+i*3],mon_sector[(i+freeze_slot_offset)&0x1ff],2);
    }
    // Two spaces before character rendering of block
    output_buffer[8+16*3+0]=' ';
    output_buffer[8+16*3+1]=' ';
    // C64 character rendering of each byte
    for(i=0;i<16;i++) {
      hex_value=mon_sector[(i+freeze_slot_offset)&0x1ff];
      output_buffer[8+16*3+2+i]=hex_value;
    }

  }
  // Convert hex back to C64 screen codes
  for(i=0;i<8+16*3;i++) {
    if (output_buffer[i]>='A'&&output_buffer[i]<='F')
      output_buffer[i]&=0x0f;
  }
    
  write_line_raw(output_buffer,0,8+16*3+2+16);
}

void show_memory(void)
{
  unsigned char i;
  for(i=0;i<16;i++) {
    show_memory_line(mon_address);
    mon_address+=16;
  }
}

char reg_desc_line[80]="PC   IRQ  NMI  A  X  Y  Z  B  SP   FLAGS    $01   MAPLO   MAPHI";
#define REGLINE_PC 0
#define REGLINE_IRQ 5
#define REGLINE_NMI 10
#define REGLINE_A 15
#define REGLINE_X 18
#define REGLINE_Y 21
#define REGLINE_Z 24
#define REGLINE_B 27
#define REGLINE_SP 30
#define REGLINE_FLAGS 35
#define REGLINE_01 44
#define REGLINE_MAPLO 50
#define REGLINE_MAPHI 58
void show_registers(void)
{
  // Get hypervisor register backup area
  uint32_t freeze_slot_offset=address_to_freeze_slot_offset(0xFFD3640U);
  unsigned short value;

  freeze_slot_offset=freeze_slot_offset>>9L;
  
  lfill((long)output_buffer,' ',80);
  
  if (freeze_slot_offset==0xFFFFFFFFL) {
    write_line("? FROZEN REGISTERS NOT FOUND  ERROR",0);
    recolour_last_line(2);
  } else {
    sdcard_readsector(freeze_slot_start_sector+freeze_slot_offset);

    // Now show registers: First the description line
    write_line(reg_desc_line,0);

    // Now prepare the line of actual register values

    // $D640-$D67F is frozen as a single piece, so the offsets are $00, not $40 from the beginning

    // PC
    value=sector_buffer[0x08]+(sector_buffer[0x09]<<8);
    format_hex((long)&output_buffer[REGLINE_PC],value,4);
    
    // A
    value=sector_buffer[0x00];
    format_hex((long)&output_buffer[REGLINE_A],value,2);
    
    // X
    value=sector_buffer[0x01];
    format_hex((long)&output_buffer[REGLINE_X],value,2);
    
    // Y
    value=sector_buffer[0x02];
    format_hex((long)&output_buffer[REGLINE_Y],value,2);
    
    // Z
    value=sector_buffer[0x03];
    format_hex((long)&output_buffer[REGLINE_Z],value,2);
    
    // B
    value=sector_buffer[0x04];
    format_hex((long)&output_buffer[REGLINE_B],value,2);

    // SP
    value=sector_buffer[0x05]+(sector_buffer[0x06]<<8);
    format_hex((long)&output_buffer[REGLINE_SP],value,4);

    // $00/$01 CPU port
    value=sector_buffer[0x10];
    format_hex((long)&output_buffer[REGLINE_01],value,2);
    value=sector_buffer[0x11];
    output_buffer[REGLINE_01+2]='/';
    format_hex((long)&output_buffer[REGLINE_01+3],value,2);

    // FLAGS
    value=sector_buffer[0x07];
    output_buffer[REGLINE_FLAGS+0]=(value&0x80)?'N':'-';
    output_buffer[REGLINE_FLAGS+1]=(value&0x40)?'V':'-';
    output_buffer[REGLINE_FLAGS+2]=(value&0x20)?'E':'-';
    output_buffer[REGLINE_FLAGS+3]=(value&0x10)?'B':'-';
    output_buffer[REGLINE_FLAGS+4]=(value&0x08)?'D':'-';
    output_buffer[REGLINE_FLAGS+5]=(value&0x04)?'I':'-';
    output_buffer[REGLINE_FLAGS+6]=(value&0x02)?'Z':'-';
    output_buffer[REGLINE_FLAGS+7]=(value&0x01)?'C':'-';

    // MAPLO
    value=sector_buffer[0x0A]+(sector_buffer[0x0B]<<8);
    format_hex((long)&output_buffer[REGLINE_MAPLO],value,4);
    value=sector_buffer[0x0E];
    output_buffer[REGLINE_MAPLO+4]='/';
    format_hex((long)&output_buffer[REGLINE_MAPLO+5],value,2);

    // MAPHI
    value=sector_buffer[0x0C]+(sector_buffer[0x0D]<<8);
    format_hex((long)&output_buffer[REGLINE_MAPLO],value,4);
    value=sector_buffer[0x0F];
    output_buffer[REGLINE_MAPLO+4]='/';
    format_hex((long)&output_buffer[REGLINE_MAPLO+5],value,2);
    
    
    
    write_line(output_buffer,0);
    
  }
}

unsigned char parse_hex(void)
{
  unsigned char digits=0;
  hex_value=0;
  while(1) {
    switch(screen_line_buffer[screen_line_offset]) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      hex_value=hex_value<<4;
      hex_value|=screen_line_buffer[screen_line_offset]&0xf;
      digits++;
      screen_line_offset++;
      break;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
      hex_value=hex_value<<4;
      hex_value|=9+(screen_line_buffer[screen_line_offset]&0xf);
      digits++;
      screen_line_offset++;
      break;
    default:
      return digits;
    }
  }
  
}

unsigned char parse_address(void)
{
  // Try to read hex digits from screen_line_buffer[screen_line_offset].
  unsigned char digits=parse_hex();
  if (digits>7) {
    write_line("? ADDRESS TOO LONG  ERROR",0); recolour_last_line(2);
    write_line("(Addresses should consist of 1 - 7 hex digits).",0); recolour_last_line(7);
    return 1;
  }
  if (!digits) {
    // No digits, so use previous address
  } else {
    // Use the supplied address
    mon_address=hex_value;
  }
  return 0;
}

void freeze_monitor(void)
{

  setup_screen();

  // Flush input buffer
  while (PEEK(0xD610U)) POKE(0xD610U,0);

  show_registers();
  
  while(1)
    {
      read_line(screen_line_buffer,80);      
      screen_line_buffer[79]=0;
      write_line(screen_line_buffer,0);

      // Skip initial char for parsing routines
      screen_line_offset=1;
      
      // Command syntax purposely matches that of the Matrix Mode / UART monitor to avoid confusion
      switch(screen_line_buffer[0]) {
      case 0:
	// empty line - nothing to do
	break;
      case 'x': case 'X':
	// Exit monitor
	// Return screen to normal
	POKE(0xD054U,0);
	POKE(0xD018U,0x15); // VIC-II hot register, so should reset most display settings
	POKE(0xD016U,0xC8);
	POKE(0xDD00U,PEEK(0xDD00U)|3); // video bank 0
	POKE(0xD031U,PEEK(0xD031U)&0x7f); // 40 columns
	return;
      case 'm': case 'M':
        // Display memory
	if (parse_address()) break;
	show_memory();
	break;
      case 'd': case 'D':
	// Disassemble memory
	break;
      case 'a': case 'A':
	// Assemble memory
	break;
      case 'r': case 'R':
	// Display register values
	show_registers();
	break;
      case 'f': case 'F':
	// Fill memory
	break;
      case 'h': case 'H':
	// Search (hunt) memory
	break;
      case 's': case 'S':
	// Set memory values
	break;
      default:
	write_line("Unknown command.",0);
	recolour_last_line(0x02);
	break;
      }
    }
  
  
}
