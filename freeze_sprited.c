/* 
 * SPRED65 - The MEGA65 sprite editor  
 *
 * Copyright (c) 2020 Hernán Di Pietro, Paul Gardner-Stephen.
 * 
 *  This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
    Version   0.8
    Date      2020-08-02

    CHANGELOG

    v0.5        Uses conio for proper initialization and some of its
                new features.  Color selection with MEGA/CTRL keys.

    v0.6        FIX: Screen moved to $12000. 
                Multicolor and 16-color sprite support. 
                New color selection UI.
                Change sprite type on-the-fly.with * key.
                Clear sprite key.
                
    v0.8        80x50 screen mode option, new UI.
                Redesigned key scheme.
                Supports 16-bit sprite data pointers (SPRPTR16).
                Honours VIC-II Bank bits ($DD00) if SPRPTR16 is OFF.
                16-color sprite uses 64-bit implicit width.
                

                
 */

#define TEST_SPRITES

#include <cc65.h>
#include "../mega65-libc/cc65/include/conio.h"
#include "../mega65-libc/cc65/include/mouse.h"
#include <cbm.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "../mega65-libc/cc65/include/memory.h"

extern int errno;

#define VIC_BASE                    0xD000UL
#define REG_HOTREG                  (VIC_BASE + 0x5D)
#define REG_SPRPTR_B0               (PEEK(VIC_BASE + 0x6C))
#define REG_SPRPTR_B1               (PEEK(VIC_BASE + 0x6D))
#define REG_SPRPTR_B2               (PEEK(VIC_BASE + 0x6E) & 0x7F)
#define REG_SPRPTR16                (PEEK(VIC_BASE + 0x6E) & 0x80)
#define REG_SPR_16COL               (VIC_BASE + 0x6B)
#define REG_SPR_MULTICOLOR          (VIC_BASE + 0x1C)
#define REG_SPRX64EN                (VIC_BASE + 0x57)
#define IS_SPR_MULTICOLOR(n)        ((PEEK(REG_SPR_MULTICOLOR)) & (1 << (n)))
#define IS_SPR_16COL(n)             ((PEEK(REG_SPR_16COL)) & (1 << (n)))
#define IS_SPR_XWIDTH(n)            ((PEEK(REG_SPRX64EN)) & (1 << (n)))
#define SPRITE_POINTER_ADDR         (((long)REG_SPRPTR_B0) | ((long)REG_SPRPTR_B1 << 8) | ((long)REG_SPRPTR_B2 << 16))

#define SCREEN_ROWS                 25
#define SCREEN_COLS                 80
#define CANVAS_HEIGHT               SCREEN_ROWS - 2

#define TRUE                        1
#define FALSE                       0
#define SPRITE_MAX_COUNT            8
#define DEFAULT_BORDER_COLOR        6
#define DEFAULT_SCREEN_COLOR        6
#define DEFAULT_BACK_COLOR          11
#define DEFAULT_FORE_COLOR          1
#define DEFAULT_MULTI1_COLOR        3
#define DEFAULT_MULTI2_COLOR        4
#define TRANS_CHARACTER             230
#define SIDEBAR_COLUMN              65
#define JOY_DELAY                   10000U

#define MIN(a,b)                    ((a)<(b) ? (a):(b))
#define MAX(a,b)                    ((a)>(b) ? (a):(b))

// Screen RAM for our area. We do not use 16-bit character mode
// so we need 80x25 = 2K area. 
#define SCREEN_RAM_ADDRESS          0x12000UL
#define CHARSET_ADDRESS             0x15000UL

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

typedef enum tagDRAWING_TOOL
{
    DRAWING_TOOL_PIXEL = 0,
    DRAWING_TOOL_BOX,
    DRAWING_TOOL_FILLEDBOX,
    DRAWING_TOOL_CIRCLE,
    DRAWING_TOOL_FILLED_CIRCLE,
    DRAWING_TOOL_LINE
} DRAWING_TOOL;

typedef struct tagAPPSTATE
{
    unsigned int spriteSizeBytes;
    long spriteDataAddr;
    BYTE spriteNumber;
    SPR_COLOR_MODE spriteColorMode;
    BYTE spriteWidth, spriteHeight;
    BYTE cellsPerPixel, bytesPerRow, pixelsPerByte;
    BYTE color[4];
    BYTE color_source[4];
    BYTE currentColorIdx;
    BYTE cursorX, cursorY;
    BYTE canvasLeftX;
    DRAWING_TOOL drawingTool;
    BYTE toolActive, toolOrgX, toolOrgY;
    void (*drawCellFn)(BYTE,BYTE);
    void (*paintCellFn)(void);
} APPSTATE;

typedef struct tagFILEOPTIONS
{
    BYTE mode;
    BYTE name[16];
} FILEOPTIONS;

static APPSTATE g_state;

static const char *clrIndexName[] = {"bg ", "fg ", "mc1", "mc2"};

void UpdateSpriteParameters(void);

unsigned char sprite_pointer[63]={
  0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xFC, 0x00, 0x00, 0xFC, 0x00, 0x00, 0xDC, 
  0x00, 0x00, 0xCE, 0x00, 0x00, 0xCE, 0x00, 0x00, 0x07, 0x00, 0x00, 0x07, 0x00, 0x00, 0x03, 0x80, 
  0x00, 0x03, 0x80, 0x00, 0x01, 0xC0, 0x00, 0x01, 0xC0, 0x00, 0x00, 0xE0, 0x00, 0x00, 0xE0, 0x00, 
  0x00, 0x70, 0x00, 0x00, 0x70, 0x00, 0x00, 0x38, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00
};

#ifdef TEST_SPRITES

    // Sprite 1 to test Multicolor

    unsigned char test_mc [64] ={
    0x00,0x38,0x00,0x00,0xea,0x00,0x03,0x6a,
    0x80,0x01,0xb6,0x80,0x0f,0x6a,0xa0,0x0d,
    0xda,0x60,0x3f,0x6a,0xa8,0x3d,0xb6,0xa8,
    0x37,0x6a,0xa4,0x3d,0xea,0xa8,0x3f,0x7a,
    0x68,0x3d,0xa6,0xa8,0x0f,0x6a,0xa0,0x0d,
    0xda,0xa0,0x03,0x6a,0x80,0x01,0xa6,0x80,
    0x00,0x7a,0x00,0x00,0x28,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x83};

    // Sprite 2 to test 16-color

    unsigned char test_16 [8*21]={
    
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x10,
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x10,
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x10,
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x10,
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x10,
    
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x10,
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x10,
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x10,
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x10,
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x10,

    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08 };
    
#endif 

/* Toolbox Character set, in order of DRAWING_TOOL... enumeration */

static const BYTE chsetToolbox[] = {
    // Pixel tool
    255,128,128,129,129,128,128,255, 255,1,1,129,129,1,1,255,
    
    // Box tool
    255,128,143,136,136,143,128,255, 255,1,241,17,17,241,1,255,

    // Circle tool
    255,129,130,132,132,130,129,255, 255,129,65,33,33,65,129,255,

    // Line tool
    255,128,184,135,128,128,128,255, 255,1,1,1,225,29,1,255,

    // Filled-box tool
    255,128,143,143,143,143,128,255, 255,1,241,241,241,241,1,255,

    // Filled-circle tool
    255,129,131,135,135,131,129,255, 255,129,193,225,225,193,129,255
};

#define TOOLBOX_CHARSET_BASE_IDX 240

static void Initialize()
{
    // Set 40MHz, VIC-IV I/O, 80 column, screen RAM @ $8000
    POKE(0, 65);

    conioinit();
    
    // Set sprite 0 to our cursor
    lcopy((long)sprite_pointer,0x380,63);
    POKE(0xD015,1);
    POKE(0x07F8,0x380/64);
    POKE(0x07F9,(0x380+64)/64);
    POKE(0x07FA,(0x380+128)/64);

    POKE(0xD000,100);
    POKE(0xD001,100);
    POKE(0xD027,7);
    POKE(0xD06C,0xF8);
    POKE(0xD06D,0x07);
    POKE(0xD06E,0x00);

    setextendedattrib(1);
    setscreensize(SCREEN_COLS, SCREEN_ROWS);
    setscreenaddr(SCREEN_RAM_ADDRESS);
    lcopy(0x2D800, CHARSET_ADDRESS, 2048);
    lcopy((long) chsetToolbox, CHARSET_ADDRESS  + TOOLBOX_CHARSET_BASE_IDX * 8, sizeof(chsetToolbox) );
    setcharsetaddr(CHARSET_ADDRESS);

#ifdef TEST_SPRITES
    POKE(53276UL, 2);
    POKE(REG_SPR_16COL, 4);

    lcopy((long)test_mc,0x380+64,64);
    lcopy((long)test_16,0x380+64+64,168);
#endif

    g_state.color[COLOR_BACK] = DEFAULT_BACK_COLOR;
    g_state.color[COLOR_FORE] = DEFAULT_FORE_COLOR;
    g_state.color[COLOR_MC1] = DEFAULT_MULTI1_COLOR;
    g_state.color[COLOR_MC2] = DEFAULT_MULTI2_COLOR;
    g_state.spriteNumber = 0;
    g_state.cursorX = g_state.cursorY = 0;
    g_state.currentColorIdx = COLOR_FORE;
    g_state.drawingTool = DRAWING_TOOL_PIXEL;
    g_state.toolActive = 0;
    g_state.toolOrgX = g_state.toolOrgY = 0;
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
    const long byteAddr = (g_state.spriteDataAddr + (y * 3)) + (x / 8);
    const BYTE p = lpeek(byteAddr) & ( 0x80 >> (x % 8));

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
    register BYTE cell = 0;
    const long byteAddr = (g_state.spriteDataAddr + (y * 8)) + (x / 2);
    const BYTE p = 0xF & (lpeek(byteAddr) >> (((x + 1) % 2) * 4));
    
    revers(1);
    gotoxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), y + 2);
    for (cell = 0; cell < g_state.cellsPerPixel; ++cell)
    {
        textcolor(p);
        cputc(p ? ' ' : TRANS_CHARACTER);
    }
    revers(0);
}

static void DrawMulticolorCell(BYTE x, BYTE y)
{
    register BYTE cell = 0;
    const long byteAddr = (g_state.spriteDataAddr + (y * 3)) + (x / 4);
    const BYTE b = lpeek(byteAddr);
    const BYTE p0 = b & (0x80 >> ( 2 * ( x % 4)));
    const BYTE p1 = b & (0x40 >> ( 2 * ( x % 4)));
    BYTE color = g_state.color[COLOR_BACK]; 
    if (!p0 && p1)
        color = g_state.color[COLOR_MC1];
    else if (p0 && !p1)
        color = g_state.color[COLOR_FORE];
    else if (p0 && p1)
        color = g_state.color[COLOR_MC2];

    revers(1);
    gotoxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), y + 2);
    for (cell = 0; cell < g_state.cellsPerPixel; ++cell)
    {
        textcolor(color);
        cputc(p0 | p1 ? ' ' : TRANS_CHARACTER);
    }
    revers(0);
}

static void PaintPixelMono()
{    
    const long byteAddr = (g_state.spriteDataAddr + (g_state.cursorY * 3)) + (g_state.cursorX / 8);
    const BYTE bitsel = 0x80 >> (g_state.cursorX % 8);
    const BYTE b = lpeek(byteAddr);
    lpoke(byteAddr, g_state.currentColorIdx == COLOR_BACK ? (b & ~bitsel) : (b | bitsel) );
}

static void PaintPixelMulti()
{    
    const long byteAddr = (g_state.spriteDataAddr + (g_state.cursorY * 3)) + (g_state.cursorX / 4);
    const BYTE b = lpeek(byteAddr);
    const BYTE bitsel = (2 * (g_state.cursorX % 4));
    const BYTE p0 = b & (0x80 >> bitsel);
    const BYTE p1 = b & (0x40 >> bitsel);
    const BYTE mask = ((0x80 >> bitsel) | (0x40 >> bitsel));
    if ((g_state.currentColorIdx == COLOR_BACK))
    {
        lpoke(byteAddr, lpeek(byteAddr) & ~mask);
    }
    else 
    {
        if (g_state.currentColorIdx == COLOR_FORE)
        {
            lpoke(byteAddr, lpeek(byteAddr) & ~mask | (0x80 >> bitsel) );
        }
        else if (g_state.currentColorIdx == COLOR_MC1)
        {
            lpoke(byteAddr, lpeek(byteAddr) & ~mask | (0x40 >> bitsel) );
        } 
        else if (g_state.currentColorIdx == COLOR_MC2)
        {
            lpoke(byteAddr, lpeek(byteAddr) & ~mask | ( (0x80 >> bitsel) | (0x40 >> bitsel) ));
        }
    }
}

static void PaintPixel16Color()
{    
    const long byteAddr = (g_state.spriteDataAddr + (g_state.cursorY * 8)) + (g_state.cursorX / 2);
    const BYTE bitsel = (((g_state.cursorX + 1) % 2) * 4);

    lpoke(byteAddr, lpeek(byteAddr) & (0xF0 >> bitsel) | (g_state.color[g_state.currentColorIdx] << bitsel));
}

static void ClearSprite()
{
    lfill(g_state.spriteDataAddr, 0, g_state.spriteSizeBytes);
}

void UpdateSpriteParameters(void)
{
    g_state.spriteHeight = 21;
    g_state.spriteSizeBytes = IS_SPR_XWIDTH(g_state.spriteNumber)  ? 168 : 64;
    g_state.bytesPerRow = g_state.spriteSizeBytes / g_state.spriteHeight;

    g_state.spriteDataAddr = REG_SPRPTR16 ? 64 * ( 
        ((long)lpeek(SPRITE_POINTER_ADDR + 1 + g_state.spriteNumber * 2) << 8) +
        ((long)lpeek(SPRITE_POINTER_ADDR + g_state.spriteNumber * 2)))        
         : (long) (64 * lpeek(SPRITE_POINTER_ADDR + g_state.spriteNumber)) | ( ((long) (~lpeek(0xDD00) & 0x3)) << 14);

    if (IS_SPR_16COL(g_state.spriteNumber))
    {
        g_state.drawCellFn = Draw16ColorCell;
        g_state.paintCellFn = PaintPixel16Color;
        g_state.spriteColorMode = SPR_COLOR_MODE_16COLOR;
        g_state.spriteWidth = 16; // Extended width is implied for 16-color sprites.
        g_state.cellsPerPixel = 3;
        g_state.pixelsPerByte = 2;
    }
    else if (IS_SPR_MULTICOLOR(g_state.spriteNumber))
    {
        g_state.drawCellFn = DrawMulticolorCell;
        g_state.paintCellFn = PaintPixelMulti;
        g_state.spriteColorMode = SPR_COLOR_MODE_MULTICOLOR;
        g_state.spriteWidth =  IS_SPR_XWIDTH(g_state.spriteNumber) ? 32 : 12;
        g_state.cellsPerPixel = 4;
        g_state.pixelsPerByte = 4;
    }
    else
    {
        g_state.drawCellFn = DrawMonoCell;
        g_state.paintCellFn = PaintPixelMono;
        g_state.spriteColorMode = SPR_COLOR_MODE_MONOCHROME;
        g_state.spriteWidth = IS_SPR_XWIDTH(g_state.spriteNumber) ? 64 : 24;
        g_state.cellsPerPixel = 2;
        g_state.pixelsPerByte = 8;
    }

    g_state.canvasLeftX =  (SIDEBAR_COLUMN / 2) - (g_state.spriteWidth * g_state.cellsPerPixel / 2);

}

static void EraseCanvasSpace()
{
    RECT rc;
    rc.top = 2;
    rc.left = 0;
    rc.right = SIDEBAR_COLUMN-1;
    rc.bottom = 23;
    fillrect(&rc, ' ', 1);
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
    cprintf("{home}{rvson}{lgrn}mega65 sprite editor v0.7                    copyright (c) 2020 hernan di pietro{rvsoff}");
}                                                             

static void DrawColorSelector()
{
    RECT rc = { SIDEBAR_COLUMN, 5, 80, 7 };
    fillrect(&rc, ' ', DEFAULT_SCREEN_COLOR);

    switch(g_state.spriteColorMode) {
        case SPR_COLOR_MODE_MONOCHROME:

            revers(1);
            textcolor(g_state.color[COLOR_BACK]);
            cputsxy(SIDEBAR_COLUMN, 5, "      ");
            textcolor(g_state.color[COLOR_FORE]);
            cputsxy(SIDEBAR_COLUMN + 8, 5, "      ");
            revers(0);
            
            textcolor(g_state.currentColorIdx == COLOR_BACK ? 1 : COLOUR_DARKGREY);
            cputsxy(SIDEBAR_COLUMN + 2, 6, "bk");
            
            textcolor(g_state.currentColorIdx == COLOR_FORE ? 1 : COLOUR_DARKGREY);
            cputsxy(SIDEBAR_COLUMN + 8 + 2, 6, "fg");
            
        break;

        case SPR_COLOR_MODE_16COLOR:
            revers(1);
            textcolor(g_state.color[COLOR_FORE]);
            cputsxy(SIDEBAR_COLUMN, 5, "               ");
            revers(0);
            
            textcolor(1);
            cputsxy(SIDEBAR_COLUMN + 2, 6, g_state.color[COLOR_FORE] == 0 ? "background":"foreground");
        break;

        case SPR_COLOR_MODE_MULTICOLOR:
            revers(1);
            textcolor(g_state.color[COLOR_BACK]);
            cputsxy(SIDEBAR_COLUMN, 5, "   ");
            textcolor(g_state.color[COLOR_FORE]);
            cputsxy(SIDEBAR_COLUMN + 4, 5, "   ");
            textcolor(g_state.color[COLOR_MC1]);
            cputsxy(SIDEBAR_COLUMN + 4 * 2, 5, "   ");
            textcolor(g_state.color[COLOR_MC2]);
            cputsxy(SIDEBAR_COLUMN + 4 * 3, 5, "   ");
            revers(0);
            
            textcolor(COLOUR_DARKGREY);
            cputsxy(SIDEBAR_COLUMN + 1, 6, "bk");
            cputsxy(SIDEBAR_COLUMN + 5, 6, "fg");
            cputsxy(SIDEBAR_COLUMN + 8, 6, "mc1");
            cputsxy(SIDEBAR_COLUMN + 12, 6, "mc2");
            
            textcolor(1);
            switch (g_state.currentColorIdx)
            {
                case COLOR_BACK: 
                    cputsxy(SIDEBAR_COLUMN + 1, 6, "bk");
                    break;
                case COLOR_FORE:
                    cputsxy(SIDEBAR_COLUMN + 5, 6, "fg");
                    break;
                case COLOR_MC1:
                    cputsxy(SIDEBAR_COLUMN + 8, 6, "mc1");
                    break;
                case COLOR_MC2:
                    cputsxy(SIDEBAR_COLUMN + 12, 6, "mc2");
                    break;
            }

        break;
    }
}

static void DrawToolbox()
{
    BYTE i = 0;
    for (i = 0; i < sizeof(chsetToolbox)/8; ++i)
    {
        if (g_state.drawingTool == i / 2 )
        {
            textcolor(COLOUR_WHITE);
        }
        else 
        {
            revers(0);
            textcolor(COLOUR_MEDIUMGREY);
        }
        cputc(TOOLBOX_CHARSET_BASE_IDX + i);
    }
}

static void DrawSidebar()
{
    BYTE i;

    DrawColorSelector();
    textcolor(1);
    gotoxy(SIDEBAR_COLUMN, 2);
    cputs("sprite ");
    cputdec(g_state.spriteNumber, 0, 0);
    cputs(g_state.spriteColorMode == SPR_COLOR_MODE_MONOCHROME ? " mono    " : 
        (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR ? " multi   " : " 16-col"));
    gotoxy(SIDEBAR_COLUMN, 3);
    textcolor(3);
    cputhex(g_state.spriteDataAddr, 7);
    cputc(' ');
    cputdec(g_state.cursorX, 0, 0);
    cputc(',');
    cputdec(g_state.cursorY, 0, 0);  

    gotoxy(SIDEBAR_COLUMN,8);
    DrawToolbox();

    DrawColorSelector();
    textcolor(14);


    // cputsxy(TOOLBOX_COLUMN, 10, ", . sel sprite");
    // cputsxy(TOOLBOX_COLUMN, 11, "spc draw");
    // if (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR)
    // {
    //     cputsxy(TOOLBOX_COLUMN, 12, "h   sel fgcolor");
    //     cputsxy(TOOLBOX_COLUMN, 13, "j   sel bkcolor");
    //     cputsxy(TOOLBOX_COLUMN, 14, "k   sel color1 ");
    //     cputsxy(TOOLBOX_COLUMN, 15, "l   sel color2 ");
    // }
    // else if (g_state.spriteColorMode == SPR_COLOR_MODE_MONOCHROME)
    // {
    //     cputsxy(TOOLBOX_COLUMN, 12, "j   sel fgcolor");
    //     cputsxy(TOOLBOX_COLUMN, 13, "k   sel bkcolor");
    //     cputsxy(TOOLBOX_COLUMN, 14, "               ");
    //     cputsxy(TOOLBOX_COLUMN, 15, "               ");
    // }
    // else 
    // {
    //     cputsxy(TOOLBOX_COLUMN, 12, "               ");
    //     cputsxy(TOOLBOX_COLUMN, 13, "               ");
    //     cputsxy(TOOLBOX_COLUMN, 14, "               ");
    //     cputsxy(TOOLBOX_COLUMN, 15, "               ");
    // }
    // cputsxy(TOOLBOX_COLUMN, 16, "*   change type");
    // cputsxy(TOOLBOX_COLUMN, 17, "s   save");
    // cputsxy(TOOLBOX_COLUMN, 18, "l   load");
    // cputsxy(TOOLBOX_COLUMN, 20, "c   clear");
    // cputsxy(TOOLBOX_COLUMN, 19, "f1  help/info");
    // cputsxy(TOOLBOX_COLUMN, 21, "f3  exit");

    // for(i = 10; i <= 21; ++i)
    // {
    //     cellcolor(SIDEBAR_COLUMN,   i, COLOUR_GREY3);
    //     cellcolor(SIDEBAR_COLUMN+1, i, COLOUR_GREY3);
    //     cellcolor(SIDEBAR_COLUMN+2, i, COLOUR_GREY3);
    // }
}

static void Ask(const char* question, char* outbuffer, unsigned char maxlen)
{
    gotoy(SCREEN_ROWS - 1);
    textcolor(COLOUR_PINK);
    revers(1);
    cputncxy(0, SCREEN_ROWS - 1, SCREEN_COLS, ' ');
    cputsxy(0, SCREEN_ROWS - 1, question);
    cinput(outbuffer, maxlen, CINPUT_ACCEPT_ALL);
    revers(0);
    textcolor(COLOUR_BLUE);
    cputncxy(0, SCREEN_ROWS - 1, SCREEN_COLS, ' ');
}

static BYTE SaveRawData(const BYTE name[16], char deviceNumber)
{
    return 0;
}

static BYTE LoadRawData(const BYTE name[16])
{
    return 0;
}

static void PrintKeyGroup(const char* list[], BYTE count, BYTE x, BYTE y)
{
    register BYTE i = 0;
    gotoxy(x,y);
    textcolor(COLOUR_PINK);
    revers(1);
    cputs(list[0]);
    revers(0);
    textcolor(COLOUR_WHITE);

    for(i = 1; i < count; ++i) 
    {
        gotoxy(x,y+i);
        cputs(list[i]);
    }
}

static void ShowHelp()
{
    const char* fileKeys[] = { 
        "       file        ", 
        "new          ctrl+n",
        "load             f5",
        "save             f7"};

    const char* drawKeys[] = { 
        "       tools       ",
        "pixel             p",
        "box               x",
        "filled box      s-x",
        "circle            o",
        "filled circle   s-o",
        "line              l"};

    const char* colorKeys[] = {
        "       color       ",
        "select         0..9",
        "               a..f",
        "prev component    \x1b",
        "next component    \x1d",
        "palette      ctrl+p" };
    
    flushkeybuf();
    clrscr();
    DrawHeader();
    PrintKeyGroup(fileKeys,  4, 0, 2);
    PrintKeyGroup(drawKeys,  7, 0, 2 +4+1 );
    PrintKeyGroup(colorKeys, 6, 0, 2 +4+1 +7+1);
    cgetc();
    
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
    bgcolor(DEFAULT_SCREEN_COLOR);    
    revers(0);
    clrscr();
    DrawHeader();
    DrawCanvas();
    DrawSidebar();
}

unsigned short joy_delay_countdown = 0;
unsigned char fire_lock = 0;

unsigned short mx,my;

static void MainLoop()
{
    FILEOPTIONS fileOpt;
    unsigned char buf[64];
    unsigned char key = 0, keymod = 0;
    BYTE redrawCanvas = FALSE;
    BYTE redrawTools = FALSE;

    mouse_set_bounding_box(0 + 24, 0 + 50, 319 + 24 - 8, 199 + 50);
    mouse_warp_to(24, 100);
    mouse_bind_to_sprite(0);

    while (1)
    {
        mouse_update_position(&mx, &my);
        if ((my >= 66 && my <= 233) && (mx >= 55 && mx <= 235))
        {
            if ((((mx - 55) / 8) != g_state.cursorX) || (((my - 66) / 8) != g_state.cursorY))
            {
                g_state.drawCellFn(g_state.cursorX, g_state.cursorY);
                redrawTools = TRUE;
                g_state.cursorX = (mx - 55) / 8;
                g_state.cursorY = (my - 66) / 8;
                DrawCursor();
                fire_lock = 0;
            }
        }
        if (kbhit())
        {
            key = cgetc();
            joy_delay_countdown = 0;
        }
        else
        {
            key = 0;
            if ((PEEK(0xDC00) & 0x1f) != 0x1f)
            {
                // Check joysticks

                if (!(PEEK(0xDC00) & 0x10))
                {
                    // Toggle pixel
                    if (!fire_lock)
                    {
                        key = 0x20;
                        joy_delay_countdown = joy_delay_countdown >> 3;
                    }
                    fire_lock = 1;
                }
                else
                    fire_lock = 0;

                if (joy_delay_countdown)
                    joy_delay_countdown--;
                else
                {
                    switch (PEEK(0xDC00) & 0xf)
                    {
                    case 0x7: // RIGHT
                        joy_delay_countdown = JOY_DELAY;
                        fire_lock = 0;
                        key = CH_CURS_RIGHT;
                        break;
                    case 0xB: // LEFT
                        fire_lock = 0;
                        joy_delay_countdown = JOY_DELAY;
                        key = CH_CURS_LEFT;
                        break;
                    case 0xE: // UP
                        fire_lock = 0;
                        joy_delay_countdown = JOY_DELAY;
                        key = CH_CURS_UP;
                        break;
                    case 0xD: // DOWN
                        fire_lock = 0;
                        joy_delay_countdown = JOY_DELAY;
                        key = CH_CURS_DOWN;
                        break;
                    default:
                        key = 0;
                    }
                }
            }
        }
        if (!key)
        {
            if (mouse_clicked())
            {
                if (!fire_lock)
                    key = 0x20;
                fire_lock = 1;
            }
        }
        switch (key)
        {
        case 0:
            // No key, do nothing
            break;

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

        case 104: // h
            redrawTools = TRUE;
            g_state.currentColorIdx = COLOR_BACK;
            DrawCursor();
            break;

        case 106: // j
            redrawTools = TRUE;
            g_state.currentColorIdx = COLOR_FORE;
            DrawCursor();
            break;

        case 107: // k
            redrawTools = TRUE;
            if (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR)
                g_state.currentColorIdx = COLOR_MC1;
            DrawCursor();
            break;

        case 108: // l
            redrawTools = TRUE;
            if (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR)
                g_state.currentColorIdx = COLOR_MC2;
            DrawCursor();
            break;

        case ',':
        case '.':

            if (key == ',' && g_state.spriteNumber++ == SPRITE_MAX_COUNT - 1)
                g_state.spriteNumber = 0;
            else if (key == '.' && g_state.spriteNumber-- == 0)
                g_state.spriteNumber = SPRITE_MAX_COUNT - 1;

            UpdateSpriteParameters();
            EraseCanvasSpace();
            g_state.cursorX = MIN(g_state.spriteWidth-1, g_state.cursorX);
            g_state.cursorY = MIN(g_state.spriteHeight-1, g_state.cursorY);
            redrawCanvas = redrawTools = TRUE;
            break;

        case '*':
            switch(g_state.spriteColorMode) {
                case SPR_COLOR_MODE_16COLOR:
                    // Switch to Hi-Res
                    POKE(REG_SPR_16COL, PEEK(REG_SPR_16COL) & ~(1 << g_state.spriteNumber));
                    POKE(REG_SPR_MULTICOLOR, PEEK(REG_SPR_MULTICOLOR) & ~(1 << g_state.spriteNumber));
                    break;
                case SPR_COLOR_MODE_MULTICOLOR:
                    // Switch to 16-col
                    POKE(REG_SPR_16COL, PEEK(REG_SPR_16COL) | (1 << g_state.spriteNumber));
                    POKE(REG_SPR_MULTICOLOR, PEEK(REG_SPR_MULTICOLOR) & ~(1 << g_state.spriteNumber));
                    break;
                case SPR_COLOR_MODE_MONOCHROME:
                    // Switch to Multicol
                    POKE(REG_SPR_16COL, PEEK(REG_SPR_16COL) & ~(1 << g_state.spriteNumber));
                    POKE(REG_SPR_MULTICOLOR, PEEK(REG_SPR_MULTICOLOR) | (1 << g_state.spriteNumber));
                    break;
            }
            UpdateSpriteParameters();
            EraseCanvasSpace();
            redrawCanvas = redrawTools = TRUE;
            break;

        case ' ':
            switch(g_state.drawingTool)
            {
                case DRAWING_TOOL_PIXEL:
                    g_state.paintCellFn();
                    break;
                
            }
            redrawCanvas = TRUE;
            break;

        
        case 0xF1: // F1
            ShowHelp();
            DrawScreen();
            break;
        
        /* --------------------------- FILE GROUP ----------------------------- */

        case 0xF3: // F3
            Ask("exit sprite editor: are you sure (yes/no)? ", buf, 3);
            if (buf[0] == 'y' && buf[1] == 'e' && buf[2] == 's'){
             return;
            }
            break;

        case 0xF5: // F5 LOAD
            Ask("enter file name to load: ", buf, 19);
            break;

        case 0xF7: // F7 SAVE
            Ask("enter file name to save: ", buf, 19);
            break;

        case 14: // CTRL-N
            Ask("clear sprite (yes/no)? ", buf, 3);
            if (buf[0] == 'y' && buf[1] == 'e' && buf[2] == 's'){
             redrawTools = redrawCanvas = TRUE;
             ClearSprite();
            }
            break;
        
        // case ASC_HELP:
        //     break;

        /* --------------------------- DRAWING TOOLS GROUP ----------------------- */

        case 120: // x = draw box
            g_state.toolOrgX = g_state.cursorX;
            g_state.toolOrgY = g_state.cursorY;
            g_state.drawingTool = DRAWING_TOOL_BOX;
            break;

        default:
            //gohome(); cputdec(key,0,0);
            
            if (key >= '0' && key <= '9')
            {
                g_state.color[g_state.currentColorIdx] = key - 48;
                redrawTools = redrawCanvas = TRUE;
            }
            else if (key >= 97 && key <= 102) //a..f
            {
                g_state.color[g_state.currentColorIdx] = 10 + key - 'a';
                redrawTools = redrawCanvas = TRUE;
            }
        }

        if (redrawCanvas)
            DrawCanvas();

        if (redrawTools)
            DrawSidebar();

        redrawCanvas = redrawTools = FALSE;
    }
}

void do_sprite_editor()
{
    int i = 0;
    Initialize();
    DrawScreen();
    MainLoop();
    DoExit();
}
