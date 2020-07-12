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
 
    Version   0.5
    Date      2020-07-06

    CHANGELOG

    v0.5        Uses conio for proper initialization and some of its
                new features.  Color selection with MEGA/CTRL keys.

    v0.6        Multicolor and 16-color sprite support.
                
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
#define REG_SPRPTR_B2               (PEEK(VIC_BASE + 0x6E))
#define REG_SCREEN_RAM_ADDRESS      0x8000
#define REG_SCREEN_BASE_B0          (VIC_BASE + 0x60)
#define REG_SCREEN_BASE_B1          (VIC_BASE + 0x61)
#define REG_SCREEN_BASE_B2          (VIC_BASE + 0x62)
#define REG_SCREEN_BASE_B3          (VIC_BASE + 0x63) // Bits 0..3
#define IS_SPR_MULTICOLOR(n)        ((PEEK(VIC_BASE + 0x1C)) & (1 << (n)))
#define IS_SPR_16COL(n)             ((PEEK(VIC_BASE + 0x6B)) & (1 << (n)))
#define SPRITE_POINTER_ADDR         (((long)REG_SPRPTR_B0) | ((long)REG_SPRPTR_B1 << 8) | ((long)REG_SPRPTR_B2 << 16))
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
#define TOOLBOX_COLUMN              65

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
    BYTE color_source[4];
    BYTE currentColorIdx;
    BYTE cursorX, cursorY;
    BYTE canvasLeftX;
    void (*drawCellFn)(BYTE,BYTE);
    void (*togglePixelFn)(void);
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

    unsigned char test_16 [64]={
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
    0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
    0x48,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F};
#endif 

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

    POKE(53276UL, 2);

    setscreenaddr(0x8000);
    setscreensize(80,25);
    setextendedattrib(1);

#ifdef TEST_SPRITES
    POKE(VIC_BASE + 0x6B, 4);

    lcopy((long)test_mc,0x380+64,64);
    lcopy((long)test_16,0x380+64+64,64);
#endif

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
    const long byteAddr = (g_state.spriteDataAddr + (y * 3)) + (x / 2);
    const BYTE p = lpeek(byteAddr) & ( 0xF >> (x % 2));

    revers(1);
    gotoxy(g_state.canvasLeftX + (x * g_state.cellsPerPixel), y + 2);
    for (cell = 0; cell < g_state.cellsPerPixel; ++cell)
    {
        textcolor(p & 0xF);
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

static void TogglePixelMono()
{    
    long byteAddr = (g_state.spriteDataAddr + (g_state.cursorY * 3)) + (g_state.cursorX / 8);
    lpoke(byteAddr, lpeek(byteAddr) ^ (0x80 >> (g_state.cursorX % 8)));
}

static void TogglePixelMulti()
{    
    long byteAddr = (g_state.spriteDataAddr + (g_state.cursorY * 3)) + (g_state.cursorX / 4);
    const BYTE b = lpeek(byteAddr);
    const BYTE bitsel = (2 * (g_state.cursorX % 4));
    const BYTE p0 = b & (0x80 >> bitsel);
    const BYTE p1 = b & (0x40 >> bitsel);
    const BYTE mask = ((0x80 >> bitsel) | (0x40 >> bitsel));
    if ((g_state.currentColorIdx == COLOR_BACK) || (p0 | p1) != 0)
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

static void TogglePixel16Color()
{    
    long byteAddr = (g_state.spriteDataAddr + (g_state.cursorY * 3)) + (g_state.cursorX / 8);
    lpoke(byteAddr, lpeek(byteAddr) ^ (0x80 >> (g_state.cursorX % 8)));
}

void UpdateSpriteParameters(void)
{
    g_state.spriteHeight = 21;
    g_state.cellsPerPixel = 2;
    g_state.spriteSizeBytes = 64;
    g_state.spriteDataAddr = (long) 64 * lpeek(SPRITE_POINTER_ADDR + g_state.spriteNumber);

    if (IS_SPR_16COL(g_state.spriteNumber))
    {
        g_state.drawCellFn = Draw16ColorCell;
        g_state.togglePixelFn = TogglePixel16Color;
        g_state.spriteColorMode = SPR_COLOR_MODE_16COLOR;
        g_state.spriteWidth = 6;
        g_state.cellsPerPixel = 4;
    }
    else if (IS_SPR_MULTICOLOR(g_state.spriteNumber))
    {
        g_state.drawCellFn = DrawMulticolorCell;
        g_state.togglePixelFn = TogglePixelMulti;
        g_state.spriteColorMode = SPR_COLOR_MODE_MULTICOLOR;
        g_state.spriteWidth = 12;
        g_state.cellsPerPixel = 4;
    }
    else
    {
        g_state.drawCellFn = DrawMonoCell;
        g_state.togglePixelFn = TogglePixelMono;
        g_state.spriteColorMode = SPR_COLOR_MODE_MONOCHROME;
        g_state.spriteWidth = 24;
    }

    g_state.canvasLeftX =  (TOOLBOX_COLUMN / 2) - (g_state.spriteWidth * g_state.cellsPerPixel / 2);

}

static void EraseCanvasSpace()
{
    RECT rc;
    rc.top = 2;
    rc.left = 0;
    rc.right = TOOLBOX_COLUMN-1;
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
    cprintf("{home}{rvson}{lgrn}mega65 sprite editor v0.6                   copyright (c) 2020 hernan di pietro");
}                                                             

static void DrawToolbox()
{
    BYTE i;
    
    textcolor(1);
    gotoxy(TOOLBOX_COLUMN, 2);
    cputs("sprite ");
    cputdec(g_state.spriteNumber, 0, 0);
    cputs(g_state.spriteColorMode == SPR_COLOR_MODE_MONOCHROME ? " mono    " : 
        (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR ? " multi   " : " 16-col"));
    gotoxy(TOOLBOX_COLUMN, 3);
    textcolor(3);
    cputhex(g_state.spriteDataAddr, 7);
    cputc(' ');
    cputdec(g_state.cursorX, 0, 0);
    cputc(',');
    cputdec(g_state.cursorY, 0, 0);  

    textcolor(14);

    gotoxy(TOOLBOX_COLUMN, 5);
    cprintf("{rvson}{blk} {wht} {red} {cyan} {pur} {grn} {blu} {yel} ");
    gotoxy(TOOLBOX_COLUMN, 6);
    cprintf("{ora} {brn} {pink} {gray1} {gray2} {lgrn} {lblu} {gray3} {rvsoff}");
    
    revers(1);
    textcolor(g_state.color[g_state.currentColorIdx]);
    cputcxy(TOOLBOX_COLUMN + (g_state.color[g_state.currentColorIdx] % 8), 5 + (g_state.color[g_state.currentColorIdx] / 8), '*');
    revers(0);
    
    textcolor(COLOUR_GREY3);
    cputsxy(TOOLBOX_COLUMN + 11, 6, clrIndexName[g_state.currentColorIdx]);

    cputsxy(TOOLBOX_COLUMN, 10, ", . sel sprite");
    cputsxy(TOOLBOX_COLUMN, 11, "spc draw");
    if (g_state.spriteColorMode == SPR_COLOR_MODE_MULTICOLOR)
    {
        cputsxy(TOOLBOX_COLUMN, 12, "j   sel fgcolor");
        cputsxy(TOOLBOX_COLUMN, 13, "k   sel bkcolor");
        cputsxy(TOOLBOX_COLUMN, 14, "n   sel color1 ");
        cputsxy(TOOLBOX_COLUMN, 15, "m   sel color2 ");
    }
    else
    {
        cputsxy(TOOLBOX_COLUMN, 12, "j   sel fgcolor");
        cputsxy(TOOLBOX_COLUMN, 13, "k   sel bkcolor");
        cputsxy(TOOLBOX_COLUMN, 14, "               ");
        cputsxy(TOOLBOX_COLUMN, 15, "               ");
    }
    cputsxy(TOOLBOX_COLUMN, 16, "*   change type");
    cputsxy(TOOLBOX_COLUMN, 17, "s   save");
    cputsxy(TOOLBOX_COLUMN, 18, "l   load");
    cputsxy(TOOLBOX_COLUMN, 20, "z   palette");
    cputsxy(TOOLBOX_COLUMN, 19, "f1  help/info");
    cputsxy(TOOLBOX_COLUMN, 21, "f3  exit");

    for(i = 10; i <= 21; ++i)
    {
        cellcolor(TOOLBOX_COLUMN,   i, COLOUR_GREY3);
        cellcolor(TOOLBOX_COLUMN+1, i, COLOUR_GREY3);
        cellcolor(TOOLBOX_COLUMN+2, i, COLOUR_GREY3);
    }
}


static BYTE SaveRawData(const BYTE name[16], char deviceNumber)
{
    //return cbm_save(name, deviceNumber, g_state.spriteDataAddr, g_state.spriteSizeBytes);
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

static void MainLoop()
{
    FILEOPTIONS fileOpt;

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
                        joy_delay_countdown = joy_delay;
                        fire_lock = 0;
                        key = CH_CURS_RIGHT;
                        break;
                    case 0xB: // LEFT
                        fire_lock = 0;
                        joy_delay_countdown = joy_delay;
                        key = CH_CURS_LEFT;
                        break;
                    case 0xE: // UP
                        fire_lock = 0;
                        joy_delay_countdown = joy_delay;
                        key = CH_CURS_UP;
                        break;
                    case 0xD: // DOWN
                        fire_lock = 0;
                        joy_delay_countdown = joy_delay;
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
            EraseCanvasSpace();
            redrawCanvas = redrawTools = TRUE;
            break;

        case '.':
            if (g_state.spriteNumber-- == 0)
                g_state.spriteNumber = SPRITE_MAX_COUNT - 1;

            UpdateSpriteParameters();
            EraseCanvasSpace();
            redrawCanvas = redrawTools = TRUE;
            break;

        case ' ':
            g_state.togglePixelFn();
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

        // Color keys
        //         1   2   3   4   5   6   7   8
        // CTRL    $90 $05 $1c $9f $9c $1e &1f $9e 
        // MEGA    $81 $95 $96 $97 $98 $99 $9a $9b 

        case 0x90:// Ctrl + 1
            g_state.color[g_state.currentColorIdx] = 0;
            redrawTools = redrawCanvas = TRUE;
            break;

        case 0x5: // Ctrl + 2
            g_state.color[g_state.currentColorIdx] = 1;
            redrawTools = redrawCanvas = TRUE;
            break;

        case 0x1c: // Ctrl + 3
            g_state.color[g_state.currentColorIdx] = 2;
            redrawTools = redrawCanvas = TRUE;
            break;
        
        case 0x9f:  // Ctrl + 4
            g_state.color[g_state.currentColorIdx] = 3;
            redrawTools = redrawCanvas = TRUE;
        break;

        case 0x9c: // Ctrl + 5
            g_state.color[g_state.currentColorIdx] = 4;
            redrawTools = redrawCanvas = TRUE;
        break;

        case 0x1e: // Ctrl + 6
            g_state.color[g_state.currentColorIdx] = 5;
            redrawTools = redrawCanvas = TRUE;
        break;

        case 0x1f: // Ctrl + 7
            g_state.color[g_state.currentColorIdx] = 6;
            redrawTools = redrawCanvas = TRUE;
        break;

        case 0x9e: // Ctrl + 8
            g_state.color[g_state.currentColorIdx] = 7;
            redrawTools = redrawCanvas = TRUE;
        break;

        case 0x81: // CBM + 1
            g_state.color[g_state.currentColorIdx] = 8;
            redrawTools = redrawCanvas = TRUE;
      
        default:
            // Remaining CBM-2..8 keys (consecutive)
            if (key >= 149 && key <= 156)  // Mega+ 1-8
            {
                 g_state.color[g_state.currentColorIdx] = 9 + (key - 149);
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
