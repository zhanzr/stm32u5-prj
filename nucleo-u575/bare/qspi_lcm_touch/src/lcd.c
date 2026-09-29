/*
  lcd.c - NV3030B 1.83" 240x284 LCD driver (nucleo-u575 port, MD183
  module). Ported from the ch32v307 reference (same module, vendor
  verbatim registers).

  The NV3030B runs the wrapped-command protocol: every command is one CS
  frame - the 4-byte header 02 00 <cmd> 00, then the parameter / pixel
  bytes in the same frame. See interface.c.

  Every pixel burst is chunked into whole-row transactions: the window is
  set once (2A/2B), the first row goes out as 2C and the remaining rows as
  3C (write memory continue). The panel honours byte-granular transactions
  between 2C/3C chunks.

  Bus: plain single-lane SPI, soft (bit-banged) or hardware (SPI1) -
  selectable at runtime, see interface.h. CS=PA4, SCK=PA5, MOSI=PA7.
  The module has no reset pin (the vendor sequence settles CS - here just
  the inter-command delays - before the init commands) and no backlight
  pin (hardwired on-module). Write-only: no readback.
  Touch: CST816D over hardware I2C1 (SCL=PB8, SDA=PB9) - see touch.c.
*/

#include <string.h>
#include <stdint.h>
#include "board.h"
#include "lcd.h"
#include "interface.h"

/* ---- runtime drawing window ----
 * Every draw is relative to this window (origin + size). It defaults to
 * the full panel and can be shrunk at runtime. */
static uint16_t s_win_x = 0, s_win_y = 0;
static uint16_t s_win_w = COL, s_win_h = ROW;

void LCD_SetWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (x >= COL || y >= ROW) { return; }
    if (w == 0U || h == 0U)   { return; }
    if (x + w > COL) { w = (uint16_t)(COL - x); }
    if (y + h > ROW) { h = (uint16_t)(ROW - y); }
    s_win_x = x;
    s_win_y = y;
    s_win_w = w;
    s_win_h = h;
}

void LCD_ResetWindow(void)
{
    s_win_x = 0U;
    s_win_y = 0U;
    s_win_w = COL;
    s_win_h = ROW;
}

uint16_t LCD_W(void)
{
    return s_win_w;
}

uint16_t LCD_H(void)
{
    return s_win_h;
}

/* One pixel row of RGB565 bytes (big-endian on the wire). */
static uint8_t s_row[COL * 2U];

/* No reset pin on this module: the vendor sequence settles CS (high,
 * then low) and waits before the init commands. CS is hardware-managed
 * here, so only the delays remain. */
void LCD_RESET(void)
{
    HAL_Delay(20);
    HAL_Delay(100);
}

/* =====================================================================
   Init sequence - NV3030B (vendored TK018F3716 example, verbatim
   registers): command-set enable, panel power/gate/source timing,
   gamma, 16bpp COLMOD, MADCTL 0x08, IPS inversion on, sleep out,
   display on.

   One CS frame per command: header 02 00 <cmd> 00 then the parameters.
   ===================================================================== */
void LCD_IC_Init(void)
{
    static const uint8_t p2[] = { 0x06, 0x08 };

    static const uint8_t p61[] = { 0x07, 0x04 };
    static const uint8_t p62[] = { 0x00, 0x44, 0x45 };
    static const uint8_t p63[] = { 0x41, 0x07, 0x12, 0x12 };
    static const uint8_t p64[] = { 0x37 };
    static const uint8_t p65[] = { 0x09, 0x10, 0x21 };  /* VSP          */
    static const uint8_t p66[] = { 0x09, 0x10, 0x21 };  /* VSN          */
    static const uint8_t p67[] = { 0x20, 0x40 };        /* src_neg_time */
    static const uint8_t p68[] = { 0x90, 0x4C, 0x7C, 0x66 }; /* gamma   */

    static const uint8_t pB1[] = { 0x0F, 0x02, 0x01 };
    static const uint8_t pB4[] = { 0x01 };
    static const uint8_t pB5[] = { 0x02, 0x02, 0x0A, 0x14 }; /* porch  */
    static const uint8_t pB6[] = { 0x04, 0x01, 0x9F, 0x00, 0x02 };
    static const uint8_t pDF[] = { 0x11 };              /* gamma select */

    static const uint8_t pE2[] = { 0x13, 0x00, 0x00, 0x30, 0x33, 0x3F };
    static const uint8_t pE5[] = { 0x3F, 0x33, 0x30, 0x00, 0x00, 0x13 };
    static const uint8_t pE1[] = { 0x00, 0x57 };        /* PRP          */
    static const uint8_t pE4[] = { 0x58, 0x00 };        /* PRN          */
    static const uint8_t pE0[] = { 0x01, 0x03, 0x0E, 0x0E, 0x0C, 0x15, 0x19 };
    static const uint8_t pE3[] = { 0x1A, 0x16, 0x0C, 0x0F, 0x0E, 0x0D, 0x02, 0x01 };
    static const uint8_t pE6[] = { 0x00, 0xFF };
    static const uint8_t pE7[] = { 0x01, 0x04, 0x03, 0x03, 0x00, 0x12 };
    static const uint8_t pE8[] = { 0x00, 0x70, 0x00 };  /* source       */
    static const uint8_t pEC[] = { 0x52 };              /* gate         */
    static const uint8_t pF1[] = { 0x01, 0x01, 0x02 };
    static const uint8_t pF6[] = { 0x09, 0x10, 0x00, 0x00 };
    static const uint8_t pFD_exit[] = { 0xFA, 0xFC };   /* cmd set off  */

    static const uint8_t p3A[] = { 0x05 };              /* 16bpp        */
    static const uint8_t p36[] = { 0x08 };              /* MADCTL       */
    static const uint8_t p35[] = { 0x00 };              /* TE polarity  */

    QSPI_Write(0xFD, p2, sizeof p2);        /* command set enable    */
    QSPI_Write(0x61, p61, sizeof p61);      /* gate/source timing    */
    QSPI_Write(0x62, p62, sizeof p62);
    QSPI_Write(0x63, p63, sizeof p63);
    QSPI_Write(0x64, p64, sizeof p64);
    QSPI_Write(0x65, p65, sizeof p65);
    QSPI_Write(0x66, p66, sizeof p66);
    QSPI_Write(0x67, p67, sizeof p67);
    QSPI_Write(0x68, p68, sizeof p68);
    QSPI_Write(0xB1, pB1, sizeof pB1);
    QSPI_Write(0xB4, pB4, sizeof pB4);
    QSPI_Write(0xB5, pB5, sizeof pB5);
    QSPI_Write(0xB6, pB6, sizeof pB6);
    QSPI_Write(0xDF, pDF, sizeof pDF);
    QSPI_Write(0xE2, pE2, sizeof pE2);      /* positive gamma VRP    */
    QSPI_Write(0xE5, pE5, sizeof pE5);      /* negative gamma VRN    */
    QSPI_Write(0xE1, pE1, sizeof pE1);
    QSPI_Write(0xE4, pE4, sizeof pE4);
    QSPI_Write(0xE0, pE0, sizeof pE0);      /* PKP                   */
    QSPI_Write(0xE3, pE3, sizeof pE3);      /* PKN                   */
    QSPI_Write(0xE6, pE6, sizeof pE6);
    QSPI_Write(0xE7, pE7, sizeof pE7);
    QSPI_Write(0xE8, pE8, sizeof pE8);
    QSPI_Write(0xEC, pEC, sizeof pEC);
    QSPI_Write(0xF1, pF1, sizeof pF1);
    QSPI_Write(0xF6, pF6, sizeof pF6);

    /* Vendor QSPI-enable block (ESP32 example, the only vendor reference
     * that drives this module in QSPI): unlock + read-channel config. */
    QSPI_EnableBlock();

    QSPI_Write(0xFD, pFD_exit, sizeof pFD_exit);  /* command set off */
    QSPI_Write(0x3A, p3A, sizeof p3A);      /* COLMOD 16bpp          */
    QSPI_Write(0x36, p36, sizeof p36);      /* MADCTL                */
    QSPI_Write(0x35, p35, sizeof p35);

    QSPI_Cmd(0x21);                         /* INVON - IPS inversion */
    QSPI_Cmd(0x11);                         /* sleep out             */
    HAL_Delay(120);                         /* datasheet: 120 ms     */
    QSPI_Cmd(0x29);                         /* display on            */
    HAL_Delay(10);
}

void LCD_Init(void)
{
    /* Power-on settle. This module has no reset pin, so the panel's own
     * power-on reset and its charge pumps are tied to board power; on a
     * COLD power-on the first commands can arrive while the panel is
     * still starting up and are lost. Wait here, before the very first
     * clock edge. The LCD_Reinit() path skips this (panel already up). */
    HAL_Delay(LCD_POWER_SETTLE_MS);

    LCD_RESET();
    LCD_IC_Init();

    /* Clear to black. */
    DispColor(C565_BLACK);
}

/* Re-init an already-running panel. NV3030B has no software-reset
 * command (public set: 00h, 04h-0Fh, 10h-13h, ... no 01h), so sleep in
 * is the closest equivalent: it stops the DC-DC, oscillator and scan -
 * the next init sequence then starts from a clean state. Without this,
 * re-initializing a live panel can latch it (needs a power cycle). */
void LCD_Reinit(void)
{
    QSPI_Cmd(0x28);               /* display off */
    QSPI_Cmd(0x10);               /* sleep in    */
    HAL_Delay(20);                /* datasheet: >=5 ms before new cmds */

    LCD_RESET();
    LCD_IC_Init();
}

/* =====================================================================
   Pixel streaming. The drawing window is set once (2A/2B), then each
   row of s_row goes out as one transaction: 2C for the first row,
   3C (write memory continue) for the rest.
   ===================================================================== */

/* Set the pixel window (absolute coords + the runtime window origin).
 * Window setup is a command + parameters: fully single-line. */
void LCD_SetAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t p[4];

    x1 = (uint16_t)(x1 + s_win_x);
    y1 = (uint16_t)(y1 + s_win_y);
    x2 = (uint16_t)(x2 + s_win_x);
    y2 = (uint16_t)(y2 + s_win_y);

    p[0] = (uint8_t)(x1 >> 8);
    p[1] = (uint8_t)x1;
    p[2] = (uint8_t)(x2 >> 8);
    p[3] = (uint8_t)x2;
    QSPI_Write(0x2A, p, 4U);

    p[0] = (uint8_t)(y1 >> 8);
    p[1] = (uint8_t)y1;
    p[2] = (uint8_t)(y2 >> 8);
    p[3] = (uint8_t)y2;
    QSPI_Write(0x2B, p, 4U);
}

static uint16_t rgb888_to_rgb565(uint32_t c)
{
    uint16_t r = (uint16_t)((c & 0x00F80000UL) >> 8);
    uint16_t g = (uint16_t)((c & 0x0000FC00UL) >> 5);
    uint16_t b = (uint16_t)((c & 0x000000F8UL) >> 3);
    return (uint16_t)(r | g | b);
}

/* Current foreground/background RGB565 + text-transparent flag. */
static uint16_t s_Color     = C565_BLACK;
static uint16_t s_BackColor = C565_BLACK;
static uint8_t  s_Transparent = 0;
static pFONT  *s_AsciiFont = NULL;

void LCD_SetColor(uint32_t rgb888)
{
    s_Color = rgb888_to_rgb565(rgb888);
}

void LCD_SetBackColor(uint32_t rgb888)
{
    s_BackColor = rgb888_to_rgb565(rgb888);
}

/* Solid fill: set the window, then stream `rows` identical rows. */
static void lcd_fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                     uint16_t color)
{
    uint16_t w = (uint16_t)(x2 - x1 + 1U);
    uint16_t n = (uint16_t)(y2 - y1 + 1U);
    uint32_t len = (uint32_t)w * 2U;
    uint16_t r;

    LCD_SetAddress(x1, y1, x2, y2);
    for (r = 0; r < w; r++)
    {
        s_row[r * 2U]     = (uint8_t)(color >> 8);
        s_row[r * 2U + 1U] = (uint8_t)color;
    }
    for (r = 0; r < n; r++)
    {
        QSPI_WritePixel((r == 0U) ? 0x2C : 0x3C, s_row, len);
    }
}

void LCD_Clear(void)
{
    lcd_fill(0, 0, (uint16_t)(s_win_w - 1U), (uint16_t)(s_win_h - 1U),
             s_BackColor);
}

void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    if ((width == 0U) || (height == 0U)) { return; }
    lcd_fill(x, y, (uint16_t)(x + width - 1U), (uint16_t)(y + height - 1U),
             s_BackColor);
}

/* =====================================================================
   Vendor demo screens - identical geometry/ordering to the reference.
   Each row of s_row goes out as one whole-row transaction (2C first,
   3C after).
   ===================================================================== */
void DispColor(uint32_t color)
{
    lcd_fill(0, 0, (uint16_t)(LCD_W() - 1U), (uint16_t)(LCD_H() - 1U),
             (uint16_t)color);
}

void DispFrame(void)
{
    uint16_t w = LCD_W();
    uint16_t h = LCD_H();
    uint16_t i, r, len = (uint16_t)(w * 2U);

    LCD_SetAddress(0, 0, (uint16_t)(w - 1U), (uint16_t)(h - 1U));

    /* Top / bottom rows: red, (w-2) white pixels, blue. */
    s_row[0] = 0xF8; s_row[1] = 0x00;
    for (i = 1; i < w - 1U; i++)
    {
        s_row[i * 2U] = 0xFF; s_row[i * 2U + 1U] = 0xFF;
    }
    s_row[(w - 1U) * 2U] = 0x00; s_row[(w - 1U) * 2U + 1U] = 0x1F;
    QSPI_WritePixel(0x2C, s_row, len);

    /* Interior rows: red, (w-2) black pixels, blue. */
    for (i = 1; i < w - 1U; i++)
    {
        s_row[i * 2U] = 0x00; s_row[i * 2U + 1U] = 0x00;
    }
    for (r = 1; r < h - 1U; r++)
    {
        QSPI_WritePixel(0x3C, s_row, len);
    }

    /* Bottom row: red, (w-2) white pixels, blue. */
    for (i = 1; i < w - 1U; i++)
    {
        s_row[i * 2U] = 0xFF; s_row[i * 2U + 1U] = 0xFF;
    }
    QSPI_WritePixel(0x3C, s_row, len);
}

void DispGrayHor16(void)
{
    uint16_t w = LCD_W();
    uint16_t h = LCD_H();
    uint16_t unit = (uint16_t)(w / 16U);
    uint16_t lead = (uint16_t)(w % 16U);
    uint16_t j, r, x, len = (uint16_t)(w * 2U);

    LCD_SetAddress(0, 0, (uint16_t)(w - 1U), (uint16_t)(h - 1U));

    for (x = 0; x < lead; x++)
    {
        s_row[x * 2U] = 0x00; s_row[x * 2U + 1U] = 0x00;
    }
    for (j = 0; j < 16U; j++)
    {
        uint16_t c = (uint16_t)(((((j * 2U) << 3) | ((j * 4U) >> 3)) << 8) |
                                (((j * 4U) << 5) | (j * 2U)));
        for (r = 0; r < unit; r++)
        {
            s_row[(lead + r) * 2U]     = (uint8_t)(c >> 8);
            s_row[(lead + r) * 2U + 1U] = (uint8_t)c;
        }
        lead = (uint16_t)(lead + unit);
    }
    for (r = 0; r < h; r++)
    {
        QSPI_WritePixel((r == 0U) ? 0x2C : 0x3C, s_row, len);
    }
}

void DispBand(void)
{
    static const uint16_t color[8] = { 0xF800, 0xF800, 0x07E0, 0x07E0,
                                       0x001F, 0x001F, 0xFFFF, 0xFFFF };
    uint16_t w = LCD_W();
    uint16_t h = LCD_H();
    uint16_t band = (uint16_t)(h / 8U);
    uint16_t rem  = (uint16_t)(h % 8U);
    uint16_t r, i, len = (uint16_t)(w * 2U);
    uint8_t  first = 1U;

    LCD_SetAddress(0, 0, (uint16_t)(w - 1U), (uint16_t)(h - 1U));

    for (i = 0; i < 8U; i++)
    {
        for (r = 0; r < w; r++)
        {
            s_row[r * 2U]     = (uint8_t)(color[i] >> 8);
            s_row[r * 2U + 1U] = (uint8_t)color[i];
        }
        for (r = 0; r < band; r++)
        {
            QSPI_WritePixel(first ? 0x2C : 0x3C, s_row, len);
            first = 0U;
        }
    }
    for (r = 0; r < w; r++)
    {
        s_row[r * 2U]     = (uint8_t)(color[7] >> 8);
        s_row[r * 2U + 1U] = (uint8_t)color[7];
    }
    for (r = 0; r < rem; r++)
    {
        QSPI_WritePixel(first ? 0x2C : 0x3C, s_row, len);
        first = 0U;
    }
}

void StopDelay(uint16_t ms)
{
    HAL_Delay(ms);
}

/* =====================================================================
   Colors, addressing, text and 2D drawing - the standard st7789-style
   drawing API, adapted to the 240x284 NV3030B.
   ===================================================================== */

/* ---- ASCII text ---- */
void LCD_SetAsciiFont(pFONT *font)
{
    s_AsciiFont = font;
}

void LCD_ShowTransparent(uint8_t mode)
{
    s_Transparent = mode;
}

void LCD_DisplayChar(uint16_t x, uint16_t y, uint8_t c)
{
    uint16_t index;
    uint8_t  disChar;
    /* Must hold the LARGEST font's pixels (8x16 = 128); the 6x12 font
     * only fills the first 72. */
    uint16_t Buff[8 * 16];

    if ((s_AsciiFont == NULL) || (c < 0x20U) || (c > 0x7EU))
    {
        return;
    }
    c -= 0x20U;   /* table starts at space */

    if (s_Transparent)
    {
        uint16_t bytesPerRow = (uint16_t)(s_AsciiFont->Sizes / s_AsciiFont->Height);
        for (uint16_t row = 0; row < s_AsciiFont->Height; row++)
        {
            for (uint16_t col = 0; col < s_AsciiFont->Width; col++)
            {
                disChar = s_AsciiFont->pTable[(uint16_t)c * s_AsciiFont->Sizes
                          + (uint16_t)row * bytesPerRow + (col / 8)];
                if (disChar & (uint8_t)(1U << (col % 8)))
                {
                    LCD_DrawPoint((uint16_t)(x + col), (uint16_t)(y + row),
                                  s_Color);
                }
            }
        }
        return;
    }

    index = 0;
    /* Row-major fill matching the font layout: each glyph row is
     * bytesPerRow bytes, bit 0 = leftmost pixel (the same convention the
     * transparent path uses). Filling linearly from the raw bit stream
     * instead smears the 6x12 font: its 6-bit rows are byte-packed with 2
     * padding bits that would bleed into the next row. */
    uint16_t bytesPerRow = (uint16_t)(s_AsciiFont->Sizes / s_AsciiFont->Height);
    for (uint16_t row = 0; row < s_AsciiFont->Height; row++)
    {
        for (uint16_t col = 0; col < s_AsciiFont->Width; col++)
        {
            disChar = s_AsciiFont->pTable[(uint16_t)c * s_AsciiFont->Sizes
                      + (uint16_t)row * bytesPerRow + (col / 8)];
            Buff[index++] = (disChar & (uint8_t)(1U << (col % 8)))
                            ? s_Color : s_BackColor;
        }
    }
    LCD_CopyBuffer(x, y, s_AsciiFont->Width, s_AsciiFont->Height, Buff);
}

void LCD_DisplayString(uint16_t x, uint16_t y, char *p)
{
    while (x < COL && *p != '\0')
    {
        LCD_DisplayChar(x, y, (uint8_t)*p);
        x += (uint16_t)((s_AsciiFont != NULL) ? s_AsciiFont->Width : 6);
        p++;
    }
}

/* ---- 2D drawing ---- */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color)
{
    uint8_t p[2];

    LCD_SetAddress(x, y, x, y);
    p[0] = (uint8_t)(color >> 8);
    p[1] = (uint8_t)color;
    QSPI_WritePixel(0x2C, p, 2U);
}

void LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t height)
{
    if (height == 0U) { return; }
    lcd_fill(x, y, x, (uint16_t)(y + height - 1U), s_Color);
}

void LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t width)
{
    if (width == 0U) { return; }
    lcd_fill(x, y, (uint16_t)(x + width - 1U), y, s_Color);
}

void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0;
    int16_t yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0;
    int16_t numpixels = 0, curpixel = 0;

    deltax = ABS((int16_t)x2 - (int16_t)x1);
    deltay = ABS((int16_t)y2 - (int16_t)y1);
    x = x1; y = y1;

    if (x2 >= x1) { xinc1 = 1; xinc2 = 1; } else { xinc1 = -1; xinc2 = -1; }
    if (y2 >= y1) { yinc1 = 1; yinc2 = 1; } else { yinc1 = -1; yinc2 = -1; }

    if (deltax >= deltay)
    {
        xinc1 = 0; yinc2 = 0; den = deltax; num = deltax / 2;
        numadd = deltay; numpixels = deltax;
    }
    else
    {
        xinc2 = 0; yinc1 = 0; den = deltay; num = deltay / 2;
        numadd = deltax; numpixels = deltay;
    }
    for (curpixel = 0; curpixel <= numpixels; curpixel++)
    {
        LCD_DrawPoint((uint16_t)x, (uint16_t)y, s_Color);
        num += numadd;
        if (num >= den)
        {
            num -= den;
            x += xinc1;
            y += yinc1;
        }
        x += xinc2;
        y += yinc2;
    }
}

void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    LCD_DrawLine_H(x, y, width);
    LCD_DrawLine_H(x, (uint16_t)(y + height - 1), width);
    LCD_DrawLine_V(x, y, height);
    LCD_DrawLine_V((uint16_t)(x + width - 1), y, height);
}

void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r)
{
    int16_t Xadd = -(int16_t)r, Yadd = 0, err = 2 - 2 * (int16_t)r, e2;
    do
    {
        LCD_DrawPoint((uint16_t)(x - Xadd), (uint16_t)(y + Yadd), s_Color);
        LCD_DrawPoint((uint16_t)(x + Xadd), (uint16_t)(y + Yadd), s_Color);
        LCD_DrawPoint((uint16_t)(x + Xadd), (uint16_t)(y - Yadd), s_Color);
        LCD_DrawPoint((uint16_t)(x - Xadd), (uint16_t)(y - Yadd), s_Color);
        e2 = err;
        if (e2 <= Yadd)
        {
            Yadd++;
            err += (int16_t)(Yadd * 2 + 1);
            if (-Xadd == Yadd && e2 <= Xadd) { e2 = 0; }
        }
        if (e2 > Xadd)
        {
            Xadd++;
            err += (int16_t)(Xadd * 2 + 1);
        }
    }
    while (Xadd <= 0);
}

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    if ((width == 0U) || (height == 0U)) { return; }
    lcd_fill(x, y, (uint16_t)(x + width - 1U), (uint16_t)(y + height - 1U),
             s_Color);
}

void LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r)
{
    int32_t  D;
    uint32_t CurX, CurY;

    D = 3 - ((int32_t)r << 1);
    CurX = 0;
    CurY = r;
    while (CurX <= CurY)
    {
        if (CurY > 0)
        {
            LCD_DrawLine_V((uint16_t)(x - CurX), (uint16_t)(y - CurY),
                           (uint16_t)(2 * CurY));
            LCD_DrawLine_V((uint16_t)(x + CurX), (uint16_t)(y - CurY),
                           (uint16_t)(2 * CurY));
        }
        if (CurX > 0)
        {
            LCD_DrawLine_V((uint16_t)(x - CurY), (uint16_t)(y - CurX),
                           (uint16_t)(2 * CurX));
            LCD_DrawLine_V((uint16_t)(x + CurY), (uint16_t)(y - CurX),
                           (uint16_t)(2 * CurX));
        }
        if (D < 0)
        {
            D += (int32_t)(CurX << 2) + 6;
        }
        else
        {
            D += (int32_t)((CurX - CurY) << 2) + 10;
            CurY--;
        }
        CurX++;
    }
    LCD_DrawCircle(x, y, r);
}

void LCD_CopyBuffer(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                    const uint16_t *data)
{
    uint16_t r, c, len;
    uint8_t  first = 1U;
    uint16_t clip_w = width;

    if ((width == 0U) || (height == 0U)) { return; }

    /* The row staging buffer is COL wide; clip so a full-width request can
     * never overflow it (len is computed from the clipped width too). */
    if (clip_w > COL) { clip_w = COL; }
    len = (uint16_t)(clip_w * 2U);

    LCD_SetAddress(x, y, (uint16_t)(x + clip_w - 1U),
                   (uint16_t)(y + height - 1U));
    for (r = 0; r < height; r++)
    {
        for (c = 0; c < clip_w; c++)
        {
            uint16_t v = data[(uint32_t)r * width + c];
            s_row[c * 2U]     = (uint8_t)(v >> 8);
            s_row[c * 2U + 1U] = (uint8_t)v;
        }
        QSPI_WritePixel(first ? 0x2C : 0x3C, s_row, len);
        first = 0U;
    }
}
