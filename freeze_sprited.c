/* 
 * SPRED65 - The MEGA65 sprite editor  
 *
 * Copyright (c) 2020-2021 Hernán Di Pietro, Paul Gardner-Stephen.
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
 
    Version   0.9
    Date      2021-05-28

    CHANGELOG

    v0.5        Uses conio for proper initialization and some of its
                new features.  Color selection with MEGA/CTRL keys.

    v0.6        FIX: Screen moved to $12000. 
                Multicolor and 16-color sprite support. 
                New color selection UI.
                Change sprite type on-the-fly.with * key.
                Clear sprite key.
                
    v0.8        80x50 screen mode option, new UI, redraw optimizations.
                Redesigned key scheme.
                Supports 16-bit sprite data pointers (SPRPTR16).
                Honours VIC-II Bank bits ($DD00) if SPRPTR16 is OFF.
                Honours VIC color registers.
                Supports extended width sprites (SPRX64EN)
                16-color sprite uses 64-bit implicit width.
                Drawing tools: pixel, box, circle ,lines.
                Sprite Test Mode.

    v0.9        Transfer to/from frozen sprite memory of registers and data.
                Display: 50-line mode, wide-screen/4:3 aspect ratio modes.
                Fixes buffer overrun in Ask() function.
                Sprite preview
                Fancy selection cursor
                H/V expand toggle
                Size optimizations


    TODO: 

    * Instead of re-drawing full canvas, we should invalidate 
      only the needed area.
    * Consider SPRBPMEN for 16-color sprites
 */

//#define TEST_SPRITES

#include <cc65.h>
#include "../mega65-libc/cc65/include/conio.h"
#include "../mega65-libc/cc65/include/mouse.h"
#include "../mega65-libc/cc65/include/hal.h"
#include <cbm.h>
#include <stdio.h>
#include <stdlib.h>
#include "../mega65-libc/cc65/include/memory.h"
#include "freezer.h"

extern int errno;

#define PAGE_SIZE                   256
#define VIC_BASE                    0xFFD3000UL  // This is where VIC-II is mapped in frozen memory
#define LOCAL_VIC_BASE              0xD000
#define REG_SPRPTR_B0               (freeze_peek(VIC_BASE + 0x6CUL))
#define REG_SPRPTR_B1               (freeze_peek(VIC_BASE + 0x6DUL))
#define REG_SPRPTR_B2               (freeze_peek(VIC_BASE + 0x6EUL) & 0x7F)
#define REG_SPRPTR16                (freeze_peek(VIC_BASE + 0x6EUL) & 0x80)
#define REG_SPR_16COL               (VIC_BASE + 0x6BUL)
#define REG_SPR_MULTICOLOR          (VIC_BASE + 0x1CUL)
#define REG_SPRX64EN                (VIC_BASE + 0x57UL)
#define REG_SPRITE_MULTICOL1        (VIC_BASE + 0x25UL)
#define REG_SPRITE_MULTICOL2        (VIC_BASE + 0x26UL)
#define REG_SPRITE_COLOR(n)         (VIC_BASE + 0x27UL + (n))
#define REG_SPRPALSEL               (VIC_BASE + 0x70UL)

#define LOCAL_REG_SPR_16COL               (LOCAL_VIC_BASE + 0x6BUL)
#define LOCAL_REG_SPR_MULTICOLOR          (LOCAL_VIC_BASE + 0x1CUL)
#define LOCAL_REG_SPRX64EN                (LOCAL_VIC_BASE + 0x57UL)
#define LOCAL_REG_SPRITE_MULTICOL1        (LOCAL_VIC_BASE + 0x25UL)
#define LOCAL_REG_SPRITE_MULTICOL2        (LOCAL_VIC_BASE + 0x26UL)
#define LOCAL_REG_SPRITE_COLOR(n)         (LOCAL_VIC_BASE + 0x27UL + (n))
#define LOCAL_REG_SPRPALSEL               (LOCAL_VIC_BASE + 0x70UL)

#define SPRITE_PALETTE              ((freeze_peek(REG_SPRPALSEL) & 0x30) >> 4)
#define IS_SPR_MULTICOLOR(n)        ((freeze_peek(REG_SPR_MULTICOLOR)) & (1 << (n)))
#define IS_SPR_16COL(n)             ((freeze_peek(REG_SPR_16COL)) & (1 << (n)))
#define IS_SPR_XWIDTH(n)            ((freeze_peek(REG_SPRX64EN)) & (1 << (n)))
#define SPRITE_POINTER_ADDR         (((long)REG_SPRPTR_B0) | ((long)REG_SPRPTR_B1 << 8) | ((long)REG_SPRPTR_B2 << 16))
#define SPRITE_SIZE_BYTES(n)        (( IS_SPR_XWIDTH( (n) ) | IS_SPR_16COL( (n) ))  ? 168 : 64) 
#define SPRITE_DATA_ADDR(n)         (REG_SPRPTR16 ? 64 * (                                  \
                                    ((long)freeze_peek(SPRITE_POINTER_ADDR + 1 + n * 2) << 8) +   \
                                    ((long)freeze_peek(SPRITE_POINTER_ADDR + n * 2)))             \
                                    : (long) (64 * freeze_peek(SPRITE_POINTER_ADDR + n)) | ( ((long) (~freeze_peek(0xFFD3D00UL) & 0x3)) << 14))
// #define REG_SPRBPMEN_0_3            (vic_registers[0x49] >> 4)
// #define REG_SPRBPMEN_4_7            (vic_registers[0x4B] >> 4)
// #define SPRITE_BITPLANE_ENABLE(n)	(((REG_SPRBPMEN_4_7) << 4 | REG_SPRBPMEN_0_3) & (1 << (n)))
#define SCREEN_ROWS                 25
#define SCREEN_COLS                 80
#define CANVAS_HEIGHT               (SCREEN_ROWS - 2)
#define CANVAS_TOP_MARGIN           2

#define TRUE                        1
#define FALSE                       0
#define SPRITE_MAX_COUNT            8
#define DEFAULT_BORDER_COLOR        6
#define DEFAULT_SCREEN_COLOR        6
#define DEFAULT_BACK_COLOR          11

#define TRANS_CHARACTER             230
#define CURSOR_CHARACTER            219
#define SOLID_BLOCK_CHARACTER       224
#define SHAPE_PREVIEW_CHARACTER     32
#define SIDEBAR_COLUMN              65
#define SIDEBAR_WIDTH               (SCREEN_COLS - SIDEBAR_COLUMN)
#define SIDEBAR_PREVIEW_AREA_TOP    10
#define SIDEBAR_PREVIEW_AREA_BOTTOM 20
#define SIDEBAR_PREVIEW_AREA_HEIGHT (SIDEBAR_PREVIEW_AREA_BOTTOM - SIDEBAR_PREVIEW_AREA_TOP)
#define SPRITE_OFFSET_X             23
#define SPRITE_OFFSET_Y             50

#define JOY_DELAY                   10000U

#define MIN(a,b)                    ((a)<(b) ? (a):(b))
#define MAX(a,b)                    ((a)>(b) ? (a):(b))
#define ABS8(x)                     (((x) ^ ((x) >> 7)) -  ((x) >> 7))
#define ABS16(x)                    (((x) ^ ((x) >> 15)) -  ((x) >> 15))

// Screen RAM for our area. We do not use 16-bit character mode
// so we need 80x25 = 2K area. 
#define SCREEN_RAM_ADDRESS          0x12000UL
#define CHARSET_ADDRESS             0x15000UL
#define SPRITE_POINTER_TABLE        0x16000UL
#define SPRITE_BUFFER               0x40000UL
#define PREVIEW_SPRITE_NUM          2

// Redraw side-bar flags
#define REDRAW_SB_NONE   0
#define REDRAW_SB_INFO   1
#define REDRAW_SB_COORD  2
#define REDRAW_SB_COLOR  4
#define REDRAW_SB_TOOLS  8
#define REDRAW_SB_ALL    1 + 2 + 4 + 8

// Screen palette mode
#define SCRPALSEL_TEXT          0  // Default text display palette
#define SCRPALSEL_SPRITE        1  // Sprite palette/fixed 248-255 entries 
#define SCRPALSEL_SPRITE_ALL    2  // Sprite palette/full entries

typedef unsigned char BYTE;
typedef unsigned char BOOL;

#define SPR_COLOR_MODE_MONOCHROME       0
#define SPR_COLOR_MODE_MULTICOLOR       1
#define SPR_COLOR_MODE_16COLOR          2

// Drawing tools
#define DRAWING_TOOL_PIXEL              0
#define DRAWING_TOOL_LINE               1
#define DRAWING_TOOL_BOX                2
#define DRAWING_TOOL_FILLEDBOX          3
#define DRAWING_TOOL_CIRCLE             4
#define DRAWING_TOOL_FILLED_CIRCLE      5

// Index into color array
#define COLOR_BACK          0   
#define COLOR_FORE          1
#define COLOR_MC1           2
#define COLOR_MC2           3

typedef struct tagAPPSTATE
{
    BYTE wideScreenMode;
    BYTE screenPalette;
    BYTE spriteNumber;
    BYTE spriteColorMode;
    BYTE spriteWidth, spriteHeight;
    BYTE cellsPerPixel, bytesPerRow, pixelsPerByte;
    BYTE color[4];
    BYTE color_source[4];
    BYTE currentColorIdx;
    BYTE cursorX, cursorY;
    BYTE canvasLeftX;
    BYTE drawingTool;
    BYTE toolActive, toolOrgX, toolOrgY, fillShape;
    BYTE redrawSideBarFlags; // See REDRAW_SB_ constants for flags
    RECT redrawRect;
    void (*drawCellFn)(BYTE,BYTE);
    void (*paintCellFn)(BYTE,BYTE);
    void (*drawShapeFn)(BOOL);
    unsigned int spriteSizeBytes;
    long spriteDataAddr;
} APPSTATE;

typedef struct tagFILEOPTIONS
{
    BYTE mode;
    BYTE name[16];
} FILEOPTIONS;

static APPSTATE g_state;
static unsigned long g_freezeSlotStartSector;

/* Nonstatic Function prototypes */
void UpdateSpriteParameters(void);
void SetDrawTool(BYTE);
void SetRedrawFullCanvas(void);
void SetEffectiveToolRect(RECT*);
void SetupTextPalette(void);

BYTE  sprite_pointer[63]={
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

    // ------ UPPER ROW -----

    // Pixel tool 
    0,0,255,128,128,128,128,129, 0,0,255,1,1,1,1,129,

    // Line tool
    0,0,255,128,128,176,140,131, 0,0,255,1,1,1,1,1,
    
    // Box tool
    0,0,255,128,128,159,144,144,  0,0,255,1,1,249,9,9,

    // Filled-box tool
    0,0,255,128,128,143,143,143, 0,0,255,1,1,241,241,241,

    // Circle tool
    0,0,255,128,128,131,132,136, 0,0,255,1,1,193,33,17,

    // Filled-circle tool
    0,0,255,128,128,129,135,143, 0,0,255,1,1,129,225,241,

    // ------ LOWER ROW -------

     // Pixel tool 
    129,128,128,128,128,255,63,0, 129,1,1,1,1,255,255,0,
    
    // Line tool
    128,128,128,128,128,255,255,0, 193,49,13,1,1,255,255,0,

    // Box tool
    144,144,159,128,128,255,255,0,  9,9,249,1,1,255,255,0,
    
    // Filled-box tool
    143,143,143,128,128,255,255,0, 241,241,241,1,1,255,255,0,

    // Circle tool
     136,132,131,128,128,255,255,0, 17,33,193,1,1,255,255,0,

    // Filled-circle tool
    143,135,129,128,128,255,255,0, 241,225,129,1,1,255,255,0
};

#define TOOLBOX_CHARSET_BASE_IDX 232

static void SetRect(RECT* rc, BYTE left, BYTE top, BYTE right, BYTE bottom)
{
    rc->left   = left;
    rc->right  = right;
    rc->top    = top;
    rc->bottom = bottom;
}

static void Initialize()
{
    // Set 40MHz, VIC-IV I/O, 80 column, screen RAM @ $8000
    POKE(0, 65);

    // --- Freezer slot setup

    g_freezeSlotStartSector = find_freeze_slot_start_sector(0);
    request_freeze_region_list();

    // --- Screen setup ----

    conioinit();

    sethotregs(1);
  
    setextendedattrib(1);
    setscreensize(SCREEN_COLS, SCREEN_ROWS);
    setscreenaddr(SCREEN_RAM_ADDRESS);
    bordercolor(DEFAULT_BORDER_COLOR);
    bgcolor(DEFAULT_SCREEN_COLOR);

    // --- Charset setup ----

    lcopy(0x2D800, CHARSET_ADDRESS, 2048);
    lcopy((long) chsetToolbox, CHARSET_ADDRESS  + TOOLBOX_CHARSET_BASE_IDX * 8, sizeof(chsetToolbox) );
    setcharsetaddr(CHARSET_ADDRESS);


    // --- Sprite setup ---- 
    
    // Set pointer table to SPRITE_POINTER_TABLE
    // Set sprite 0 to our cursor. Address = $380.
    // Set sprite 1 to editing cursor #1   Address = $3C0
    // Set sprite 2 to current sprite placeholder.   Address = SPRITE_BUFFER  


    // Set local sprite pointer table

    POKE(0xD06C, (BYTE) SPRITE_POINTER_TABLE);
    POKE(0xD06D, (BYTE) (SPRITE_POINTER_TABLE >> 8));
    POKE(0xD06E, (BYTE) (SPRITE_POINTER_TABLE >> 16));
    POKE(0xD06E, PEEK(0xD06E) | 128); // Enable SPRPTR16

    // #0: Mouse Pointer sprite at 0x380
    
    lcopy((long) sprite_pointer, 0x380, 63);
    lpoke(SPRITE_POINTER_TABLE, 0x0E);
    lpoke(SPRITE_POINTER_TABLE + 1, 0x00);
    
    // Address of edit cursor
    lpoke(SPRITE_POINTER_TABLE + 2, 0x0F); //64 * 0xF = 0x3C0
    lpoke(SPRITE_POINTER_TABLE + 3, 0x00);

    // Address of current sprite image preview
    lpoke(SPRITE_POINTER_TABLE + 4, SPRITE_BUFFER / 64 % 256);
    lpoke(SPRITE_POINTER_TABLE + 5, SPRITE_BUFFER / 64 / 256);

    // Sprite properties (color, initial pos, etc.)

    POKE(0xD015, 7);            // Enable #0, #1, #2
    POKE(0xD000,100);
    POKE(0xD001,100);

    POKE(0xD002,0);
    POKE(0xD003,0);

    POKE(0xD027,7);
    POKE(0xD028,1);
    POKE(0xD010, 1 << PREVIEW_SPRITE_NUM);  // 8th bit for Sprite#2
    POKE(0xD01C,0); // All mono/hires sprites
    POKE(0xD06B,0); // 16-color mode OFF

#ifdef TEST_SPRITES
    POKE(53276UL, 2);
    POKE(REG_SPR_16COL, 4);

    lcopy((long)test_mc,0x380+64,64);
    lcopy((long)test_16,0x380+64+64,168);
#endif
    g_state.redrawSideBarFlags = REDRAW_SB_ALL;
  
    g_state.spriteNumber = 0;
    g_state.cursorX = g_state.cursorY = 0;
    g_state.currentColorIdx = COLOR_FORE;
    g_state.toolActive = 0;
    g_state.toolOrgX = g_state.toolOrgY = 0;
    g_state.color[COLOR_BACK] = DEFAULT_BACK_COLOR;
    g_state.wideScreenMode = 0;
    g_state.screenPalette = SCRPALSEL_SPRITE; 

    SetupTextPalette();
    SetDrawTool(DRAWING_TOOL_PIXEL);
    UpdateSpriteParameters();
    SetRedrawFullCanvas();
}

void SetupTextPalette(void)
{
    // To properly display color in the editor, we match the sprite bank
    // to the text/bitmap one.  Last 8 colors are fixed for display chores.

    //POKE(0xD070, SPRITE_PALETTE);
    POKE(0xD070UL, (PEEK(0xD070UL) & ~0x30) | (freeze_peek(REG_SPRPALSEL) & 0x30));
    setmapedpal(SPRITE_PALETTE);
}

static void DrawCursor(BYTE x, BYTE y)
{
    textcolor(g_state.color[g_state.currentColorIdx]);
    cputncxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), 2 + y, g_state.cellsPerPixel, CURSOR_CHARACTER);
}

static void DrawShapeChar(BYTE x, BYTE y)
{   
    textcolor(g_state.color[g_state.currentColorIdx]);
    cputncxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), 2 + y, g_state.cellsPerPixel, SHAPE_PREVIEW_CHARACTER);
}

static void DrawLine(BOOL bPreview)
{
    RECT rc;
    void (*pfun)(BYTE, BYTE) = bPreview ? DrawShapeChar : g_state.paintCellFn;
    SetEffectiveToolRect(&rc);

    if (g_state.toolOrgY == g_state.cursorY) // Horizontal
    {
        register BYTE x = rc.left;
        while (x <= rc.right)
            pfun(x++, g_state.cursorY);
    }
    else if (g_state.toolOrgX == g_state.cursorX) // Vertical
    {
        register BYTE y = rc.top;
        while (y <= rc.bottom)
            pfun(g_state.cursorX, y++);
    }
    else // Bresenham- algorithm.
    {
        const signed char dx = rc.right - rc.left;
        const signed char dy = -(rc.bottom - rc.top);
        BYTE x = g_state.toolOrgX, y = g_state.toolOrgY;
        signed char e = dx + dy;
        signed char e2 = 0;
        signed char sx = g_state.cursorX > g_state.toolOrgX ? 1 : -1;
        signed char sy = g_state.cursorY > g_state.toolOrgY ? 1 : -1;

        for(;;)
        {
            pfun(x, y);
            if(x == g_state.cursorX && y == g_state.cursorY)
                break;
            e2 = e * 2;
            if (e2 >= dy)
            {
                e += dy;
                x += sx;
            }
            if (e2 <= dx)
            {
                e += dx;
                y += sy;
            }
        }
    }
}

static void DrawCircle(BOOL bPreview)
{
    // RECT rc;
    // void (*pfun)(BYTE, BYTE) = bPreview ? DrawShapeChar : g_state.paintCellFn;
    // SetEffectiveToolRect(&rc);

    // const signed char dx = rc.right - rc.left;
    // const signed char dy = -(rc.bottom - rc.top);
    // BYTE x = g_state.toolOrgX, y = g_state.toolOrgY;
    // signed char e = dx + dy;
    // signed char e2 = 0;
    // signed char sx = g_state.cursorX > g_state.toolOrgX ? 1 : -1;
    // signed char sy = g_state.cursorY > g_state.toolOrgY ? 1 : -1;

    // for (;;)
    // {
    //     pfun(x, y);
    //     if (x == g_state.cursorX && y == g_state.cursorY)
    //         break;
    //     e2 = e * 2;
    //     if (e2 >= dy)
    //     {
    //         e += dy;
    //         x += sx;
    //     }
    //     if (e2 <= dx)
    //     {
    //         e += dx;
    //         y += sy;
    //     }
    // }
}

static void DrawBox(BOOL bPreview)
{
    RECT rc;
    register BYTE x, y, i;
    void(*pfun)(BYTE,BYTE) = bPreview ? DrawShapeChar : g_state.paintCellFn;
    SetEffectiveToolRect(&rc);

    x = rc.left;
    while (x <= rc.right)
    {
        pfun(x,g_state.cursorY);
        if (g_state.fillShape) 
        {
            for(i = rc.top+1 ; i < rc.bottom ; ++i) 
            {
                pfun(x,i);
            }
        }
        pfun(x++,g_state.toolOrgY);
    }

    y = rc.top;
    while (y <= rc.bottom)
    {
        pfun(g_state.cursorX, y);
        pfun(g_state.toolOrgX, y++);
    }
}

void SetDrawTool(BYTE dt)
{
    g_state.drawingTool = dt;
    switch(dt)
    {
        case DRAWING_TOOL_BOX:   
            g_state.fillShape = 0;
            g_state.drawShapeFn = DrawBox; 
            break;
        case DRAWING_TOOL_FILLEDBOX:
            g_state.fillShape = 1;
            g_state.drawShapeFn = DrawBox; 
            break;
        case DRAWING_TOOL_LINE:  
            g_state.drawShapeFn = DrawLine; 
            break;
    }
}

static void DrawMonoCell(BYTE x, BYTE y)
{
    register BYTE cell = 0;
    const long byteAddr = (SPRITE_BUFFER + (y * g_state.spriteWidth / 8)) + (x / 8);
    const BYTE p = lpeek(byteAddr) & ( 0x80 >> (x % 8));

    gotoxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), y + 2);
    for (cell = 0; cell < g_state.cellsPerPixel; ++cell)
    {
        textcolor(p ? g_state.color[COLOR_FORE] : g_state.color[COLOR_BACK]);
        cputc(p ? SOLID_BLOCK_CHARACTER : TRANS_CHARACTER);
    }
}

static void Draw16ColorCell(BYTE x, BYTE y)
{
    register BYTE cell = 0;
    const long byteAddr = (SPRITE_BUFFER + (y * 8)) + (x / 2);
    const BYTE p = 0xF & (lpeek(byteAddr) >> (((x + 1) % 2) * 4));
    const BYTE col = (g_state.spriteNumber * 16) + p;
    
    gotoxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), y + 2);
    for (cell = 0; cell < g_state.cellsPerPixel; ++cell)
    {
        textcolor(col);
        cputc(p ? SOLID_BLOCK_CHARACTER : TRANS_CHARACTER);
    }
}

static void DrawMulticolorCell(BYTE x, BYTE y)
{
    register BYTE cell = 0;
    const long byteAddr = (SPRITE_BUFFER + (y * g_state.spriteWidth / 4)) + (x / 4);
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

    gotoxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), y + 2);
    for (cell = 0; cell < g_state.cellsPerPixel; ++cell)
    {
        textcolor(color);
        cputc(p0 | p1 ? SOLID_BLOCK_CHARACTER : TRANS_CHARACTER);
    }
}

static void PaintPixelMono(BYTE x, BYTE y)
{    
    const long byteAddr = (SPRITE_BUFFER + (y * g_state.spriteWidth / 8)) + (x / 8);
    const BYTE bitsel = 0x80 >> (x % 8);
    const BYTE b = lpeek(byteAddr);
    lpoke(byteAddr, g_state.currentColorIdx == COLOR_BACK ? (b & ~bitsel) : (b | bitsel) );
}

static void PaintPixelMulti(BYTE x, BYTE y)
{    
    const long byteAddr = (SPRITE_BUFFER + (y * g_state.spriteWidth / 4)) + (x / 4);
    const BYTE b = lpeek(byteAddr);
    const BYTE bitsel = (2 * (x % 4));
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

static void PaintPixel16Color(BYTE x, BYTE y)
{    
    const long byteAddr = (SPRITE_BUFFER + (y * 8)) + (x / 2);
    const BYTE bitsel = (((x + 1) % 2) * 4);
    lpoke(byteAddr, lpeek(byteAddr) & (0xF0 >> bitsel) | (g_state.color[g_state.currentColorIdx] << bitsel));
}

static void ClearSprite()
{
    SetRedrawFullCanvas();
    lfill(SPRITE_BUFFER, 0, g_state.spriteSizeBytes);
}

static void FetchSpriteDataFromSlot()
{
    register BYTE i;
    const unsigned long spriteSourceAddr = SPRITE_DATA_ADDR(g_state.spriteNumber);
    for (i = 0; i < g_state.spriteSizeBytes; ++i) 
    {
        lpoke(SPRITE_BUFFER + i, freeze_peek(spriteSourceAddr + i));
    }
}

static void PutSpriteDataToSlot()
{
    register BYTE i;
    const unsigned long spriteSourceAddr = SPRITE_DATA_ADDR(g_state.spriteNumber);
    for (i = 0; i < g_state.spriteSizeBytes; ++i) 
    {
        freeze_poke(spriteSourceAddr + i, lpeek(SPRITE_BUFFER + i));
    }
}

void UpdateSpriteParameters(void)
{
    const BYTE isXWidth = IS_SPR_XWIDTH(g_state.spriteNumber);
    g_state.spriteHeight = 21;
    g_state.spriteSizeBytes = SPRITE_SIZE_BYTES(g_state.spriteNumber);
    g_state.bytesPerRow = g_state.spriteSizeBytes / g_state.spriteHeight;

    FetchSpriteDataFromSlot();
    g_state.spriteDataAddr = SPRITE_DATA_ADDR(g_state.spriteNumber);

    if (IS_SPR_16COL(g_state.spriteNumber))
    {
        g_state.drawCellFn = Draw16ColorCell;
        g_state.paintCellFn = PaintPixel16Color;
        g_state.spriteColorMode = SPR_COLOR_MODE_16COLOR;
        g_state.spriteWidth = 16; // Extended width is implied for 16-color sprites.
        g_state.cellsPerPixel = 3;
        g_state.pixelsPerByte = 2;
        g_state.currentColorIdx = COLOR_FORE;
        POKE(LOCAL_REG_SPR_16COL, PEEK(LOCAL_REG_SPR_16COL) | (1 << PREVIEW_SPRITE_NUM));
        POKE(LOCAL_REG_SPR_MULTICOLOR, PEEK(LOCAL_REG_SPR_MULTICOLOR) & ~(1 << PREVIEW_SPRITE_NUM));
    }
    else if (IS_SPR_MULTICOLOR(g_state.spriteNumber))
    {
        g_state.drawCellFn = DrawMulticolorCell;
        g_state.paintCellFn = PaintPixelMulti;
        g_state.spriteColorMode = SPR_COLOR_MODE_MULTICOLOR;
        g_state.spriteWidth =  isXWidth ? 32 : 12;
        g_state.cellsPerPixel = isXWidth ? 2 : 4;
        g_state.pixelsPerByte = 4;
        g_state.color[COLOR_FORE] = freeze_peek(REG_SPRITE_COLOR(g_state.spriteNumber));
        g_state.color[COLOR_MC1] = freeze_peek(REG_SPRITE_MULTICOL1);
        g_state.color[COLOR_MC2] = freeze_peek(REG_SPRITE_MULTICOL2);
        POKE(LOCAL_REG_SPR_16COL, PEEK(LOCAL_REG_SPR_16COL) & ~(1 << PREVIEW_SPRITE_NUM));
        POKE(LOCAL_REG_SPR_MULTICOLOR, PEEK(LOCAL_REG_SPR_MULTICOLOR) | (1 << PREVIEW_SPRITE_NUM));
    }
    else
    {
        g_state.drawCellFn = DrawMonoCell;
        g_state.paintCellFn = PaintPixelMono;
        g_state.spriteColorMode = SPR_COLOR_MODE_MONOCHROME;
        g_state.spriteWidth = isXWidth ? 64 : 24;
        g_state.cellsPerPixel = isXWidth  ? 1 : 2;
        g_state.pixelsPerByte = 8;
        g_state.color[COLOR_FORE] = freeze_peek(REG_SPRITE_COLOR(g_state.spriteNumber));
        POKE(LOCAL_REG_SPR_16COL, PEEK(LOCAL_REG_SPR_16COL) & ~(1 << PREVIEW_SPRITE_NUM));
        POKE(LOCAL_REG_SPR_MULTICOLOR, PEEK(LOCAL_REG_SPR_MULTICOLOR) & ~(1 << PREVIEW_SPRITE_NUM));
    }

    g_state.cellsPerPixel >>= g_state.wideScreenMode;
    g_state.canvasLeftX =  (SIDEBAR_COLUMN / 2) - (g_state.spriteWidth * g_state.cellsPerPixel / 2);
    
    // Restore border affected by previous SD Card I/O
    bordercolor(DEFAULT_BORDER_COLOR);

    // Setup Preview Area sprite. (we divide by 2 for H320 sprites, should divide by 1 if H640 mode)
    
    POKE(LOCAL_REG_SPRITE_COLOR(PREVIEW_SPRITE_NUM),  g_state.color[COLOR_FORE]);
    POKE(LOCAL_REG_SPRITE_MULTICOL1, g_state.color[COLOR_MC1]);
    POKE(LOCAL_REG_SPRITE_MULTICOL2, g_state.color[COLOR_MC2]);
    
    POKE(0xD004, (SPRITE_OFFSET_X + 
        ((SIDEBAR_COLUMN * 8 / 2) + 
        (((SIDEBAR_WIDTH * 8 / 2) / 2) - (g_state.spriteWidth / (IS_SPR_MULTICOLOR(g_state.spriteNumber) ? 1 : 2)))))  & 0xFF);

    POKE(0xD005, (SPRITE_OFFSET_Y + 
        (SIDEBAR_PREVIEW_AREA_TOP * 8 ) + 
        (((SIDEBAR_PREVIEW_AREA_HEIGHT * 8) / 2) - (g_state.spriteHeight / 2))));
}

static void UpdateColorRegs()
{
    freeze_poke(REG_SPRITE_COLOR(g_state.spriteNumber), g_state.color[COLOR_FORE]);
    freeze_poke(REG_SPRITE_MULTICOL1, g_state.color[COLOR_MC1]);
    freeze_poke(REG_SPRITE_MULTICOL2, g_state.color[COLOR_MC2]);
    bordercolor(COLOUR_BLUE);

    POKE(LOCAL_REG_SPRITE_COLOR(PREVIEW_SPRITE_NUM),  g_state.color[COLOR_FORE]);
    POKE(LOCAL_REG_SPRITE_MULTICOL1, g_state.color[COLOR_MC1]);
    POKE(LOCAL_REG_SPRITE_MULTICOL2, g_state.color[COLOR_MC2]);
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

    register BYTE row;
    register BYTE col;
    for (row = g_state.redrawRect.top; row < g_state.redrawRect.bottom; ++row)
        for (col = g_state.redrawRect.left; col < g_state.redrawRect.right; ++col)
            g_state.drawCellFn(col, row);
    if (g_state.toolActive)
    {
        g_state.drawShapeFn(TRUE);
    }

    DrawCursor(g_state.cursorX, g_state.cursorY);

    SetRect(&g_state.redrawRect, 0, 0, 0, 0);
}

void SetEffectiveToolRect(RECT *rc)
{
    SetRect(rc, MIN(g_state.toolOrgX, g_state.cursorX),
                MIN(g_state.toolOrgY, g_state.cursorY),
                MAX(g_state.toolOrgX, g_state.cursorX),
                MAX(g_state.toolOrgY, g_state.cursorY));
}

static void SetRedrawToolRect()
{
    if (g_state.toolActive)
        SetEffectiveToolRect(&g_state.redrawRect);
}

void SetRedrawFullCanvas(void)
{
    SetRect(&g_state.redrawRect, 0, 0, g_state.spriteWidth, g_state.spriteHeight);
}

static void DrawHeader()
{
    cprintf("{home}{rvson}{lgrn}mega65 sprite editor v0.9          (c)2021 hernan di pietro-paul gardner-stephen{rvsoff}");
}

static void DrawColorSelector()
{
    RECT rc;
    if (g_state.redrawSideBarFlags & REDRAW_SB_COLOR)
    {
        SetRect(&rc, SIDEBAR_COLUMN, 5, 80, 7);
        fillrect(&rc, ' ', DEFAULT_SCREEN_COLOR);

        switch (g_state.spriteColorMode)
        {
        case SPR_COLOR_MODE_MONOCHROME:

            textcolor(g_state.color[COLOR_BACK]);
            cputsxy(SIDEBAR_COLUMN, 5, "\xe0\xe0\xe0\xe0\xe0\xe0");
            textcolor(g_state.color[COLOR_FORE]);
            cputsxy(SIDEBAR_COLUMN + 8, 5, "\xe0\xe0\xe0\xe0\xe0\xe0");

            textcolor(g_state.currentColorIdx == COLOR_BACK ? 1 : COLOUR_DARKGREY);
            cputsxy(SIDEBAR_COLUMN + 2, 6, "bk");

            textcolor(g_state.currentColorIdx == COLOR_FORE ? 1 : COLOUR_DARKGREY);
            cputsxy(SIDEBAR_COLUMN + 8 + 2, 6, "fg");

            break;

        case SPR_COLOR_MODE_16COLOR:
            textcolor(g_state.color[COLOR_FORE]);
            cputsxy(SIDEBAR_COLUMN, 5, "\xe0\xe0\xe0\xe0\xe0\xe0\xe0\xe0\xe0\xe0\xe0\xe0\xe0\xe0\xe0");

            textcolor(1);
            cputsxy(SIDEBAR_COLUMN + 2, 6, g_state.color[COLOR_FORE] == 0 ? "background" : "foreground");
            break;

        case SPR_COLOR_MODE_MULTICOLOR:
            textcolor(g_state.color[COLOR_BACK]);
            cputsxy(SIDEBAR_COLUMN, 5, "\xe0\xe0\xe0");
            textcolor(g_state.color[COLOR_FORE]);
            cputsxy(SIDEBAR_COLUMN + 4, 5, "\xe0\xe0\xe0");
            textcolor(g_state.color[COLOR_MC1]);
            cputsxy(SIDEBAR_COLUMN + 4 * 2, 5, "\xe0\xe0\xe0");
            textcolor(g_state.color[COLOR_MC2]);
            cputsxy(SIDEBAR_COLUMN + 4 * 3, 5, "\xe0\xe0\xe0");

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
}

static void DrawToolbox()
{
    register BYTE i = 0;
    const BYTE numButtons = sizeof(chsetToolbox)/8/2/2;

    if (g_state.redrawSideBarFlags & REDRAW_SB_TOOLS)
    {
        for (i = 0; i < numButtons; ++i)
        {
            if (g_state.drawingTool == i)
            {
                textcolor(COLOUR_WHITE);
            }
            else
            {
                textcolor(COLOUR_DARKGREY);
            }
            
            cputcxy(SIDEBAR_COLUMN + i*2 , SCREEN_ROWS - 4,   TOOLBOX_CHARSET_BASE_IDX + i*2);
            cputcxy(SIDEBAR_COLUMN + i*2 + 1, SCREEN_ROWS - 4, TOOLBOX_CHARSET_BASE_IDX + i*2 + 1);

            cputcxy(SIDEBAR_COLUMN + i*2 , SCREEN_ROWS - 3,   TOOLBOX_CHARSET_BASE_IDX + i*2 + numButtons*2);
            cputcxy(SIDEBAR_COLUMN + i*2 + 1, SCREEN_ROWS - 3, TOOLBOX_CHARSET_BASE_IDX + i*2 + 1 + numButtons*2);
        }
    }
}

static void DrawSideBarSpriteInfo()
{
    if (g_state.redrawSideBarFlags & REDRAW_SB_INFO)
    {
        textcolor(1);
        gotoxy(SIDEBAR_COLUMN, 2);
        cputs("sprite ");
        cputdec(g_state.spriteNumber, 0, 0);
        cputs(g_state.spriteColorMode == SPR_COLOR_MODE_MONOCHROME ? " mono    " :
             (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR ? " multi   " : " 16-col"));
        gotoxy(SIDEBAR_COLUMN, 3);
        textcolor(3);
        cputhex(g_state.spriteDataAddr, 7);
        gotoxy(SIDEBAR_COLUMN, 8);
        textcolor(COLOUR_LIGHTBLUE);
        cputs(IS_SPR_XWIDTH(g_state.spriteNumber) | IS_SPR_16COL(g_state.spriteNumber) ? 
            "extended width" : "              ");
    }
}

static void DrawCoordinates()
{
    if (g_state.redrawSideBarFlags & REDRAW_SB_COORD)
    {
        cputncxy(SIDEBAR_COLUMN, SCREEN_ROWS - 1, SIDEBAR_WIDTH, ' ');
        gotox(SIDEBAR_COLUMN);
        textcolor(COLOUR_CYAN);
        cputc('(');
        cputdec(g_state.cursorX, 0, 0);
        cputc(',');
        cputdec(g_state.cursorY, 0, 0);  
        cputc(')');
    }
}

static void DrawSpritePreviewArea()
{
    register BYTE i = 0;
    textcolor(g_state.color[COLOR_BACK]);
    for (i = 0; i < SIDEBAR_PREVIEW_AREA_HEIGHT; ++i)
        cputncxy(SIDEBAR_COLUMN, SIDEBAR_PREVIEW_AREA_TOP + i, SIDEBAR_WIDTH, SOLID_BLOCK_CHARACTER);
}

static void DrawSidebar()
{
    DrawSideBarSpriteInfo();
    DrawCoordinates();
    DrawToolbox();
    DrawColorSelector();
    DrawSpritePreviewArea();
    g_state.redrawSideBarFlags = REDRAW_SB_NONE;
}

static void Ask(const char* question, char* outbuffer, unsigned char maxlen)
{
    gotoy(SCREEN_ROWS - 1);
    textcolor(COLOUR_PINK);
    cputncxy(0, SCREEN_ROWS - 1, SCREEN_COLS, ' ');
    cputsxy(0, SCREEN_ROWS - 1, question);
    cinput(outbuffer, maxlen + 1, CINPUT_ACCEPT_ALL);
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
    cputs(list[0]);
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
        "  file / txfer     ", 
        "new          ctrl+n",
        "load             f5",
        "save             f7",
        "fetch            f8",
        "store            f9"};

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
        "prev component    -",
        "next component    +",
        "select background k",
        "sel pal bank ctrl+p" };

    const char* editKeys[] = {
        "       edit        ",
        "prev sprite       <",
        "next sprite       >", 
        "change type       *",
        "toggle x-width    \x1e",
        "copy sprite  ctrl+c",
        "horz flip    ctrl+h",
        "vert flip    ctrl+v",
        "test              t" };

    const char* testKeys[] = {
        "     test mode     ",
        "play/pause    space",
        "h-expand          h",
        "v-expand          v",
        "set start frame   f",
        "set end frame     e",
        "set speed         s",
        "exit             f3" };

    const char* displayKeys[] = {
        "     display       ",
        "aspect ratio  alt+r",
        "25/50-line    alt+d",
        };
    
    flushkeybuf();
    clrscr();
    DrawHeader();
    PrintKeyGroup(fileKeys,     4, 0, 2);
    PrintKeyGroup(editKeys,     9, 21, 2);
    PrintKeyGroup(drawKeys,     7, 0, 2 +4+1 );
    PrintKeyGroup(colorKeys,    7, 0, 2 +4+1 +7+1);
    PrintKeyGroup(testKeys,     8, 21, 2 + 9 + 2);
    PrintKeyGroup(displayKeys,  3, 42, 2);

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
    clrscr();
    DrawHeader();
    DrawCanvas();
    DrawSidebar();
}

static void UpdateAndFullRedraw()
{
    UpdateSpriteParameters();
    EraseCanvasSpace();
    SetRedrawFullCanvas();
}

unsigned short joy_delay_countdown = 0;
unsigned char fire_lock = 0;

unsigned short mx,my;

static void MainLoop()
{
    FILEOPTIONS fileOpt;
    unsigned char buf[64];
    unsigned char key = 0, keymod = 0;
    BYTE redrawStatusBar = FALSE;

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
                g_state.cursorX = (mx - 55) / 8;
                g_state.cursorY = (my - 66) / 8;
                DrawCursor(g_state.cursorX, g_state.cursorY);
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

        /* ------------------------------------ HELP ----------------------------------- */

        case 0xF1: // F1
            ShowHelp();
            EraseCanvasSpace();
            SetRedrawFullCanvas();
            g_state.redrawSideBarFlags = REDRAW_SB_ALL;
            break;

        /* ------------------------- CURSOR MOVEMENT GROUP ----------------------------- */

        case CH_CURS_DOWN:
            SetRedrawFullCanvas();//SetRedrawToolRect();
            g_state.drawCellFn(g_state.cursorX, g_state.cursorY);
            g_state.redrawSideBarFlags = REDRAW_SB_COORD;
            g_state.cursorY = (g_state.cursorY == g_state.spriteHeight - 1) ? 0 : (g_state.cursorY + 1);
            break;

        case CH_CURS_UP:
            SetRedrawFullCanvas();//SetRedrawToolRect();
            g_state.drawCellFn(g_state.cursorX, g_state.cursorY);
            g_state.redrawSideBarFlags = REDRAW_SB_COORD;
            g_state.cursorY = (g_state.cursorY == 0) ? (g_state.spriteHeight - 1) : (g_state.cursorY - 1);
            break;

        case CH_CURS_LEFT:
            SetRedrawFullCanvas();//SetRedrawToolRect();
            g_state.drawCellFn(g_state.cursorX, g_state.cursorY);
            g_state.redrawSideBarFlags = REDRAW_SB_COORD;
            g_state.cursorX = (g_state.cursorX == 0) ? (g_state.spriteWidth - 1) : (g_state.cursorX - 1);
            break;

        case CH_CURS_RIGHT:
            SetRedrawFullCanvas();//SetRedrawToolRect();
            g_state.drawCellFn(g_state.cursorX, g_state.cursorY);
            g_state.redrawSideBarFlags = REDRAW_SB_COORD;
            g_state.cursorX = (g_state.cursorX == g_state.spriteWidth - 1) ? 0 : (g_state.cursorX + 1);
            break;

        /* ------------------------- EDIT GROUP ------------------------------------------ */

        case ' ':
            if (g_state.drawingTool == DRAWING_TOOL_PIXEL)
            {
                g_state.paintCellFn(g_state.cursorX, g_state.cursorY);
            }
            else
            {
                if (g_state.toolActive)
                {
                    g_state.toolActive = 0;
                    g_state.drawShapeFn(FALSE);
                    redrawStatusBar = TRUE;
                    SetRedrawFullCanvas();
                }
                else
                {
                    g_state.toolOrgX = g_state.cursorX;
                    g_state.toolOrgY = g_state.cursorY;
                    g_state.toolActive = 1;
                }
            }
            break;
        case ',':
        case '.':

            PutSpriteDataToSlot();
            g_state.redrawSideBarFlags = REDRAW_SB_ALL;

            if (key == '.' && g_state.spriteNumber++ == SPRITE_MAX_COUNT - 1)
                g_state.spriteNumber = 0;
            else if (key == ',' && g_state.spriteNumber-- == 0)
                g_state.spriteNumber = SPRITE_MAX_COUNT - 1;

            UpdateAndFullRedraw();
            g_state.cursorX = MIN(g_state.spriteWidth-1, g_state.cursorX);
            g_state.cursorY = MIN(g_state.spriteHeight-1, g_state.cursorY);

            break;

        case '*':
            g_state.redrawSideBarFlags = REDRAW_SB_ALL;
            switch(g_state.spriteColorMode) {
                case SPR_COLOR_MODE_16COLOR:
                    // Switch to Hi-Res
                    freeze_poke(REG_SPR_16COL, freeze_peek(REG_SPR_16COL) & ~(1 << g_state.spriteNumber));
                    freeze_poke(REG_SPR_MULTICOLOR, freeze_peek(REG_SPR_MULTICOLOR) & ~(1 << g_state.spriteNumber));
                    break;
                case SPR_COLOR_MODE_MULTICOLOR:
                    // Switch to 16-col
                    freeze_poke(REG_SPR_16COL, freeze_peek(REG_SPR_16COL) | (1 << g_state.spriteNumber));
                    freeze_poke(REG_SPR_MULTICOLOR, freeze_peek(REG_SPR_MULTICOLOR) & ~(1 << g_state.spriteNumber));
                    break;
                case SPR_COLOR_MODE_MONOCHROME:
                    // Switch to Multicol
                    freeze_poke(REG_SPR_16COL, freeze_peek(REG_SPR_16COL) & ~(1 << g_state.spriteNumber));
                    freeze_poke(REG_SPR_MULTICOLOR, freeze_peek(REG_SPR_MULTICOLOR) | (1 << g_state.spriteNumber));
                    break;
            }
            UpdateAndFullRedraw();
            break;

        case '@' : //94: // Up-arrow

            freeze_poke(REG_SPRX64EN, freeze_peek(REG_SPRX64EN) ^ (1 << g_state.spriteNumber));
            g_state.redrawSideBarFlags = REDRAW_SB_ALL;
            UpdateAndFullRedraw();
            break;

        case 3: // CTRL-C
            Ask("copy sprite to? ", buf, 1);
            if (buf[0] >= '0' && buf[0] <= '7')
            {
                BYTE toSprite = buf[0] - 48;
                if (SPRITE_SIZE_BYTES(toSprite) == g_state.spriteSizeBytes) 
                {
                    // Display error if bytes do not match
                    lcopy(g_state.spriteDataAddr, SPRITE_DATA_ADDR(toSprite), g_state.spriteSizeBytes);
                }
                else
                {
                    bordercolor(COLOUR_RED);
                    cgetc();
                    bordercolor(DEFAULT_BORDER_COLOR);
                }
            }
            break;

        /* ------------------------------- COLOR GROUP -------------------------- */

        case '+':
            g_state.redrawSideBarFlags = REDRAW_SB_COLOR;
            g_state.currentColorIdx = (g_state.currentColorIdx + 1) % (
                (g_state.spriteColorMode == SPR_COLOR_MODE_MONOCHROME | g_state.spriteColorMode == SPR_COLOR_MODE_16COLOR) ? 2 : 4);
        break;

        case '-':
            g_state.redrawSideBarFlags = REDRAW_SB_COLOR;
            g_state.currentColorIdx = (g_state.currentColorIdx - 1) % (
                (g_state.spriteColorMode == SPR_COLOR_MODE_MONOCHROME | g_state.spriteColorMode == SPR_COLOR_MODE_16COLOR) ? 2 : 4);
        break;
        
        case 107: // k
            g_state.redrawSideBarFlags = REDRAW_SB_COLOR;
            g_state.currentColorIdx = COLOR_BACK;
            break;

        case 16: // Ctrl+P
        {
            const unsigned char cBankReg = freeze_peek(REG_SPRPALSEL);
            const unsigned char cSprPalBank = (cBankReg & 0x30) >> 4;
            gotoxy(1,1);
            cputdec(cSprPalBank,0,4);
            freeze_poke(REG_SPRPALSEL, (cBankReg & ~0x30) | (((cSprPalBank + 1) % 4) << 4));
            SetupTextPalette();
        }
            break;

        /* ------------------------- DISPLAY GROUP --------------------------------------- */

        case 240: //ALT-D
            setscreensize(80,50);
            UpdateAndFullRedraw();
            break;

        case 174: //ALT-R
            g_state.wideScreenMode = ~g_state.wideScreenMode & 1;
            UpdateAndFullRedraw();
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
             ClearSprite();
            }
            break;
        
        // case ASC_HELP:
        //     break;

     
         
        
        /* --------------------------- DRAWING TOOLS GROUP ----------------------- */

        case 111: // o = circle  tool
            SetDrawTool(DRAWING_TOOL_CIRCLE);
            g_state.redrawSideBarFlags = REDRAW_SB_TOOLS;
            SetRedrawFullCanvas();
            break;

        case 79: // "O" = filled circle  tool
            SetDrawTool(DRAWING_TOOL_FILLED_CIRCLE);
            g_state.redrawSideBarFlags = REDRAW_SB_TOOLS;
            SetRedrawFullCanvas();
            break;

        case 112: // p=pixel tool
            SetDrawTool(DRAWING_TOOL_PIXEL);
            g_state.redrawSideBarFlags = REDRAW_SB_TOOLS;
            SetRedrawFullCanvas();
            g_state.toolActive = 0;
            break;

        case 120: // x = draw box
            SetDrawTool(DRAWING_TOOL_BOX);
            g_state.redrawSideBarFlags = REDRAW_SB_TOOLS;
            SetRedrawFullCanvas();
            break;

        case 88: // "X" 
            SetDrawTool(DRAWING_TOOL_FILLEDBOX);
            g_state.redrawSideBarFlags = REDRAW_SB_TOOLS;
            SetRedrawFullCanvas();
            break;

        case 108: // l = line
            SetDrawTool(DRAWING_TOOL_LINE);
            g_state.redrawSideBarFlags = REDRAW_SB_TOOLS;
            SetRedrawFullCanvas();
            break;

        default:
            if (key >= '0' && key <= '9')
            {
                g_state.color[g_state.currentColorIdx] = key - 48;
                g_state.redrawSideBarFlags = REDRAW_SB_COLOR;
                SetRedrawFullCanvas();
                UpdateColorRegs();
            }
            else if (key >= 97 && key <= 102) //a..f
            {
                g_state.color[g_state.currentColorIdx] = 10 + key - 'a';
                g_state.redrawSideBarFlags = REDRAW_SB_COLOR;
                SetRedrawFullCanvas();
                UpdateColorRegs();
            }
        }
        DrawCanvas();
        DrawSidebar();
    }
}

void do_sprite_editor()
{
    Initialize();
    DrawScreen();
    MainLoop();
    DoExit();
}
