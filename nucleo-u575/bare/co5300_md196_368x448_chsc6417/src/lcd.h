/*
  lcd.h - CO5300 1.96" 368x448 LCD driver (nucleo-u575 port, TK0196M106
  module, MD196 form factor).
  Same drawing API as the nv3030b_md183_240x284_cst816d project (24-bit
  colors, lines, rectangles, circles, fills, buffer copy, ASCII text).
  Bus: plain single-lane SPI - soft (bit-banged) or hardware SPI1,
  selectable at runtime - see interface.h. CS=PA4, SCK=PA5, MOSI=PA7.
  The module has no DC and no backlight pin on this wiring (the vendor
  drives brightness over SPI, command 51h). No reset pin is wired either;
  see the LCD_RESET() note in lcd.c for why the CO5300 software reset
  (01h) cannot be used here and the power-on settle stands in for it.
  Touch: CHSC6417 over hardware I2C1 (SCL=PB8, SDA=PB9) - see touch.c.
  Panel geometry: 368x448, 16bpp (3Ah = 0x55). The vendor demo drives
  columns 0..367 directly (Lcd_ColorBox(0,0,368,448)), so the X GRAM
  offset defaults to 0 (LCD_X_OFFSET). NOTE: that demo comment refers to
  a *240x284* module, so it is not authoritative for this glass - if the
  image is shifted, try 16.
*/

#ifndef __LCD_H
#define __LCD_H

#include <stdint.h>
#include "lcd_fonts.h"

/* ---- Panel geometry / demo timing ---- */
#define LCD_Width    368
#define LCD_Height   448
#define COL       368
#define ROW       448
#define COL_Pre   0
#define ROW_Pre   0
#define Delay_Time 500

/* X GRAM offset added to every column address (see the header comment).
 * 0 matches the working vendor demo; 16 matches the vendor's commented
 * window on some units. */
#ifndef LCD_X_OFFSET
#define LCD_X_OFFSET 0
#endif

/* Display brightness - DCS WRDISBV (51h). The datasheet defines it as a
 * plain linear ratio, DBV/256:
 *
 *   0xFF = 100 %   0xC0 = 75 %   0xA0 = 62.5 %   0x80 = 50 %
 *   0x60 = 37.5 %  0x40 = 25 %   0x20 = 12.5 %   0x00 = 0 %
 *
 *   vendor sequence / vendor ESP32 example : 0xA0 (62.5 %)
 *   default here                           : 0x01 (0.78 %, lowest usable)
 *
 * 0x00 is the true floor but it is 0 % - the panel renders nothing, so
 * 0x01 is the lowest value that still shows an image. This panel is much
 * brighter than the other modules on this board, hence the minimum.
 * Tune it at build time:
 *
 *   bash build.sh -DLCD_BRIGHTNESS=0x40     # 25 %, if 0x01 is too dim
 *
 * NOTE: 51h is the ONLY brightness control available from the datasheet.
 * 7.4 Command List enumerates CMD1 only - the CMD2 pages (Gamma, OLED IP,
 * PWR Control) are not documented, so there is no spec'd deeper lever to
 * lower; driving those registers blind is not done. 63h (HBM brightness)
 * is inert because HBM_EN (66h) defaults to 0 = disabled. */
#ifndef LCD_BRIGHTNESS
#define LCD_BRIGHTNESS 0x01U
#endif

/* Power-on settle before the first panel command, in ms. No reset pin
 * is wired, so the panel's POR/charge-pump settling rides on board
 * power; a cold power-on needs this delay or the init sequence is
 * lost. */
#define LCD_POWER_SETTLE_MS 500

/* ---- runtime drawing window ----
 * All drawing is relative to this window; it defaults to the full panel
 * and can be shrunk at runtime. */
void LCD_SetWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void LCD_ResetWindow(void);   /* back to the full panel */
uint16_t LCD_W(void);         /* current window width  */
uint16_t LCD_H(void);         /* current window height */

/* ---- 24-bit colors (RGB888, converted to RGB565 by LCD_SetColor) ---- */
#define LCD_WHITE       0xFFFFFF
#define LCD_BLACK       0x000000
#define LCD_BLUE        0x0000FF
#define LCD_GREEN       0x00FF00
#define LCD_RED         0xFF0000
#define LCD_CYAN        0x00FFFF
#define LCD_MAGENTA     0xFF00FF
#define LCD_YELLOW      0xFFFF00

/* ---- raw RGB565 colors (vendor screens use these directly) ---- */
#define C565_WHITE   0xFFFF
#define C565_BLACK   0x0000
#define C565_BLUE    0x001F
#define C565_RED     0xF800
#define C565_MAGENTA 0xF81F
#define C565_GREEN   0x07E0
#define C565_CYAN    0x7FFF
#define C565_YELLOW  0xFFE0

#define ABS(X)  ((X) > 0 ? (X) : -(X))

/* ---- API: init / window / colors ---- */
void LCD_RESET(void);
void LCD_IC_Init(void);
void LCD_Init(void);
void LCD_Reinit(void);   /* sleep-in + re-init */
void LCD_SetAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_SetColor(uint32_t rbg888);
void LCD_SetBackColor(uint32_t rbg888);
void LCD_Clear(void);
void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

/* ---- API: vendor demo screens ---- */
void DispColor(uint32_t color);
void DispFrame(void);
void DispGrayHor16(void);
void DispBand(void);
void StopDelay(uint16_t ms);

/* ---- API: ASCII text ---- */
void LCD_SetAsciiFont(pFONT *font);
void LCD_ShowTransparent(uint8_t mode);
void LCD_DisplayChar(uint16_t x, uint16_t y, uint8_t c);
void LCD_DisplayString(uint16_t x, uint16_t y, char *p);

/* ---- API: 2D drawing ---- */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color);
void LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t height);
void LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t width);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r);

/* ---- API: raw buffer blit (RGB565 words) ---- */
void LCD_CopyBuffer(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                    const uint16_t *data);

#endif /* __LCD_H */
