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

    Version   0.10
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
                UI Enhancements and fixes.
                Fixes buffer overrun in Ask() function.
                Sprite preview at side.
                Fancy editing cursor sprite.
                Fancy pointer sprite
                H/V expand toggle
                Size optimizations
                Redrawing optimizations
                Fetch/Store slot shortcuts.

    v0.10       90-column display mode by default.
                Faster I/O to Freeze memory (fetch_sector_32 functions)
                Fixed Copy Sprite bug
                Fixed 64bit-width sprite preview.
                Changes at HELP screen
                Cursor updates optimization and fix w/SPRX64EN activated
                Fix "K" key for 16color



    TODO:
    * Consider SPRBPMEN for 16-color sprites
 */
#include <cc65.h>
#include "./mega65-libc/include/mega65/conio.h"
#include "./mega65-libc/include/mega65/mouse.h"
#include "./mega65-libc/include/mega65/hal.h"
#include <cbm.h>
#include <stdio.h>
#include <stdlib.h>
#include "./mega65-libc/include/mega65/memory.h"
#include "freezer.h"

extern int errno;
//#define SPRITED_STANDALONE
#define PAGE_SIZE 256
#define LOCAL_VIC_BASE 0xD000
#ifdef SPRITED_STANDALONE
#define VIC_BASE LOCAL_VIC_BASE
#define CIA2_PORT_A 0xDD00UL
#define FREEZE_PEEK(x) PEEK((x))
#define FREEZE_POKE(x, y) POKE((x), (y))
#else
#define VIC_BASE 0xFFD3000UL // This is where VIC-II is mapped in frozen memory
#define CIA2_PORT_A 0xFFD3D00UL
#define FREEZE_PEEK(x) freeze_peek((x))
#define FREEZE_POKE(x, y) freeze_poke((x), (y))
#endif
#define REG_SPRPTR_B0 (FREEZE_PEEK(VIC_BASE + 0x6CUL))
#define REG_SPRPTR_B1 (FREEZE_PEEK(VIC_BASE + 0x6DUL))
#define REG_SPRPTR_B2 (FREEZE_PEEK(VIC_BASE + 0x6EUL) & 0x7F)
#define REG_SPRPTR16 (FREEZE_PEEK(VIC_BASE + 0x6EUL) & 0x80)
#define REG_SPR_VEXPAND (VIC_BASE + 0x17UL)
#define REG_SPR_HEXPAND (VIC_BASE + 0x1DUL)
#define REG_SPR_16COL (VIC_BASE + 0x6BUL)
#define REG_SPR_MULTICOLOR (VIC_BASE + 0x1CUL)
#define REG_SPRX64EN (VIC_BASE + 0x57UL)
#define REG_SPRITE_MULTICOL1 (VIC_BASE + 0x25UL)
#define REG_SPRITE_MULTICOL2 (VIC_BASE + 0x26UL)
#define REG_SPRITE_COLOR(n) (VIC_BASE + 0x27UL + (n))
#define REG_SPRPALSEL (VIC_BASE + 0x70UL)

#define LOCAL_REG_SPR_16COL (LOCAL_VIC_BASE + 0x6BUL)
#define LOCAL_REG_SPR_MULTICOLOR (LOCAL_VIC_BASE + 0x1CUL)
#define LOCAL_REG_SPRX64EN (LOCAL_VIC_BASE + 0x57UL)
#define LOCAL_REG_SPRITE_MULTICOL1 (LOCAL_VIC_BASE + 0x25UL)
#define LOCAL_REG_SPRITE_MULTICOL2 (LOCAL_VIC_BASE + 0x26UL)
#define LOCAL_REG_SPRITE_COLOR(n) (LOCAL_VIC_BASE + 0x27UL + (n))
#define LOCAL_REG_SPRPALSEL (LOCAL_VIC_BASE + 0x70UL)

#define SPRITE_PALETTE ((FREEZE_PEEK(REG_SPRPALSEL) & 0x30) >> 4)
#define IS_SPR_MULTICOLOR(n) ((FREEZE_PEEK(REG_SPR_MULTICOLOR)) & (1 << (n)))
#define IS_SPR_16COL(n) ((FREEZE_PEEK(REG_SPR_16COL)) & (1 << (n)))
#define IS_SPR_XWIDTH(n) ((FREEZE_PEEK(REG_SPRX64EN)) & (1 << (n)))
#define IS_SPR_HEXPAND(n) ((FREEZE_PEEK(REG_SPR_HEXPAND)) & (1 << (n)))
#define IS_SPR_VEXPAND(n) ((FREEZE_PEEK(REG_SPR_VEXPAND)) & (1 << (n)))
#define SPRITE_POINTER_ADDR (((long)REG_SPRPTR_B0) | ((long)REG_SPRPTR_B1 << 8) | ((long)REG_SPRPTR_B2 << 16))
#define SPRITE_SIZE_BYTES(n) ((IS_SPR_XWIDTH((n)) | IS_SPR_16COL((n))) ? 168 : 64)
#define SPRITE_DATA_ADDR(n)                                                                                                 \
  (REG_SPRPTR16 ? 64                                                                                                        \
                      * (((long)FREEZE_PEEK(SPRITE_POINTER_ADDR + 1 + n * 2) << 8)                                          \
                          + ((long)FREEZE_PEEK(SPRITE_POINTER_ADDR + n * 2)))                                               \
                : (long)(64 * FREEZE_PEEK(SPRITE_POINTER_ADDR + n)) | (((long)(~FREEZE_PEEK(CIA2_PORT_A) & 0x3)) << 14))
// #define REG_SPRBPMEN_0_3            (vic_registers[0x49] >> 4)
// #define REG_SPRBPMEN_4_7            (vic_registers[0x4B] >> 4)
// #define SPRITE_BITPLANE_ENABLE(n)	(((REG_SPRBPMEN_4_7) << 4 | REG_SPRBPMEN_0_3) & (1 << (n)))
#define SCREEN_ROWS 25
#define SCREEN_COLS 80
#define CANVAS_HEIGHT (SCREEN_ROWS - 2)
#define CANVAS_TOP_MARGIN 2

#define TRUE 1
#define FALSE 0
#define SPRITE_MAX_COUNT 8
#define DEFAULT_BORDER_COLOR 6
#define DEFAULT_SCREEN_COLOR 6
#define DEFAULT_BACK_COLOR 11

#define TRANS_CHARACTER 230
#define SOLID_BLOCK_CHARACTER 224
#define SHAPE_PREVIEW_CHARACTER 32
#define SIDEBAR_COLUMN 65
#define SIDEBAR_WIDTH (SCREEN_COLS - SIDEBAR_COLUMN)
#define SIDEBAR_PREVIEW_AREA_TOP 10
#define SIDEBAR_PREVIEW_AREA_BOTTOM 20
#define SIDEBAR_PREVIEW_AREA_HEIGHT (SIDEBAR_PREVIEW_AREA_BOTTOM - SIDEBAR_PREVIEW_AREA_TOP)
#define SPRITE_OFFSET_X 24
#define SPRITE_OFFSET_Y 50

#define JOY_DELAY 10000U

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS8(x) (((x) ^ ((x) >> 7)) - ((x) >> 7))
#define ABS16(x) (((x) ^ ((x) >> 15)) - ((x) >> 15))
#define ARRAY_SIZE(x) (sizeof(##x) / sizeof(##x[0]))

// Screen RAM for our area. We do not use 16-bit character mode
// so we need 80x25 = 2K area.
#define SCREEN_RAM_ADDRESS 0x12000UL
#define CHARSET_ADDRESS 0x15000UL
#define SPRITE_POINTER_TABLE 0x16000UL
#define SPRITE_BUFFER 0x40000UL
#define PREVIEW_SPRITE_NUM 2
#define EDIT_CURSOR_NUM 1

// Redraw flags
#define REDRAW_SB_NONE 0
#define REDRAW_SB_INFO 1
#define REDRAW_SB_COORD 2
#define REDRAW_SB_COLOR 4
#define REDRAW_SB_TOOLS 8
#define REDRAW_TOOL_PREVIEW 16
#define REDRAW_TOOL_HEADER 32
#define REDRAW_SB_ALL 1 + 2 + 4 + 8 + 16 + 32

typedef unsigned char BYTE;
typedef unsigned char BOOL;

#define SPR_COLOR_MODE_MONOCHROME 0
#define SPR_COLOR_MODE_MULTICOLOR 1
#define SPR_COLOR_MODE_16COLOR 2

// Drawing tools
#define DRAWING_TOOL_PIXEL 0
#define DRAWING_TOOL_LINE 1
#define DRAWING_TOOL_BOX 2
#define DRAWING_TOOL_FILLEDBOX 3
#define DRAWING_TOOL_CIRCLE 4
#define DRAWING_TOOL_FILLED_CIRCLE 5

// Index into color array
#define COLOR_BACK 0
#define COLOR_FORE 1
#define COLOR_MC1 2
#define COLOR_MC2 3

typedef void (*PAINTFUNC)(BYTE, BYTE);

typedef struct tagAPPSTATE {
  BYTE wideScreenMode;
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
  BYTE redrawFlags; // See REDRAW_SB_ constants for flags
  RECT redrawRect;
  void (*drawCellFn)(BYTE, BYTE);
  void (*paintCellFn)(BYTE, BYTE);
  void (*drawShapeFn)(PAINTFUNC);
  void (*updateCursorXFn)(void);
  void (*updateCursorYFn)(void);
  unsigned int spriteSizeBytes;
  long spriteDataAddr;
} APPSTATE;

typedef struct tagFILEOPTIONS {
  BYTE mode;
  BYTE name[16];
} FILEOPTIONS;

static APPSTATE g_state;

/* Nonstatic Function prototypes */
void UpdateSpriteParameters(BOOL);
void SetDrawTool(BYTE);
void SetRedrawFullCanvas(void);
void SetEffectiveToolRect(RECT*);
void SetupTextPalette(void);
void UpdateCursorX(void);
void UpdateCursorY(void);
void UpdateCursorXMSB(void);

/* Toolbox Character set, in order of DRAWING_TOOL... enumeration */

static const BYTE chsetToolbox[] = {

  // ------ UPPER ROW -----

  // Pixel tool
  0, 0, 255, 128, 128, 128, 128, 129, 0, 0, 255, 1, 1, 1, 1, 129,

  // Line tool
  0, 0, 255, 128, 128, 176, 140, 131, 0, 0, 255, 1, 1, 1, 1, 1,

  // Box tool
  0, 0, 255, 128, 128, 159, 144, 144, 0, 0, 255, 1, 1, 249, 9, 9,

  // Filled-box tool
  0, 0, 255, 128, 128, 143, 143, 143, 0, 0, 255, 1, 1, 241, 241, 241,

  // Circle tool
  0, 0, 255, 128, 128, 131, 132, 136, 0, 0, 255, 1, 1, 193, 33, 17,

  // Filled-circle tool
  0, 0, 255, 128, 128, 129, 135, 143, 0, 0, 255, 1, 1, 129, 225, 241,

  // ------ LOWER ROW -------

  // Pixel tool
  129, 128, 128, 128, 128, 255, 63, 0, 129, 1, 1, 1, 1, 255, 255, 0,

  // Line tool
  128, 128, 128, 128, 128, 255, 255, 0, 193, 49, 13, 1, 1, 255, 255, 0,

  // Box tool
  144, 144, 159, 128, 128, 255, 255, 0, 9, 9, 249, 1, 1, 255, 255, 0,

  // Filled-box tool
  143, 143, 143, 128, 128, 255, 255, 0, 241, 241, 241, 1, 1, 255, 255, 0,

  // Circle tool
  136, 132, 131, 128, 128, 255, 255, 0, 17, 33, 193, 1, 1, 255, 255, 0,

  // Filled-circle tool
  143, 135, 129, 128, 128, 255, 255, 0, 241, 225, 129, 1, 1, 255, 255, 0
};

#define TOOLBOX_CHARSET_BASE_IDX 232

static const BYTE spritePointer[] = { 128, 0, 0, 192, 0, 0, 224, 0, 0, 240, 0, 0, 248, 0, 0, 252, 0, 0, 254, 0, 0, 255, 0, 0,
  248, 0, 0, 216, 0, 0, 140, 0, 0, 12, 0, 0, 6, 0, 0, 6, 0, 0, 3, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0 };

static const BYTE editCursors[] = {
  // Cursor for single cell

  240, 0, 0, 144, 0, 0, 144, 0, 0, 144, 0, 0, 144, 0, 0, 144, 0, 0, 144, 0, 0, 240, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

  // double cell

  255, 0, 0, 129, 0, 0, 129, 0, 0, 129, 0, 0, 129, 0, 0, 129, 0, 0, 129, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

  // triple cell

  255, 240, 0, 128, 16, 0, 128, 16, 0, 128, 16, 0, 128, 16, 0, 128, 16, 0, 128, 16, 0, 255, 240, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

  // four cell
  255, 255, 0, 128, 1, 0, 128, 1, 0, 128, 1, 0, 128, 1, 0, 128, 1, 0, 128, 1, 0, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const BYTE editCursorColorMap[16] = { COLOUR_BLACK, COLOUR_BLUE, COLOUR_BROWN, COLOUR_GREY1, COLOUR_GREY2,
  COLOUR_GREY3, COLOUR_LIGHTGREEN, COLOUR_WHITE, COLOUR_WHITE, COLOUR_LIGHTGREEN, COLOUR_GREY3, COLOUR_GREY2, COLOUR_GREY1,
  COLOUR_RED, COLOUR_BROWN, COLOUR_BLUE };

static void SetRect(RECT* rc, BYTE left, BYTE top, BYTE right, BYTE bottom)
{
  rc->left = left;
  rc->right = right;
  rc->top = top;
  rc->bottom = bottom;
}

static void Initialize()
{
  // Set 40MHz, VIC-IV I/O, 80 column, screen RAM @ $8000
  POKE(0, 65);

  // --- Freezer slot setup

  find_freeze_slot_start_sector(0);
  freeze_slot_start_sector = *(uint32_t*)0xD681U;

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
  lcopy((long)chsetToolbox, CHARSET_ADDRESS + TOOLBOX_CHARSET_BASE_IDX * 8, sizeof(chsetToolbox));
  setcharsetaddr(CHARSET_ADDRESS);

  // --- Sprite setup ----

  // Set pointer table to SPRITE_POINTER_TABLE
  // Set sprite 0 to our cursor. Address = $380.
  // Set sprite 1 to editing cursor #1   Address = $3C0
  // Set sprite 2 to current sprite placeholder.   Address = SPRITE_BUFFER

  // Set local sprite pointer table

  POKE(0xD06C, (BYTE)SPRITE_POINTER_TABLE);
  POKE(0xD06D, (BYTE)(SPRITE_POINTER_TABLE >> 8));
  POKE(0xD06E, (BYTE)(SPRITE_POINTER_TABLE >> 16) | 128); // Enable SPRPTR16

  // #0: Mouse Pointer sprite at 0x380

  lcopy((long)spritePointer, 0x380, 63);
  lpoke(SPRITE_POINTER_TABLE, 0x0E);
  lpoke(SPRITE_POINTER_TABLE + 1, 0x00);

  // Address of edit cursor
  lpoke(SPRITE_POINTER_TABLE + 2, 0x0F); // 64 * 0xF = 0x3C0
  lpoke(SPRITE_POINTER_TABLE + 3, 0x00);

  // Address of current sprite image preview
  lpoke(SPRITE_POINTER_TABLE + 4, SPRITE_BUFFER / 64 % 256);
  lpoke(SPRITE_POINTER_TABLE + 5, SPRITE_BUFFER / 64 / 256);

  // Sprite properties (color, initial pos, etc.)

  POKE(0xD074, 0); // Alpha OFF
  POKE(0xD076, 0); // V400 mode off for editor sprites.
  POKE(0xD077, 0); // Y-MSBs off
  POKE(0xD078, 0); // Y-MSBs off

  POKE(0xD015, 7); // Enable #0, #1, #2
  POKE(0xD01D, 0); // H-expand off for editor sprites.
  POKE(0xD017, 0); // V-expand off for editor sprites.
  POKE(0xD000, 100);
  POKE(0xD001, 100);

  POKE(0xD002, 0);
  POKE(0xD003, 0);

  POKE(0xD027, 7);
  POKE(0xD028, 1);
  POKE(0xD010, 1 << PREVIEW_SPRITE_NUM); // 8th bit for Sprite#2
  POKE(0xD01C, 0);                       // All mono/hires sprites
  POKE(0xD06B, 0);                       // 16-color mode OFF

  g_state.redrawFlags = REDRAW_SB_ALL;
  g_state.spriteNumber = 0;
  g_state.cursorX = g_state.cursorY = 0;
  g_state.currentColorIdx = COLOR_FORE;
  g_state.toolActive = 0;
  g_state.toolOrgX = g_state.toolOrgY = 0;
  g_state.color[COLOR_BACK] = DEFAULT_BACK_COLOR;
  g_state.wideScreenMode = 0;
  g_state.updateCursorXFn = UpdateCursorX;
  g_state.updateCursorYFn = UpdateCursorY;

  SetDrawTool(DRAWING_TOOL_PIXEL);
  UpdateSpriteParameters(TRUE);
  SetRedrawFullCanvas();

  g_state.updateCursorXFn();
  g_state.updateCursorYFn();
}

void LoadSlotSpritePalette()
{
}

void SetupTextPalette(void)
{
  // To properly display color in the editor, we set the main palette bank to 0,
  // the sprite palette bank, and use the alt palette to display text and UI.
  // We do this because ALTPAL is selected by VIC-III HIGHLIGHT+UNDERLINE
  // combination and  Foreground colors in alt-palette are used from the 16th index,
  // so we avoid fiddling with this.

  // POKE(0xD070UL, (PEEK(0xD070UL) & ~48) | ((FREEZE_PEEK(REG_SPRPALSEL) & 0xC) << 2));
  // setmapedpal(SPRITE_PALETTE);
}

void UpdateCursorX()
{
  BYTE cvw = g_state.canvasLeftX * 4;
  BYTE xc = g_state.cursorX * g_state.cellsPerPixel * 4;
  POKE(0xD002, SPRITE_OFFSET_X + cvw + xc);
}

void UpdateCursorY()
{
  BYTE yc = g_state.cursorY * 8;
  POKE(0xD003, SPRITE_OFFSET_Y + (2 * 8) + yc);
}

void UpdateCursorXMSB()
{
  BYTE cvw = g_state.canvasLeftX * 4;
  BYTE xc = g_state.cursorX * g_state.cellsPerPixel * 4;
  const unsigned short sx = SPRITE_OFFSET_X + cvw + xc;
  if (sx < 256) {
    POKE(0xD010, PEEK(0xD010) & ~(1 << EDIT_CURSOR_NUM));
  }
  else {
    POKE(0xD010, PEEK(0xD010) | (1 << EDIT_CURSOR_NUM));
  }
  POKE(0xD002, sx);
}

static void DrawShapeChar(BYTE x, BYTE y)
{
  cputncxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), 2 + y, g_state.cellsPerPixel, SHAPE_PREVIEW_CHARACTER);
}

static void DrawLine(PAINTFUNC pfun)
{
  RECT rc;
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

    for (;;) {
      pfun(x, y);
      if (x == g_state.cursorX && y == g_state.cursorY)
        break;
      e2 = e * 2;
      if (e2 >= dy) {
        e += dy;
        x += sx;
      }
      if (e2 <= dx) {
        e += dx;
        y += sy;
      }
    }
  }
  clearattr();
}

/*
static void DrawCircle(PAINTFUNC pfun)
{
  RECT rc;
  SetEffectiveToolRect(&rc);

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
*/

static void DrawBox(PAINTFUNC pfun)
{
  RECT rc;
  register BYTE x, y, i;
  SetEffectiveToolRect(&rc);
  x = rc.left;
  while (x <= rc.right) {
    pfun(x, g_state.cursorY);
    if (g_state.fillShape) {
      for (i = rc.top + 1; i < rc.bottom; ++i) {
        pfun(x, i);
      }
    }
    pfun(x++, g_state.toolOrgY);
  }

  y = rc.top;
  while (y <= rc.bottom) {
    pfun(g_state.cursorX, y);
    pfun(g_state.toolOrgX, y++);
  }
  clearattr();
}

// clang-format off
#pragma warn(unused-param, push, off)
static void DrawNothing(PAINTFUNC pfun)
{
  return;
}
#pragma warn(unused-param, pop)
// clang-format on

void SetDrawTool(BYTE dt)
{
  g_state.drawingTool = dt;
  switch (dt) {
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
  case DRAWING_TOOL_PIXEL:
  default:
    g_state.drawShapeFn = DrawNothing;
  }
}

static void DrawMonoCell(BYTE x, BYTE y)
{
  register BYTE cell = 0;
  const BYTE bufoff = y * g_state.spriteWidth / 8;
  const long byteAddr = (SPRITE_BUFFER + bufoff) + (x / 8);
  const BYTE p = lpeek(byteAddr) & (0x80 >> (x % 8));

  gotoxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), y + 2);
  for (cell = 0; cell < g_state.cellsPerPixel; ++cell) {
    textcolor(p ? g_state.color[COLOR_FORE] : g_state.color[COLOR_BACK]);
    cputc(p ? SOLID_BLOCK_CHARACTER : TRANS_CHARACTER);
  }
}

static void Draw16ColorCell(BYTE x, BYTE y)
{
  register BYTE cell = 0;
  const long byteAddr = (SPRITE_BUFFER + (y * 8)) + (x / 2);
  const BYTE p = 0xF & (lpeek(byteAddr) >> (((x + 1) % 2) * 4));
  // const BYTE col = (g_state.spriteNumber * 16) + p;

  gotoxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), y + 2);
  for (cell = 0; cell < g_state.cellsPerPixel; ++cell) {
    textcolor(p);
    cputc(p ? SOLID_BLOCK_CHARACTER : TRANS_CHARACTER);
  }
}

static void DrawMulticolorCell(BYTE x, BYTE y)
{
  register BYTE cell = 0;
  const long byteAddr = (SPRITE_BUFFER + (y * g_state.spriteWidth / 4)) + (x / 4);
  const BYTE b = lpeek(byteAddr);
  const BYTE p0 = b & (0x80 >> (2 * (x % 4)));
  const BYTE p1 = b & (0x40 >> (2 * (x % 4)));
  BYTE color = g_state.color[COLOR_BACK];
  if (!p0 && p1)
    color = g_state.color[COLOR_MC1];
  else if (p0 && !p1)
    color = g_state.color[COLOR_FORE];
  else if (p0 && p1)
    color = g_state.color[COLOR_MC2];

  gotoxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), y + 2);
  for (cell = 0; cell < g_state.cellsPerPixel; ++cell) {
    textcolor(color);
    cputc(p0 | p1 ? SOLID_BLOCK_CHARACTER : TRANS_CHARACTER);
  }
}

static void PaintPixelMono(BYTE x, BYTE y)
{
  const long byteAddr = (SPRITE_BUFFER + (y * g_state.spriteWidth / 8)) + (x / 8);
  const BYTE bitsel = 0x80 >> (x % 8);
  const BYTE b = lpeek(byteAddr);
  lpoke(byteAddr, g_state.currentColorIdx == COLOR_BACK ? (b & ~bitsel) : (b | bitsel));
}

static void PaintPixelMulti(BYTE x, BYTE y)
{
  const long byteAddr = (SPRITE_BUFFER + (y * g_state.spriteWidth / 4)) + (x / 4);
  const BYTE b = lpeek(byteAddr);
  const BYTE bitsel = (2 * (x % 4));
  const BYTE p0 = b & (0x80 >> bitsel);
  const BYTE p1 = b & (0x40 >> bitsel);
  const BYTE mask = ((0x80 >> bitsel) | (0x40 >> bitsel));
  if ((g_state.currentColorIdx == COLOR_BACK)) {
    lpoke(byteAddr, lpeek(byteAddr) & ~mask);
  }
  else {
    if (g_state.currentColorIdx == COLOR_FORE) {
      lpoke(byteAddr, lpeek(byteAddr) & ~mask | (0x80 >> bitsel));
    }
    else if (g_state.currentColorIdx == COLOR_MC1) {
      lpoke(byteAddr, lpeek(byteAddr) & ~mask | (0x40 >> bitsel));
    }
    else if (g_state.currentColorIdx == COLOR_MC2) {
      lpoke(byteAddr, lpeek(byteAddr) & ~mask | ((0x80 >> bitsel) | (0x40 >> bitsel)));
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

static void FetchVic2RegsFromSlot()
{
  // H/Y expand

  const BYTE sprBit = 1 << PREVIEW_SPRITE_NUM;
  if (IS_SPR_HEXPAND(g_state.spriteNumber)) {
    POKE(0xD01D, PEEK(0xD01D) | sprBit);
  }
  else {
    POKE(0xD01D, PEEK(0xD01D) & ~sprBit);
  }
  if (IS_SPR_VEXPAND(g_state.spriteNumber)) {
    POKE(0xD017, PEEK(0xD017) | sprBit);
  }
  else {
    POKE(0xD017, PEEK(0xD017) & ~sprBit);
  }
  if (IS_SPR_XWIDTH(g_state.spriteNumber)) {
    POKE(0xD057, PEEK(0xD057) | sprBit);
  }
  else {
    POKE(0xD057, PEEK(0xD057) & ~sprBit);
  }
}

static void FetchSpriteDataFromSlot()
{
  // TODO: Sprites may exceed 512 bytes
  freeze_fetch_sector_partial(g_state.spriteDataAddr, SPRITE_BUFFER, g_state.spriteSizeBytes);
}

static void PutSpriteDataToSlot()
{
  // TODO: Sprites may exceed 512 bytes
  freeze_store_sector_partial(g_state.spriteDataAddr, SPRITE_BUFFER, g_state.spriteSizeBytes);
}

static void CopySpriteData(const uint32_t to_addr)
{
  // TODO: Sprites may exceed 512 bytes
  freeze_store_sector_partial(to_addr, SPRITE_BUFFER, g_state.spriteSizeBytes);
}

static void UpdatePalette(void)
{
  // register BYTE i = 0;
  // if (IS_SPR_16COL(g_state.spriteNumber)) {

  //     setmapedpal( (FREEZE_PEEK(REG_SPRPALSEL) >> 2) & 0x3);
  //     for (i = 0; i < 16; ++i) {
  //         POKE(0xD100 + i,
  //     }
  // }
}

#define HFACTOR (IS_SPR_HEXPAND(g_state.spriteNumber) ? 2 : 1)
#define VFACTOR (IS_SPR_VEXPAND(g_state.spriteNumber) ? 2 : 1)

static void UpdateSpritePreview(void)
{
  // Setup Preview Area sprite. (we divide by 2 for H320 sprites, should divide by 1 if H640 mode)

  POKE(LOCAL_REG_SPRITE_COLOR(PREVIEW_SPRITE_NUM), g_state.color[COLOR_FORE]);
  POKE(LOCAL_REG_SPRITE_MULTICOL1, g_state.color[COLOR_MC1]);
  POKE(LOCAL_REG_SPRITE_MULTICOL2, g_state.color[COLOR_MC2]);

  POKE(0xD004, (SPRITE_OFFSET_X
                   + ((SIDEBAR_COLUMN * 8 / 2)
                       + (((SIDEBAR_WIDTH * 8 / 2) / 2)
                           - (g_state.spriteWidth * HFACTOR / (IS_SPR_MULTICOLOR(g_state.spriteNumber) ? 1 : 2)))))
                   & 0xFF);

  POKE(0xD005, (SPRITE_OFFSET_Y + (SIDEBAR_PREVIEW_AREA_TOP * 8)
                   + (((SIDEBAR_PREVIEW_AREA_HEIGHT * 8) / 2) - (g_state.spriteHeight * VFACTOR / 2))));
}

void UpdateSpriteParameters(BOOL fFetchSlot)
{
  const BYTE isXWidth = IS_SPR_XWIDTH(g_state.spriteNumber);

  g_state.spriteHeight = 21;
  g_state.spriteSizeBytes = SPRITE_SIZE_BYTES(g_state.spriteNumber);
  g_state.bytesPerRow = g_state.spriteSizeBytes / g_state.spriteHeight;
  g_state.spriteDataAddr = SPRITE_DATA_ADDR(g_state.spriteNumber);

  if (fFetchSlot) {
    FetchSpriteDataFromSlot();
    FetchVic2RegsFromSlot();
  }

  if (IS_SPR_16COL(g_state.spriteNumber)) {
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
  else if (IS_SPR_MULTICOLOR(g_state.spriteNumber)) {
    g_state.drawCellFn = DrawMulticolorCell;
    g_state.paintCellFn = PaintPixelMulti;
    g_state.spriteColorMode = SPR_COLOR_MODE_MULTICOLOR;
    g_state.spriteWidth = isXWidth ? 32 : 12;
    g_state.cellsPerPixel = isXWidth ? 2 : 4;
    g_state.pixelsPerByte = 4;
    g_state.color[COLOR_FORE] = FREEZE_PEEK(REG_SPRITE_COLOR(g_state.spriteNumber));
    g_state.color[COLOR_MC1] = FREEZE_PEEK(REG_SPRITE_MULTICOL1);
    g_state.color[COLOR_MC2] = FREEZE_PEEK(REG_SPRITE_MULTICOL2);
    POKE(LOCAL_REG_SPR_16COL, PEEK(LOCAL_REG_SPR_16COL) & ~(1 << PREVIEW_SPRITE_NUM));
    POKE(LOCAL_REG_SPR_MULTICOLOR, PEEK(LOCAL_REG_SPR_MULTICOLOR) | (1 << PREVIEW_SPRITE_NUM));
  }
  else {
    g_state.drawCellFn = DrawMonoCell;
    g_state.paintCellFn = PaintPixelMono;
    g_state.spriteColorMode = SPR_COLOR_MODE_MONOCHROME;
    g_state.spriteWidth = isXWidth ? 64 : 24;
    g_state.cellsPerPixel = isXWidth ? 1 : 2;
    g_state.pixelsPerByte = 8;
    g_state.color[COLOR_FORE] = FREEZE_PEEK(REG_SPRITE_COLOR(g_state.spriteNumber));
    POKE(LOCAL_REG_SPR_16COL, PEEK(LOCAL_REG_SPR_16COL) & ~(1 << PREVIEW_SPRITE_NUM));
    POKE(LOCAL_REG_SPR_MULTICOLOR, PEEK(LOCAL_REG_SPR_MULTICOLOR) & ~(1 << PREVIEW_SPRITE_NUM));
  }

  g_state.cellsPerPixel >>= g_state.wideScreenMode;
  g_state.canvasLeftX = (SIDEBAR_COLUMN / 2) - (g_state.spriteWidth * g_state.cellsPerPixel / 2);

  // Restore border affected by previous SD Card I/O
  bordercolor(DEFAULT_BORDER_COLOR);

  UpdateSpritePreview();

  // Setup Edit cursor

  lcopy((long)editCursors + 63 * (g_state.cellsPerPixel - 1), 0x3C0, 63);

  // The edit cursor maybe off-bounds if a different sprite type was switched,
  // so force to recalculate
  g_state.cursorX = MIN(g_state.cursorX, g_state.spriteWidth - 1);
  g_state.cursorY = MIN(g_state.cursorY, g_state.spriteHeight - 1);
  g_state.updateCursorXFn();
  g_state.updateCursorYFn();
}

static void UpdateColorRegs()
{
  FREEZE_POKE(REG_SPRITE_COLOR(g_state.spriteNumber), g_state.color[COLOR_FORE]);
  FREEZE_POKE(REG_SPRITE_MULTICOL1, g_state.color[COLOR_MC1]);
  FREEZE_POKE(REG_SPRITE_MULTICOL2, g_state.color[COLOR_MC2]);
  bordercolor(COLOUR_BLUE);

  POKE(LOCAL_REG_SPRITE_COLOR(PREVIEW_SPRITE_NUM), g_state.color[COLOR_FORE]);
  POKE(LOCAL_REG_SPRITE_MULTICOL1, g_state.color[COLOR_MC1]);
  POKE(LOCAL_REG_SPRITE_MULTICOL2, g_state.color[COLOR_MC2]);
}

static void EraseCanvasSpace()
{
  RECT rc;
  rc.top = 2;
  rc.left = 0;
  rc.right = SIDEBAR_COLUMN - 1;
  rc.bottom = 23;
  fillrect(&rc, ' ', 1);
}

static void DrawCanvas()
{
  register BYTE row;
  register BYTE col;
  for (row = g_state.redrawRect.top; row < g_state.redrawRect.bottom; ++row)
    for (col = g_state.redrawRect.left; col < g_state.redrawRect.right; ++col)
      g_state.drawCellFn(col, row);

  if (g_state.toolActive && (g_state.redrawFlags & REDRAW_TOOL_PREVIEW)) {
    blink(1);
    revers(1);
    textcolor(g_state.color[g_state.currentColorIdx]);
    g_state.drawShapeFn(DrawShapeChar);
  }

  SetRect(&g_state.redrawRect, 0, 0, 0, 0);
  g_state.redrawFlags &= ~REDRAW_TOOL_PREVIEW;
}

void SetEffectiveToolRect(RECT* rc)
{
  SetRect(rc, MIN(g_state.toolOrgX, g_state.cursorX), MIN(g_state.toolOrgY, g_state.cursorY),
      MAX(g_state.toolOrgX, g_state.cursorX), MAX(g_state.toolOrgY, g_state.cursorY));
}

void SetRedrawFullCanvas(void)
{
  SetRect(&g_state.redrawRect, 0, 0, g_state.spriteWidth, g_state.spriteHeight);
}

static void DrawHeader()
{
  if (g_state.redrawFlags & REDRAW_TOOL_HEADER)
    cprintf("{home}{rvson}{lgrn}                            the mega65 sprite editor                            {rvsoff}");
}

static void DrawColorSelector()
{
  RECT rc;
  if (g_state.redrawFlags & REDRAW_SB_COLOR) {
    SetRect(&rc, SIDEBAR_COLUMN, 5, 80, 7);
    fillrect(&rc, ' ', DEFAULT_SCREEN_COLOR);

    switch (g_state.spriteColorMode) {
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
      switch (g_state.currentColorIdx) {
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
  const BYTE numButtons = sizeof(chsetToolbox) / 8 / 2 / 2;

  if (g_state.redrawFlags & REDRAW_SB_TOOLS) {
    for (i = 0; i < numButtons; ++i) {
      if (g_state.drawingTool == i) {
        textcolor(COLOUR_WHITE);
      }
      else {
        textcolor(COLOUR_DARKGREY);
      }

      cputcxy(SIDEBAR_COLUMN + i * 2, SCREEN_ROWS - 4, TOOLBOX_CHARSET_BASE_IDX + i * 2);
      cputcxy(SIDEBAR_COLUMN + i * 2 + 1, SCREEN_ROWS - 4, TOOLBOX_CHARSET_BASE_IDX + i * 2 + 1);

      cputcxy(SIDEBAR_COLUMN + i * 2, SCREEN_ROWS - 3, TOOLBOX_CHARSET_BASE_IDX + i * 2 + numButtons * 2);
      cputcxy(SIDEBAR_COLUMN + i * 2 + 1, SCREEN_ROWS - 3, TOOLBOX_CHARSET_BASE_IDX + i * 2 + 1 + numButtons * 2);
    }
  }
}

static void DrawSideBarSpriteInfo()
{
  if (g_state.redrawFlags & REDRAW_SB_INFO) {
    textcolor(1);
    gotoxy(SIDEBAR_COLUMN, 2);
    cputs("sprite ");
    cputdec(g_state.spriteNumber, 0, 0);
    cputs(g_state.spriteColorMode == SPR_COLOR_MODE_MONOCHROME
              ? " mono    "
              : (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR ? " multi   " : " 16-col"));
    gotoxy(SIDEBAR_COLUMN, 3);
    textcolor(3);
    cputhex(g_state.spriteDataAddr, 7);
    gotoxy(SIDEBAR_COLUMN, 8);
    textcolor(IS_SPR_XWIDTH(g_state.spriteNumber) | IS_SPR_16COL(g_state.spriteNumber) ? COLOUR_LIGHTBLUE : COLOUR_DARKGREY);
    cputs("xwide");
    textcolor(IS_SPR_HEXPAND(g_state.spriteNumber) ? COLOUR_LIGHTBLUE : COLOUR_DARKGREY);
    cputs(" hexp");
    textcolor(IS_SPR_VEXPAND(g_state.spriteNumber) ? COLOUR_LIGHTBLUE : COLOUR_DARKGREY);
    cputs(" vexp");
  }
}

static void DrawCoordinates()
{
  if (g_state.redrawFlags & REDRAW_SB_COORD) {
    cputncxy(SIDEBAR_COLUMN, SCREEN_ROWS - 1, SIDEBAR_WIDTH, ' ');
    gotoxy(SIDEBAR_COLUMN, SCREEN_ROWS - 1);
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
  DrawHeader();
  DrawSideBarSpriteInfo();
  DrawCoordinates();
  DrawToolbox();
  DrawColorSelector();
  DrawSpritePreviewArea();
  g_state.redrawFlags = REDRAW_SB_NONE;
}

static void Ask(const char* question, char* outbuffer, unsigned char maxlen)
{
  gotoy(SCREEN_ROWS - 1);
  revers(1);
  textcolor(COLOUR_PINK);
  cputncxy(0, SCREEN_ROWS - 1, SCREEN_COLS, ' ');
  cputsxy(0, SCREEN_ROWS - 1, question);
  cinput(outbuffer, maxlen + 1, CINPUT_ACCEPT_ALL);
  revers(0);
  textcolor(COLOUR_BLUE);
  cputncxy(0, SCREEN_ROWS - 1, SCREEN_COLS, ' ');
  g_state.redrawFlags |= REDRAW_SB_COORD;
}

// clang-format off
#pragma warn(unused-param, push, off)
static BYTE SaveRawData(const BYTE name[16], char deviceNumber)
{
  return 0;
}
#pragma warn(unused-param, pop)

#pragma warn(unused-param, push, off)
static BYTE LoadRawData(const BYTE name[16])
{
  return 0;
}
#pragma warn(unused-param, pop)
// clang-format on

static void PrintKeyGroup(const char* list[], BYTE count, BYTE x, BYTE y)
{
  register BYTE i = 0;
  gotoxy(x, y);
  revers(1);
  textcolor(COLOUR_PINK);
  cputs(list[0]);
  revers(0);
  textcolor(COLOUR_WHITE);

  for (i = 1; i < count; ++i) {
    gotoxy(x, y + i);
    cputs(list[i]);
  }
}

static void ShowHelp()
{
  // clang-format off
  const char* fileKeys[] = {
    "  file / txfer     ",
    "(not impl)       f5", // load
    "(not impl)     f7,r", // save raw
    "(not impl)     f7,b", // save basic
    "fetch slot       f9",
    "store slot      f11",
    "exit             f3",
  };

  const char* drawKeys[] = {
    "       tools       ",
    "pixel             p",
    "line              l",
    "box               x",
    "filled box      s-x",
    "(not impl)        o", // circle
    "(not impl)      s-o", // filled circle
  };

  const char* colorKeys[] = {
    "       color       ",
    "select         0..9",
    "               a..f",
    "prev component    -",
    "next component    +",
    "select background k",
    "sel pal bank ctrl+p",
  };

  const char* editKeys[] = {
    "       edit        ",
    "spacebar       draw",
    "del           erase",
    "clear        ctrl+n",
    "prev sprite       <",
    "next sprite       >",
    "change type       *",
    "toggle h-expand   h",
    "toggle v-expand   v",
    "(not impl)        \x1e", // toggle x-width
    "copy sprite  ctrl+c",
    "(not impl)   ctrl+h", // horz flip
    "(not impl)   ctrl+v", // vert flip
  };

  const char* displayKeys[] = {
    "     display       ",
    "aspect ratio  alt+r",
    "(not impl)    alt+d", // 25/50-line
  };

  const char* tipsNtricks[] = {
    "   tips & tricks   ",
    "press f11 to store ",
    "current sprite     ",
    "before exiting.    ",
  };
  // clang-format on

  POKE(0xD015, 0);

  flushkeybuf();
  clrscr();
  g_state.redrawFlags |= REDRAW_TOOL_HEADER;
  DrawHeader();
  PrintKeyGroup(fileKeys, ARRAY_SIZE(fileKeys), 0, 2);
  PrintKeyGroup(editKeys, ARRAY_SIZE(editKeys), 22, 2);
  PrintKeyGroup(drawKeys, ARRAY_SIZE(drawKeys), 0, 2 + ARRAY_SIZE(fileKeys) + 1);
  PrintKeyGroup(colorKeys, ARRAY_SIZE(colorKeys), 0, 2 + ARRAY_SIZE(fileKeys) + 1 + ARRAY_SIZE(drawKeys) + 1);
  PrintKeyGroup(displayKeys, ARRAY_SIZE(displayKeys), 42, 2);
  PrintKeyGroup(tipsNtricks, ARRAY_SIZE(tipsNtricks), 42, 16);

  textcolor(COLOUR_CYAN);
  revers(1);
  cputncxy(22, SCREEN_ROWS - 4, SCREEN_COLS - 22, 32);
  cputsxy(22, SCREEN_ROWS - 3, " mega65 sprite editor  v0.10 (c) 2021 hernan di pietro    ");
  cputsxy(22, SCREEN_ROWS - 2, " mouse/joy code by paul gardner-stephen.                  ");
  cputncxy(22, SCREEN_ROWS - 1, SCREEN_COLS - 22, 32);
  revers(0);

  cgetc();
  clrscr();
  POKE(0xD015, 7);
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

static void UpdateAndFullRedraw(BOOL fFetchSlot)
{
  UpdateSpriteParameters(fFetchSlot);
  EraseCanvasSpace();
  SetRedrawFullCanvas();
}

unsigned short joy_delay_countdown = 0;
unsigned char fire_lock = 0;

unsigned short mx, my;

static void SetBackground()
{
  g_state.redrawFlags = REDRAW_SB_COLOR;
  if (g_state.spriteColorMode == SPR_COLOR_MODE_16COLOR) {
    g_state.color[g_state.currentColorIdx] = 0;
  }
  else {
    g_state.currentColorIdx = COLOR_BACK;
  }
}

static void MainLoop()
{
  static BYTE editColorCounter = 0;
  unsigned char buf[64];
  unsigned char key = 0, keymod = 0;
  BYTE redrawStatusBar = FALSE;

  mouse_set_bounding_box(0 + 24, 0 + 50, 319 + 24 - 8, 199 + 50);
  mouse_warp_to(24, 100);
  mouse_bind_to_sprite(0);

  while (1) {
    POKE(0xD028, editCursorColorMap[editColorCounter++ / 16]); // Update editor cursor color

    mouse_update_position(&mx, &my);
    if ((my >= 66 && my <= 233) && (mx >= 55 && mx <= 235)) {
      if ((((mx - 55) / 8) != g_state.cursorX) || (((my - 66) / 8) != g_state.cursorY)) {
        g_state.drawCellFn(g_state.cursorX, g_state.cursorY);
        g_state.cursorX = (mx - 55) / 8;
        g_state.cursorY = (my - 66) / 8;
        g_state.updateCursorXFn();
        g_state.updateCursorYFn();
        fire_lock = 0;
      }
    }
    if (kbhit()) {
      key = cgetc();
      joy_delay_countdown = 0;
    }
    else {
      key = 0;
      if ((PEEK(0xDC00) & 0x1f) != 0x1f) {
        // Check joysticks

        if (!(PEEK(0xDC00) & 0x10)) {
          // Toggle pixel
          if (!fire_lock) {
            key = 0x20;
            joy_delay_countdown = joy_delay_countdown >> 3;
          }
          fire_lock = 1;
        }
        else
          fire_lock = 0;

        if (joy_delay_countdown)
          joy_delay_countdown--;
        else {
          switch (PEEK(0xDC00) & 0xf) {
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
    if (!key) {
      if (mouse_clicked()) {
        if (!fire_lock)
          key = 0x20;
        fire_lock = 1;
      }
    }
    switch (key) {
    case 0:
      // No key, do nothing
      break;

      /* ------------------------------------ HELP ----------------------------------- */

    case 31: // HELP
      ShowHelp();
      EraseCanvasSpace();
      SetRedrawFullCanvas();
      textcolor(COLOUR_BLUE);
      cputncxy(0, SCREEN_ROWS - 1, SCREEN_COLS, ' ');
      g_state.redrawFlags = REDRAW_SB_ALL;
      break;

      /* ------------------------- CURSOR MOVEMENT GROUP ----------------------------- */

    case CH_CURS_DOWN:
      g_state.drawShapeFn(g_state.drawCellFn);
      g_state.cursorY = (g_state.cursorY == g_state.spriteHeight - 1) ? 0 : (g_state.cursorY + 1);
      g_state.redrawFlags = REDRAW_SB_COORD | REDRAW_TOOL_PREVIEW;
      g_state.updateCursorYFn();
      break;

    case CH_CURS_UP:
      g_state.drawShapeFn(g_state.drawCellFn);
      g_state.cursorY = (g_state.cursorY == 0) ? (g_state.spriteHeight - 1) : (g_state.cursorY - 1);
      g_state.redrawFlags = REDRAW_SB_COORD | REDRAW_TOOL_PREVIEW;
      g_state.updateCursorYFn();
      break;

    case CH_CURS_LEFT:
      g_state.drawShapeFn(g_state.drawCellFn);
      g_state.cursorX = (g_state.cursorX == 0) ? (g_state.spriteWidth - 1) : (g_state.cursorX - 1);
      g_state.redrawFlags = REDRAW_SB_COORD | REDRAW_TOOL_PREVIEW;
      g_state.updateCursorXFn();
      break;

    case CH_CURS_RIGHT:
      g_state.drawShapeFn(g_state.drawCellFn);
      g_state.cursorX = (g_state.cursorX == g_state.spriteWidth - 1) ? 0 : (g_state.cursorX + 1);
      g_state.redrawFlags = REDRAW_SB_COORD | REDRAW_TOOL_PREVIEW;
      g_state.updateCursorXFn();
      break;

      /* ------------------------- EDIT GROUP ------------------------------------------ */

    case ' ':

      if (g_state.drawingTool == DRAWING_TOOL_PIXEL) {
        SetRect(&g_state.redrawRect, g_state.cursorX, g_state.cursorY, g_state.cursorX + 1, g_state.cursorY + 1);
        g_state.paintCellFn(g_state.cursorX, g_state.cursorY);
      }
      else {
        if (g_state.toolActive) {
          g_state.toolActive = 0;
          g_state.drawShapeFn(g_state.paintCellFn);
          redrawStatusBar = TRUE;
          SetRedrawFullCanvas();
        }
        else {
          g_state.toolOrgX = g_state.cursorX;
          g_state.toolOrgY = g_state.cursorY;
          g_state.toolActive = 1;
        }
      }
      break;

    case 20: // DEL
    {
      const BYTE cIndex = g_state.currentColorIdx;
      SetRect(&g_state.redrawRect, g_state.cursorX, g_state.cursorY, g_state.cursorX + 1, g_state.cursorY + 1);
      SetBackground();
      g_state.paintCellFn(g_state.cursorX, g_state.cursorY);
      g_state.currentColorIdx = cIndex;
    }

    break;

    case ',':
    case '.':

      PutSpriteDataToSlot();
      g_state.redrawFlags = REDRAW_SB_ALL;

      if (key == '.' && g_state.spriteNumber++ == SPRITE_MAX_COUNT - 1)
        g_state.spriteNumber = 0;
      else if (key == ',' && g_state.spriteNumber-- == 0)
        g_state.spriteNumber = SPRITE_MAX_COUNT - 1;

      UpdateAndFullRedraw(TRUE);
      g_state.cursorX = MIN(g_state.spriteWidth - 1, g_state.cursorX);
      g_state.cursorY = MIN(g_state.spriteHeight - 1, g_state.cursorY);

      break;

    case '*':
      g_state.toolActive = 0;
      g_state.redrawFlags = REDRAW_SB_ALL;
      switch (g_state.spriteColorMode) {
      case SPR_COLOR_MODE_16COLOR:
        // Switch to Hi-Res
        FREEZE_POKE(REG_SPR_16COL, FREEZE_PEEK(REG_SPR_16COL) & ~(1 << g_state.spriteNumber));
        FREEZE_POKE(REG_SPR_MULTICOLOR, FREEZE_PEEK(REG_SPR_MULTICOLOR) & ~(1 << g_state.spriteNumber));
        break;
      case SPR_COLOR_MODE_MULTICOLOR:
        // Switch to 16-col
        FREEZE_POKE(REG_SPR_16COL, FREEZE_PEEK(REG_SPR_16COL) | (1 << g_state.spriteNumber));
        FREEZE_POKE(REG_SPR_MULTICOLOR, FREEZE_PEEK(REG_SPR_MULTICOLOR) & ~(1 << g_state.spriteNumber));
        break;
      case SPR_COLOR_MODE_MONOCHROME:
        // Switch to Multicol
        FREEZE_POKE(REG_SPR_16COL, FREEZE_PEEK(REG_SPR_16COL) & ~(1 << g_state.spriteNumber));
        FREEZE_POKE(REG_SPR_MULTICOLOR, FREEZE_PEEK(REG_SPR_MULTICOLOR) | (1 << g_state.spriteNumber));
        break;
      }
      UpdateAndFullRedraw(FALSE);
      break;

    case '@': // 94: // Up-arrow
      // TODO: Disabled until we can fix it; issue #77
      // POKE(LOCAL_REG_SPRX64EN, PEEK(LOCAL_REG_SPRX64EN) ^ (1 << PREVIEW_SPRITE_NUM));
      // FREEZE_POKE(REG_SPRX64EN, FREEZE_PEEK(REG_SPRX64EN) ^ (1 << g_state.spriteNumber));
      // g_state.redrawFlags = REDRAW_SB_ALL;
      // g_state.updateCursorXFn = PEEK(LOCAL_REG_SPRX64EN) & (1 << PREVIEW_SPRITE_NUM) ? UpdateCursorXMSB : UpdateCursorX;
      // UpdateAndFullRedraw(FALSE);
      // UpdateSpritePreview();
      break;

    case 3: // CTRL-C
      Ask("copy sprite to (0-7)? ", buf, 1);
      if (buf[0] >= '0' && buf[0] <= '7') {
        BYTE toSprite = buf[0] - 48;
        if (SPRITE_SIZE_BYTES(toSprite) == g_state.spriteSizeBytes) {
          // Display error if bytes do not match
          CopySpriteData(SPRITE_DATA_ADDR(toSprite));
        }
        else {
          bordercolor(COLOUR_RED);
          cgetc();
        }
        bordercolor(DEFAULT_BORDER_COLOR);
      }
      break;

    case 118: // "V"-expand
      POKE(0xD017, PEEK(0xD017) ^ (1 << PREVIEW_SPRITE_NUM));
      FREEZE_POKE(VIC_BASE + 0x17, FREEZE_PEEK(VIC_BASE + 0x17) ^ (1 << g_state.spriteNumber));
      bordercolor(DEFAULT_BORDER_COLOR);
      UpdateSpritePreview();
      g_state.redrawFlags = REDRAW_SB_INFO;
      break;

    case 104: // "H"-expand
      POKE(0xD01D, PEEK(0xD01D) ^ (1 << PREVIEW_SPRITE_NUM));
      FREEZE_POKE(VIC_BASE + 0x1D, FREEZE_PEEK(VIC_BASE + 0x1D) ^ (1 << g_state.spriteNumber));
      bordercolor(DEFAULT_BORDER_COLOR);
      UpdateSpritePreview();
      g_state.redrawFlags = REDRAW_SB_INFO;
      break;

    case 8: // CTRL-H  (Horz flip)
      // TODO: implement; issue #78
      break;

    case 22: // CTRL-V  (Vert flip)
      // TODO: implement; issue #78
      break;

      /* ------------------------------- COLOR GROUP -------------------------- */

    case '+':
      g_state.redrawFlags = REDRAW_SB_COLOR;
      g_state.currentColorIdx = (g_state.currentColorIdx + 1)
                              % ((g_state.spriteColorMode == SPR_COLOR_MODE_MONOCHROME
                                     | g_state.spriteColorMode == SPR_COLOR_MODE_16COLOR)
                                      ? 2
                                      : 4);
      break;

    case '-':
      g_state.redrawFlags = REDRAW_SB_COLOR;
      g_state.currentColorIdx = (g_state.currentColorIdx - 1)
                              % ((g_state.spriteColorMode == SPR_COLOR_MODE_MONOCHROME
                                     | g_state.spriteColorMode == SPR_COLOR_MODE_16COLOR)
                                      ? 2
                                      : 4);
      break;

    case 107: // k
      SetBackground();
      break;

    case 16: // Ctrl+P
    {
      const unsigned char cBankReg = FREEZE_PEEK(REG_SPRPALSEL);
      const unsigned char cSprPalBank = (cBankReg & 0xC) >> 2;
      gotoxy(1, 1);
      cputdec(cSprPalBank, 0, 4);
      FREEZE_POKE(REG_SPRPALSEL, (cBankReg & ~0xC) | (((cSprPalBank + 1) % 4) << 2));
      bordercolor(DEFAULT_BORDER_COLOR);
      SetupTextPalette();
    } break;

      /* ------------------------- DISPLAY GROUP --------------------------------------- */

    case 240: // ALT-D
      // TODO: Disabled until we can fix it; issue #74
      // setscreensize(80, 50);
      // UpdateAndFullRedraw(FALSE);
      break;

    case 174: // ALT-R
      g_state.wideScreenMode = ~g_state.wideScreenMode & 1;
      UpdateAndFullRedraw(FALSE);
      break;

      /* --------------------------- FILE GROUP ----------------------------- */

    case 0xF3: // F3
      Ask("exit sprite editor: are you sure (yes/no)? ", buf, 3);
      if (buf[0] == 'y' && buf[1] == 'e' && buf[2] == 's') {
        return;
      }
      break;

    case 0xF5: // F5 LOAD
      // TODO: implement
      // Ask("enter file name to load: ", buf, 19);
      break;

    case 0xF7: // F7 SAVE
      // TODO: implement
      // Ask("enter file name to save: ", buf, 19);
      break;

    case 0xF9: // F9 Fetch from slot
      UpdateAndFullRedraw(TRUE);
      break;

    case 0xFB: // F11 Save to slot
      PutSpriteDataToSlot();
      bordercolor(DEFAULT_BORDER_COLOR);
      break;

    case 14: // CTRL-N
      Ask("clear sprite (yes/no)? ", buf, 3);
      if (buf[0] == 'y' && buf[1] == 'e' && buf[2] == 's') {
        ClearSprite();
      }
      break;

      // case ASC_HELP:
      //     break;

      /* --------------------------- DRAWING TOOLS GROUP ----------------------- */

      /*
      case 111: // o = circle  tool
        SetDrawTool(DRAWING_TOOL_CIRCLE);
        g_state.redrawFlags = REDRAW_SB_TOOLS | REDRAW_TOOL_PREVIEW;

        SetRedrawFullCanvas();
        break;

      case 79: // "O" = filled circle  tool
        SetDrawTool(DRAWING_TOOL_FILLED_CIRCLE);
        g_state.redrawFlags = REDRAW_SB_TOOLS | REDRAW_TOOL_PREVIEW;
        SetRedrawFullCanvas();
        break;
      */

    case 112: // p=pixel tool
      SetDrawTool(DRAWING_TOOL_PIXEL);
      g_state.redrawFlags = REDRAW_SB_TOOLS | REDRAW_TOOL_PREVIEW;
      SetRedrawFullCanvas();
      g_state.toolActive = 0;
      break;

    case 120: // x = draw box
      SetDrawTool(DRAWING_TOOL_BOX);
      g_state.redrawFlags = REDRAW_SB_TOOLS | REDRAW_TOOL_PREVIEW;
      SetRedrawFullCanvas();
      break;

    case 88: // "X"
      SetDrawTool(DRAWING_TOOL_FILLEDBOX);
      g_state.redrawFlags = REDRAW_SB_TOOLS | REDRAW_TOOL_PREVIEW;
      SetRedrawFullCanvas();
      break;

    case 108: // l = line
      SetDrawTool(DRAWING_TOOL_LINE);
      g_state.redrawFlags = REDRAW_SB_TOOLS | REDRAW_TOOL_PREVIEW;
      SetRedrawFullCanvas();
      break;

    default:
      if (key >= '0' && key <= '9') {
        g_state.color[g_state.currentColorIdx] = key - 48;
        g_state.redrawFlags = REDRAW_SB_COLOR | REDRAW_TOOL_PREVIEW;
        SetRedrawFullCanvas();
        UpdateColorRegs();
      }
      else if (key >= 97 && key <= 102) // a..f
      {
        g_state.color[g_state.currentColorIdx] = 10 + key - 'a';
        g_state.redrawFlags = REDRAW_SB_COLOR | REDRAW_TOOL_PREVIEW;
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
