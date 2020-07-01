/* 
 * SPRED65 - The MEGA65 sprite editor  
 *
 * Copyright (c) 2020 Hernán Di Pietro.
 */

#include <cc65.h>
#include "../mega65-libc/cc65/include/conio.h"
#include <cbm.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "../mega65-libc/cc65/include/memory.h"

extern int errno;

#define VIC_BASE 0xD000UL
#define REG_HOTREG (VIC_BASE + 0x5D)
#define REG_SPRPTR_B0 (PEEK(VIC_BASE + 0x6C))
#define REG_SPRPTR_B1 (PEEK(VIC_BASE + 0x6D))
#define REG_SPRPTR_B2 (PEEK(VIC_BASE + 0x6E))
#define REG_SCREEN_RAM_ADDRESS 0x8000
#define REG_SCREEN_BASE_B0 (VIC_BASE + 0x60)
#define REG_SCREEN_BASE_B1 (VIC_BASE + 0x61)
#define REG_SCREEN_BASE_B2 (VIC_BASE + 0x62)
#define REG_SCREEN_BASE_B3 (VIC_BASE + 0x63) // Bits 0..3
#define IS_SPR_MULTICOLOR(n) ((PEEK(VIC_BASE + 0x1C)) & (1 << (n)))
#define IS_SPR_16COL(n) ((PEEK(VIC_BASE + 0x6B)) & (1 << (n)))
#define SPRITE_POINTER_ADDR (((long)REG_SPRPTR_B0) | ((long)REG_SPRPTR_B1 << 8) | ((long)REG_SPRPTR_B2 << 16))
#define TRUE 1
#define FALSE 0
#define SPRITE_MAX_COUNT 8
#define DEFAULT_BORDER_COLOR 6
#define DEFAULT_SCREEN_COLOR 6
#define DEFAULT_BACK_COLOR 11
#define DEFAULT_FORE_COLOR 1
#define DEFAULT_MULTI1_COLOR 3
#define DEFAULT_MULTI2_COLOR 4
#define TRANS_CHARACTER 230
#define TOOLBOX_COLUMN  65

// Index into color array
#define COLOR_BACK 0
#define COLOR_FORE 1
#define COLOR_MC1 2
#define COLOR_MC2 3

typedef unsigned char BYTE;
typedef unsigned char BOOL;

typedef enum tagSPR_COLOR_MODE
{
    SPR_COLOR_MODE_MONOCHROME,
    SPR_COLOR_MODE_MULTICOLOR,
    SPR_COLOR_MODE_16COLOR
} SPR_COLOR_MODE;

typedef struct tagAPPSTATE
{
    unsigned int spriteSizeBytes;
    long spriteDataAddr;
    BYTE spriteNumber;
    SPR_COLOR_MODE spriteColorMode;
    BYTE spriteWidth, spriteHeight;
    BYTE cellsPerPixel;
    BYTE color[4];
    BYTE currentColorIdx;
    BYTE cursorX, cursorY;
    BYTE canvasLeftX;
    void (*drawCellFn)(BYTE,BYTE);
} APPSTATE;

typedef struct tagFILEOPTIONS
{
    BYTE mode;
    BYTE name[16];
} FILEOPTIONS;

static APPSTATE g_state;

static const char *clrIndexName[] = {"bg", "fg", "mc1", "mc2"};

void UpdateSpriteParameters(void);

unsigned char sprite_pointer[63]={
  0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xFC, 0x00, 0x00, 0xFC, 0x00, 0x00, 0xDC, 
  0x00, 0x00, 0xCE, 0x00, 0x00, 0xCE, 0x00, 0x00, 0x07, 0x00, 0x00, 0x07, 0x00, 0x00, 0x03, 0x80, 
  0x00, 0x03, 0x80, 0x00, 0x01, 0xC0, 0x00, 0x01, 0xC0, 0x00, 0x00, 0xE0, 0x00, 0x00, 0xE0, 0x00, 
  0x00, 0x70, 0x00, 0x00, 0x70, 0x00, 0x00, 0x38, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00
};

static void Initialize()
{
    // Set 40MHz, VIC-IV I/O, 80 column, screen RAM @ $8000
    POKE(0, 65);
    POKE(0xD02fL, 0x47);
    POKE(0xd02fL, 0x53);

    // Set sprite 0 to our cursor
    lcopy((long)sprite_pointer,0x380,63);
    POKE(0xD015,1);
    POKE(0x07F8,0x380/64);
    POKE(0xD000,100);
    POKE(0xD001,100);
    POKE(0xD027,7);
    
    POKE(0xD031UL, 0xE0);  // Extended attributes + 80 mode
    POKE(REG_SCREEN_BASE_B0, SCREEN_ADDRESS & 0x0000FFUL);
    POKE(REG_SCREEN_BASE_B1, (SCREEN_ADDRESS & 0xFF00UL) >> 8);
    POKE(REG_SCREEN_BASE_B2, (SCREEN_ADDRESS & 0xFF0000UL) >> 16);
    POKE(REG_SCREEN_BASE_B3, (SCREEN_ADDRESS & 0xF000000UL) >> 24);
    POKE(0xD610U,0); // empty keyboard buffer

    g_state.color[COLOR_BACK] = DEFAULT_BACK_COLOR;
    g_state.color[COLOR_FORE] = DEFAULT_FORE_COLOR;
    g_state.color[COLOR_MC1] = DEFAULT_MULTI1_COLOR;
    g_state.color[COLOR_MC2] = DEFAULT_MULTI2_COLOR;
    g_state.spriteNumber = 0;
    g_state.cursorX = 0;
    g_state.cursorY = 0;
    g_state.currentColorIdx = COLOR_FORE;
    UpdateSpriteParameters();

    clrscr();
    bordercolor(DEFAULT_BORDER_COLOR);
    bgcolor(DEFAULT_SCREEN_COLOR);
}

static void DrawCursor()
{
    BYTE i = 0;
    revers(1);
    blink(1);
    gotoxy(g_state.canvasLeftX + (g_state.cursorX * g_state.cellsPerPixel), 2 + g_state.cursorY);
    textcolor(g_state.color[g_state.currentColorIdx]);

    for (i = 0; i < g_state.cellsPerPixel; ++i)
        cputc(219);
    blink(0);
    revers(0);
}

static void DrawMonoCell(BYTE x, BYTE y)
{
    register BYTE cell = 0;
    long byteAddr = (g_state.spriteDataAddr + (y * 3)) + (x / 8);
    BYTE p = lpeek(byteAddr) & ( 0x80 >> (x % 8));

    revers(1);
    gotoxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), y + 2);
    for (cell = 0; cell < g_state.cellsPerPixel; ++cell)
    {
        textcolor(p ? g_state.color[COLOR_FORE] : g_state.color[COLOR_BACK]);
        cputc(p ? ' ' : TRANS_CHARACTER);
    }
    revers(0);
}

static void Draw16ColorCell(BYTE x, BYTE y)
{

}

static void DrawMulticolorCell(BYTE x, BYTE y)
{
    
}

void UpdateSpriteParameters(void)
{
    g_state.spriteHeight = 21;
    g_state.cellsPerPixel = 2;
    g_state.spriteSizeBytes = 64;
    g_state.spriteDataAddr = (long)g_state.spriteSizeBytes * lpeek(SPRITE_POINTER_ADDR + g_state.spriteNumber);

    if (IS_SPR_16COL(g_state.spriteNumber))
    {
        g_state.drawCellFn = Draw16ColorCell;
        g_state.spriteSizeBytes = 168;
        g_state.spriteColorMode = SPR_COLOR_MODE_16COLOR;
        g_state.spriteWidth = 16;
    }
    else if (IS_SPR_MULTICOLOR(g_state.spriteNumber))
    {
        g_state.drawCellFn = DrawMulticolorCell;
        g_state.spriteColorMode = SPR_COLOR_MODE_MULTICOLOR;
        g_state.spriteWidth = 24;
    }
    else
    {
        g_state.drawCellFn = DrawMonoCell;
        g_state.spriteColorMode = SPR_COLOR_MODE_MONOCHROME;
        g_state.spriteWidth = 24;
    }

    g_state.canvasLeftX =  (TOOLBOX_COLUMN / 2) - (g_state.spriteWidth * g_state.cellsPerPixel / 2);

}

static void DrawCanvas()
{
    // TODO: Fetch all from memory with lcopy to local buffer and draw.

    register BYTE row = 0;
    register BYTE col = 0;
    for (row = 0; row < g_state.spriteHeight; ++row)
        for (col = 0; col < g_state.spriteWidth; ++col)
            g_state.drawCellFn(col, row);

    DrawCursor();
}

static void DrawHeader()
{
    textcolor(COLOUR_LIGHTGREEN);
    revers(1);
    cputsxy(0, 0, "mega65 sprite editor v0.5                    copyright (c) 2020 hernan di pietro");
    revers(0);
}

static void DrawToolbox()
{
    BYTE i;
    
    textcolor(1);
    gotoxy(TOOLBOX_COLUMN, 2);
    cputs("sprite ");
    cputdec(g_state.spriteNumber, 0, 0);
    cputs(g_state.spriteColorMode == SPR_COLOR_MODE_MONOCHROME ? " mono" : 
        (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR ? " multi" : " 16 color"));
    gotoxy(TOOLBOX_COLUMN, 3);
    textcolor(3);
    cputhex(g_state.spriteDataAddr, 7);
    cputc(' ');
    cputdec(g_state.cursorX, 0, 0);
    cputc(',');
    cputdec(g_state.cursorY, 0, 0);  

    textcolor(14);

    cputsxy(TOOLBOX_COLUMN, 5, "01234567");
    gotoxy(TOOLBOX_COLUMN, 6);
    revers(1);
    for (i = 0; i < 8; ++i)
    {
        textcolor(i);
        cputc(' ');
    }
    revers(0);
    textcolor(14);
    cputsxy(TOOLBOX_COLUMN, 8, "89abcdef");
    gotoxy(TOOLBOX_COLUMN, 7);
    revers(1);
    for (i = 8; i < 16; ++i)
    {
        textcolor(i);
        cputc(' ');
    }

    cputcxy(TOOLBOX_COLUMN + (g_state.color[g_state.currentColorIdx] % 8), 6 + (g_state.color[g_state.currentColorIdx] / 8), '*');
    revers(0);

    cputsxy(TOOLBOX_COLUMN + 11, 6, clrIndexName[g_state.currentColorIdx]);

    textcolor(14);
    cputsxy(TOOLBOX_COLUMN, 10, ", . sel sprite");
    cputsxy(TOOLBOX_COLUMN, 11, "spc draw");
    if (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR)
    {
        cputsxy(TOOLBOX_COLUMN, 15, "j  sel fgcolor");
        cputsxy(TOOLBOX_COLUMN, 12, "k  sel bkcolor");
        cputsxy(TOOLBOX_COLUMN, 13, "n  sel color1");
        cputsxy(TOOLBOX_COLUMN, 14, "m  sel color2");
    }
    else
    {
        cputsxy(TOOLBOX_COLUMN, 12, "j   sel fgcolor");
        cputsxy(TOOLBOX_COLUMN, 13, "k   sel bkcolor");
    }
    cputsxy(TOOLBOX_COLUMN, 16, "*   change type");
    cputsxy(TOOLBOX_COLUMN, 17, "s   save");
    cputsxy(TOOLBOX_COLUMN, 18, "l   load");
    cputsxy(TOOLBOX_COLUMN, 20, "z   palette");
    cputsxy(TOOLBOX_COLUMN, 19, "f1  help/info");
    cputsxy(TOOLBOX_COLUMN, 21, "f3  exit");

    for(i = 10; i <= 21; ++i)
    {
        ccellcolor(TOOLBOX_COLUMN,   i, COLOUR_GREY3);
        ccellcolor(TOOLBOX_COLUMN+1, i, COLOUR_GREY3);
        ccellcolor(TOOLBOX_COLUMN+2, i, COLOUR_GREY3);
    }
}

static void TogglePixel()
{
    long byteAddr = (g_state.spriteDataAddr + (g_state.cursorY * 3)) + (g_state.cursorX / 8);
    lpoke(byteAddr, lpeek(byteAddr) ^ (0x80 >> (g_state.cursorX % 8)));
}

static BYTE SaveRawData(const BYTE name[16], char deviceNumber)
{
    return cbm_save(name, deviceNumber, g_state.spriteDataAddr, g_state.spriteSizeBytes);
}

static BYTE LoadRawData(const BYTE name[16])
{
    // FILE *f = NULL;
    // register int i;
    // BYTE b;
    // _filetype = 's';

    // if (f = fopen(name, "rb"))
    // {
    //     for (i = 0; i < sizeof(appStatus->spritePixels)  && !feof(f); i += 2)
    //     {
    //         bordercolor(i & 15);
    //         b = fgetc(f);
    //         appStatus->spritePixels[i] = b >> 4;
    //         appStatus->spritePixels[i+1] = b & 0xF;
    //     }
    //     fclose(f);
    // }

    // bordercolor(DEFAULT_BORDER_COLOR);
    // return errno;
    return 0;
}

// static BOOL DrawDialog(BYTE x, BYTE y, BYTE w, BYTE h, unsigned char* title)
// {

// }

static BOOL SaveDialog(FILEOPTIONS *saveOpt)
{
    BYTE opt;
    textcolor(1);
    revers(1);
    // cputsxy(23, 17, "   Save to    ");
    // cputsxy(23, 18, "RAW or BASIC? ");
    // opt = cgetc();
    // if (opt == 'b')
    //     saveOpt->mode = SAVE_BASIC_LOADER;
    // else if (opt == 'r')
    //     saveOpt->mode = SAVE_RAW_BYTES;
    // else
    //     return FALSE;
    cputsxy(23, 17, "save filename?");
    cputsxy(23, 18, "              ");
    gotoxy(23, 18);
    cursor(1);
    cscanf("%s", saveOpt->name);
    cursor(0);
    revers(0);
    cputsxy(23, 17, "              ");
    cputsxy(23, 18, "              ");
    //return strlen(saveOpt->name) > 0 ? TRUE : FALSE;
    return TRUE;
}

static BOOL LoadDialog(FILEOPTIONS *opt)
{
    textcolor(1);
    revers(1);
    cputsxy(23, 17, "load filename?");
    cputsxy(23, 18, "              ");
    gotoxy(23, 18);
    cursor(1);
    cscanf("%s", opt->name);
    cursor(0);
    revers(0);
    cputsxy(23, 17, "              ");
    cputsxy(23, 18, "              ");
    //return strlen(opt->name) > 0 ? TRUE : FALSE;

    return TRUE;
}

static void EditColorDialog()
{
    textcolor(3);
    cputsxy(23, 17, "R ");
    textcolor(4);
    cputsxy(24, 17, "G ");
    textcolor(6);
    cputsxy(24, 17, "B ");
}

static void InfoDialog()
{
    textcolor(1);
    revers(1);
    gotoxy(23, 17);
    cputs("spraddr ");
    cputhex(g_state.spriteDataAddr, 7);
    cputsxy(23, 18, "              ");
}

static void DoExit()
{
    textcolor(14);
    bordercolor(14);
    bgcolor(6);
    clrscr();
}

static void DrawScreen()
{
    DrawHeader();
    DrawCanvas();
    DrawToolbox();
}

unsigned short joy_delay_countdown = 0;
unsigned short joy_delay = 10000;
unsigned char fire_lock = 0;

unsigned short mx,my;

unsigned short mouse_min_x=0;
unsigned short mouse_min_y=0;
unsigned short mouse_max_x=319;
unsigned short mouse_max_y=199;
unsigned short mouse_x=0;
unsigned short mouse_y=0;
unsigned char mouse_sprite_number=0xff;
unsigned char mouse_pot_x=0;
unsigned char mouse_pot_y=0;
char mouse_click_flag=0;

void mouse_set_bounding_box(unsigned short x1, unsigned short y1,unsigned short x2,unsigned short y2)
{
  mouse_min_x=x1;
  mouse_min_y=y1;
  mouse_max_x=x2;
  mouse_max_y=y2;  
}

void mouse_bind_to_sprite(unsigned char sprite_num)
{
  mouse_sprite_number=sprite_num;
}

void mouse_clip_position(void)
{
  if (mouse_x<mouse_min_x) mouse_x=mouse_min_x;
  if (mouse_y<mouse_min_y) mouse_y=mouse_min_y;
  if (mouse_x>mouse_max_x) mouse_x=mouse_max_x;
  if (mouse_y>mouse_max_y) mouse_y=mouse_max_y;
}

char mouse_clicked(void)
{
  if (!(PEEK(0xDC01)&0x80)) mouse_click_flag=1;
  if (mouse_click_flag) {
    mouse_click_flag=0;
    return 1;
  }
}

void mouse_update_pointer(void)
{
  if (mouse_sprite_number<8) {
    POKE(0xD000+(mouse_sprite_number<<1),mouse_x&0xff);
    if (mouse_x&0x100) POKE(0xD010,PEEK(0xD010)|(1<<mouse_sprite_number));
    else  POKE(0xD010,PEEK(0xD010)&(0xFF-(1<<mouse_sprite_number)));
    if (mouse_x&0x200) POKE(0xD05F,PEEK(0xD05F)|(1<<mouse_sprite_number));
    else  POKE(0xD05F,PEEK(0xD05F)&(0xFF-(1<<mouse_sprite_number)));

    POKE(0xD001+(mouse_sprite_number<<1),mouse_y&0xff);
    if (mouse_y&0x100) POKE(0xD077,PEEK(0xD077)|(1<<mouse_sprite_number));
    else  POKE(0xD077,PEEK(0xD077)&(0xFF-(1<<mouse_sprite_number)));
    if (mouse_y&0x200) POKE(0xD05F,PEEK(0xD05F)|(1<<mouse_sprite_number));
    else  POKE(0xD078,PEEK(0xD078)&(0xFF-(1<<mouse_sprite_number)));
  }

  gotoxy(0,0);
  cputdec(mouse_x, 0, 0);
  gotoxy(0,1);
  cputdec(mouse_y, 0, 0);
}

void mouse_update_position(unsigned short *mx,unsigned short *my)
{
  unsigned char delta;
  delta=PEEK(0xD620)-mouse_pot_x;
  mouse_pot_x=PEEK(0xD620);
  if (delta>=0x01&&delta<=0x3f) mouse_x+=delta;
  delta=-delta;
  if (delta>=0x01&&delta<=0x3f) mouse_x-=delta;

  delta=PEEK(0xD621)-mouse_pot_y;
  mouse_pot_y=PEEK(0xD621);
  if (delta>=0x01&&delta<=0x3f) mouse_y-=delta;
  delta=-delta;
  if (delta>=0x01&&delta<=0x3f) mouse_y+=delta;
  
  mouse_clip_position();
  mouse_update_pointer();

  if (!(PEEK(0xDC01)&0x80)) mouse_click_flag=1;
  
  if (mx) *mx=mouse_x;
  if (my) *my=mouse_y;
}

void mouse_warp_to(unsigned short x,unsigned short y)
{
  mouse_x=x;
  mouse_y=y;
  mouse_clip_position();
  mouse_update_pointer();

  // Mark POT position as read
  mouse_pot_x=PEEK(0xD620);
  mouse_pot_y=PEEK(0xD621);
}

static void MainLoop()
{
    FILEOPTIONS fileOpt;

    unsigned char key = 0;
    BYTE redrawCanvas = FALSE;
    BYTE redrawTools = FALSE;

    mouse_set_bounding_box(0+24,0+50,319+24-8,199+50);
    mouse_warp_to(24,100);
    mouse_bind_to_sprite(0);
    
    while (1)
    {
      mouse_update_position(&mx,&my);
      if ((my>=66&&my<=236)&&(mx>=66&&mx<=236)) {
	g_state.drawCellFn(g_state.cursorX, g_state.cursorY);
	if ((((mx-66)/8)!=g_state.cursorX)||(((my-66)/8)!=g_state.cursorY))
	  {
	    redrawCanvas = redrawTools = TRUE;
	    g_state.cursorX = (mx-66)/8;
	    g_state.cursorY = (my-66)/8;
	  }
      }
      if (mouse_clicked()) {
	key=0x20;
      }
      
      if (kbhit())
	{
	  key = cgetc();
	  joy_delay_countdown=0;
	}
      else {
	key=0;
	if ((PEEK(0xDC00)&0x1f)!=0x1f) {
	  // Check joysticks

	  if (!(PEEK(0xDC00)&0x10)) {
	    // Toggle pixel
	    if (!fire_lock) {
	      key = 0x20;
	      joy_delay_countdown=joy_delay_countdown>>3;
	    }
	    fire_lock = 1;	    
	  }
	  else fire_lock=0;
	  	  
	  if (joy_delay_countdown)
	    joy_delay_countdown--;
	  else {
	    switch(PEEK(0xDC00)&0xf) {
	    case 0x7:  // RIGHT
	      joy_delay_countdown=joy_delay;
	      fire_lock=0;
	      key = CH_CURS_RIGHT; break;
	    case 0xB: // LEFT
	      fire_lock=0;
	      joy_delay_countdown=joy_delay;
	      key = CH_CURS_LEFT; break;
	    case 0xE: // UP
	      fire_lock=0;
	      joy_delay_countdown=joy_delay;
	      key = CH_CURS_UP; break;
	    case 0xD: // DOWN
	      fire_lock=0;
	      joy_delay_countdown=joy_delay;
	      key = CH_CURS_DOWN; break;
	    default:
	      key = 0;
	    }
	  }
	}
      }
      switch (key)
        {
        case CH_CURS_DOWN:
            g_state.drawCellFn(g_state.cursorX, g_state.cursorY);
            redrawTools = TRUE;
            g_state.cursorY = (g_state.cursorY == g_state.spriteHeight - 1) ? 0 : (g_state.cursorY + 1);
            DrawCursor();
            break;

        case CH_CURS_UP:
            g_state.drawCellFn(g_state.cursorX, g_state.cursorY);
            redrawTools = TRUE;
            g_state.cursorY = (g_state.cursorY == 0) ? (g_state.spriteHeight - 1) : (g_state.cursorY - 1);
            DrawCursor();
            break;

        case CH_CURS_LEFT:
            g_state.drawCellFn(g_state.cursorX, g_state.cursorY);   
            g_state.cursorX = (g_state.cursorX == 0) ? (g_state.spriteWidth - 1) : (g_state.cursorX - 1);
            redrawTools = TRUE;
            DrawCursor();
            break;

        case CH_CURS_RIGHT:
            g_state.drawCellFn(g_state.cursorX, g_state.cursorY);
            redrawTools = TRUE;
            g_state.cursorX = (g_state.cursorX == g_state.spriteWidth - 1) ? 0 : (g_state.cursorX + 1);
            DrawCursor();
            break;

        case 106: // j
            redrawTools = TRUE;
            g_state.currentColorIdx = COLOR_FORE;
            break;

        case 107: // k
            redrawTools = TRUE;
            g_state.currentColorIdx = COLOR_BACK;
            break;

        case 110: // n
            redrawTools = TRUE;
            if (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR)
                g_state.currentColorIdx = COLOR_MC1;
            break;

        case 109: // m
            redrawTools = TRUE;
            if (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR)
                g_state.currentColorIdx = COLOR_MC2;
            break;

        case ',':
            if (g_state.spriteNumber++ == SPRITE_MAX_COUNT - 1)
                g_state.spriteNumber = 0;

            UpdateSpriteParameters();
            redrawCanvas = redrawTools = TRUE;
            break;

        case '.':
            if (g_state.spriteNumber-- == 0)
                g_state.spriteNumber = SPRITE_MAX_COUNT - 1;

            UpdateSpriteParameters();
            redrawCanvas = redrawTools = TRUE;
            break;

        case ' ':
            TogglePixel();
            redrawCanvas = TRUE;
            break;

        case 's': // s
            if (SaveDialog(&fileOpt))
            {
                SaveRawData(fileOpt.name, 8);
                redrawTools = redrawCanvas = TRUE;
            }
            break;

        case 'Z':
            EditColorDialog();
            break;

        case 'l': // l
            if (LoadDialog(&fileOpt))
            {
                LoadRawData(fileOpt.name);
                redrawTools = redrawCanvas = TRUE;
            }
            break;

        case 0xF3: // F3
            return;

        case '?':
            InfoDialog();
            break;

	case 0:
	  // No key, do nothing
	  break;
	    
        default:
            // color keys
            if (key >= '0' && key <= '9')
	      {
		g_state.color[g_state.currentColorIdx] = key - 48;
		redrawTools = redrawCanvas = TRUE;
	      }
            else if (key >= 0x41 && key <= 'f') {
                g_state.color[g_state.currentColorIdx] = 10 + key - 65;
		redrawTools = redrawCanvas = TRUE;
	    }

        }

        if (redrawCanvas)
            DrawCanvas();

        if (redrawTools)
            DrawToolbox();

        redrawCanvas = redrawTools = FALSE;
    }
}

void do_sprite_editor()
{
    Initialize();
    DrawScreen();
    MainLoop();
    DoExit();
}
