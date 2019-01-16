/*
  Disk chooser for freeze menu.

  It is displayed over the top of the normal freeze menu,
  and so we use that screen mode.

  We get our list of disknames and put them at $40000.
  As we only care about their names, and file names are
  limited to 64 characters, we can fit ~1000.
  In fact, we can only safely mount images with names <32
  characters.

  We return the disk image name or a NULL pointer if the
  selection has failed and $FFFF if the user cancels selection
  of a disk.
*/


#include <stdio.h>
#include <string.h>

#include "freezer.h"
#include "fdisk_hal.h"
#include "fdisk_memory.h"
#include "fdisk_screen.h"
#include "fdisk_fat32.h"
#include "ascii.h"


short file_count=0;
short selection_number=0;
short display_offset=0;

char *reading_disk_list_message="SCANNING DIRECTORY ...";

unsigned char normal_row[40]={
  0,1,0,1,0,1,0,1,
  0,1,0,1,0,1,0,1,
  0,1,0,1,0,1,0,1,
  0,1,0,1,0,1,0,1,
  0,1,0,1,0,1,0,1
};

unsigned char highlight_row[40]={
  0,0x21,0,0x21,0,0x21,0,0x21,0,0x21,0,0x21,0,0x21,0,0x21,
  0,0x21,0,0x21,0,0x21,0,0x21,0,0x21,0,0x21,0,0x21,0,0x21,
  0,0x21,0,0x21,0,0x21,0,0x21
};

void draw_disk_image_list(void)
{
  unsigned addr=SCREEN_ADDRESS;
  unsigned char i,x;
  unsigned char name[64];
  // First, clear the screen
  POKE(SCREEN_ADDRESS+0,' ');
  POKE(SCREEN_ADDRESS+1,0);
  POKE(SCREEN_ADDRESS+2,' ');
  POKE(SCREEN_ADDRESS+3,0);
  lcopy(SCREEN_ADDRESS,SCREEN_ADDRESS+4,40*2*25-4);

  for(i=0;i<23;i++) {
    if ((display_offset+i)<file_count) {
      // Real line
      lcopy(0x40000U+((display_offset+i)<<6),name,64);

      for(x=0;x<20;x++) {
	if ((name[x]>='A'&&name[x]<='Z') ||(name[x]>='a'&&name[x]<='z'))
	  POKE(addr+(x<<1),name[x]&0x1f);
	else
	  POKE(addr+(x<<1),name[x]);
      }
    } else {
      // Blank dummy entry
      for(x=0;x<40;x++) POKE(addr+(x<<1),' ');
    }
    if ((display_offset+i)==selection_number) {
      // Highlight the row
      lcopy((long)highlight_row,COLOUR_RAM_ADDRESS+(i*80),40);
    } else {
      // Normal row
      lcopy((long)normal_row,COLOUR_RAM_ADDRESS+(i*80),40);
    }
    addr+=(40*2);  
  }
  
}


char disk_name_return[32];
char *freeze_select_disk_image(void)
{
  unsigned char x,dir;
  struct m65_dirent *dirent;

  file_count=0;
  selection_number=0;
  display_offset=0;
  
  // First, clear the screen
  POKE(SCREEN_ADDRESS+0,' ');
  POKE(SCREEN_ADDRESS+1,0);
  POKE(SCREEN_ADDRESS+2,' ');
  POKE(SCREEN_ADDRESS+3,0);
  lcopy(SCREEN_ADDRESS,SCREEN_ADDRESS+4,40*2*25-4);

  for(x=0;reading_disk_list_message[x];x++)
    POKE(SCREEN_ADDRESS+12*40*2+(9*2)+(x*2),reading_disk_list_message[x]&0x3f);

  dir=opendir();

  dirent=readdir(dir);
  while(dirent&&((unsigned short)dirent!=0xffffU)) {
    x=strlen(dirent->d_name)-4;
    if (x>=0) {
      if ((!strncmp(&dirent->d_name[x],".D81",4))||(!strncmp(&dirent->d_name[x],".d81",4))) {
	// File is a disk image
	lfill(0x40000L+(file_count*64),' ',64);
	lcopy((long)&dirent->d_name[0],0x40000L+(file_count*64),x);
	file_count++;
      }
    }
    
    dirent=readdir(dir);
  }

  closedir(dir);

  // If we didn't find any disk images, then just return
  if (!file_count) return NULL;

  // Okay, we have some disk images, now get the user to pick one!
  draw_disk_image_list();
  while(1) {
    x=PEEK(0xD610U);

    if (!x) continue;

    // Clear read key
    POKE(0xD610U,0);

    switch(x) {
    case 0x03:             // RUN-STOP = make no change
      return NULL;
    case 0x11: case 0x9d:  // Cursor down or left
      selection_number++;
      if (selection_number>=file_count) selection_number=0;
      break;
    case 0x91: case 0x1d:  // Cursor up or right
      selection_number--;
      if (selection_number<0) selection_number=file_count-1;
      break;
    }

    // Adjust display position
    if (selection_number<display_offset) display_offset=selection_number;
    if (selection_number>(display_offset+23)) display_offset=selection_number-22;
    if (display_offset>(file_count-22)) display_offset=file_count-22;
    if (display_offset<0) display_offset=0;

    draw_disk_image_list();
    
  }
  
  return NULL;
}
