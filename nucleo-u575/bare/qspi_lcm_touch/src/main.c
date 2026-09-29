/*
  qspi_lcm_touch main for the nucleo-u575 (STM32U575ZIT6 @ 160 MHz).
  TK018F3716 module: NV3030B 1.83" 240x284 panel over the wrapped-
  command QSPI protocol on OCTOSPI1 (hardware NCS, mode 3, 40 MHz SCK),
  plus CST816D capacitive touch over hardware I2C1 (PB8/PB9), with the
  touch state printed on the serial console. Ported from the ch32v307
  reference nv3030b_md183_240x284_cst816d (same module; that port used
  soft/hard single-lane SPI, only the hardware path is carried over).

  "Quad - Dual - Single" dataline loop: the command (instruction +
  address) phase is always single-line; the data phase lane count is
  switched per pass 4 -> 2 -> 1 via the wrapped-command instruction
  (0x22 / 0x12 / 0x02), and the full pattern set runs on each mode.

  Demo phases per pass (live FPS counter throughout): checkerboard
  stress, TEST_STAND vendor screens (timed solid fills), info pages
  (normal + inverted), HSV gradient sweep, LED test.

  Wiring: OCTOSPI1 NCS=PA2 CLK=PB10 IO0=PE12 IO1=PB0 IO2=PE14 IO3=PE15
  (all AF10). No DC/reset/backlight pin (wrapped protocol, backlight
  hardwired on-module). Touch: CST816D I2C1 SCL=PB8 SDA=PB9.
*/

#include <string.h>
#include <stdio.h>
#include "board.h"
#include "lcd.h"
#include "interface.h"
#include "touch.h"
#include "lcd_font_1608.h"

/* ---- on-board LEDs (LD1 green PC7, LD2 blue PB7, LD3 red PG2, high
 * active) - driven through the board layer ---- */
#define LED_ON()   do { LED1_ON();  LED2_ON();  LED3_ON();  } while (0)
#define LED_OFF()  do { LED1_OFF(); LED2_OFF(); LED3_OFF(); } while (0)

#define SCREEN_W   LCD_Width     /* 240 (row buffer sizing) */
#define FPS_BAND   20            /* bottom rows reserved for the FPS text   */
#define BACK_COLOR LCD_BLACK
#define LED_HALF   1000          /* LED test dwell (ms)                    */

/* Info page layout. The big 8x16 font needs a 24 px line pitch (16 px
 * glyph + spacing). The panel has rounded corners, so the block is
 * indented (INFO_X) and starts below row 0 (INFO_TOP). */
#define INFO_DY    24            /* info page line pitch (8x16 font)       */
#define INFO_X     22            /* left indent: clears the corner radius  */
#define INFO_TOP   18            /* first text row (skip row 0)            */

/* --------------------------------------------------------------------- */
/* Touch printout: polls the CST816D and prints state/X/Y on the serial
 * port (on touch-down, and on release). Reading 8 bytes from register 0
 * gives: buf[3] = 0x80 marks an active touch, buf[4] = X, buf[5/6] =
 * Y ([3:0] high nibble, [7:0] low). */
static uint8_t s_touch_down;

static void touch_task(void)
{
    uint8_t buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    uint16_t x, y;

    Touch_Read(buf, 8);

    if (buf[3] == 0x80U && buf[4] > 1U)
    {
        x = buf[4];
        y = (uint16_t)(((buf[5] & 0x0FU) << 8) | buf[6]);

        if (s_touch_down == 0U)
        {
            printf("[TOUCH] down X=%u Y=%u (284-Y=%u)\r\n",
                   (unsigned)x, (unsigned)y, (unsigned)(284U - y));
            s_touch_down = 1U;
        }
    }
    else if (s_touch_down != 0U)
    {
        printf("[TOUCH] release\r\n");
        s_touch_down = 0U;
    }
}

/* Runtime window geometry (follows LCD_SetWindow). */
static uint16_t anim_h(void)
{
    return (uint16_t)(LCD_H() - FPS_BAND);
}

/* --------------------------------------------------------------------- */
/* FPS counter.                                                          */
static volatile uint32_t g_frames;
static uint32_t         g_last_frames;
static uint32_t         g_fps_last_tick;
static uint32_t         g_fps_color = LCD_WHITE;   /* FPS glyph color     */

static void fps_frame(void)
{
    g_frames++;
}

static void fps_update(void)
{
    uint32_t now = HAL_GetTick();
    if (now - g_fps_last_tick >= 1000)
    {
        uint32_t fps = g_frames - g_last_frames;
        g_last_frames = g_frames;
        g_fps_last_tick = now;

        char buf[8];
        buf[0] = 'F'; buf[1] = 'P'; buf[2] = 'S'; buf[3] = ':';
        buf[4] = (char)('0' + (fps / 100) % 10);
        buf[5] = (char)('0' + (fps / 10) % 10);
        buf[6] = (char)('0' + fps % 10);
        buf[7] = '\0';
        LCD_SetColor(g_fps_color);
        LCD_ShowTransparent(1);              /* no opaque box */
        LCD_DisplayString(1, (uint16_t)(anim_h() + 4), buf);
        LCD_ShowTransparent(0);
    }
}

static void paint_fps_band(void)
{
    LCD_SetColor(BACK_COLOR);
    LCD_SetBackColor(BACK_COLOR);
    LCD_FillRect(0, anim_h(), LCD_W(), FPS_BAND);
}

static void delay_with_fps(uint32_t ms)
{
    uint32_t start = HAL_GetTick();
    do
    {
        fps_update();
        touch_task();                        /* touch printout during waits */
        HAL_Delay(50);
    } while (HAL_GetTick() - start < ms);
}

/* --------------------------------------------------------------------- */
/* Animated gradient: hue sweeps the full color wheel over `ms`.         */
static uint32_t hsv_to_rgb(int h, int s, int v)
{
    int region = (h / 600) % 6;
    int fpart  = h % 600;
    int p = v * (255 - s) / 255;
    int q = v * (255 - (s * fpart) / 600) / 255;
    int t = v * (255 - (s * (600 - fpart)) / 600) / 255;
    int r, g, b;
    switch (region)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default:r = v; g = p; b = q; break;
    }
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void draw_gradient(int hue_a, int hue_b, uint16_t *row)
{
    int y;
    for (y = 0; y < (int)anim_h(); y++)
    {
        int frac = y * 1000 / (int)anim_h();
        int hue  = hue_a + (hue_b - hue_a) * frac / 1000;
        uint32_t c = hsv_to_rgb(hue, 255, 255);
        uint16_t rgb565 = (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) |
                                     ((c >> 3) & 0x001F));
        int x;
        for (x = 0; x < (int)LCD_W(); x++)
        {
            row[x] = rgb565;
        }
        LCD_CopyBuffer(0, (uint16_t)y, LCD_W(), 1, row);
    }
}

static void gradient_demo(uint32_t ms)
{
    static uint16_t row[SCREEN_W];
    LCD_SetBackColor(BACK_COLOR);
    paint_fps_band();

    uint32_t start = HAL_GetTick();
    uint32_t t = 0;
    do
    {
        int hue_a = (int)(t * 3600 / ms);
        int hue_b = hue_a + 1800;
        if (hue_b >= 3600) { hue_b -= 3600; }
        draw_gradient(hue_a, hue_b, row);
        fps_frame();
        fps_update();
        t = HAL_GetTick() - start;
    } while (t < ms);
}

/* --------------------------------------------------------------------- */
/* LED test (the three on-board LEDs).                                   */
static void led_test(void)
{
    printf("[LCD] LED ON\r\n");
    LED_ON();
    delay_with_fps(LED_HALF);
    printf("[LCD] LED OFF\r\n");
    LED_OFF();
    delay_with_fps(LED_HALF);
}

/* --------------------------------------------------------------------- */
/* Vendor TEST_STAND screens. The five solid-color fills are timed (ms). */
static uint32_t g_solid_ms[5];
static const char *const g_solid_name[5] =
{
    "RED", "GREEN", "BLUE", "WHITE", "BLACK"
};

/* kHz -> "40 MHz" / "1.8 MHz" text (shared by console + info page). */
static const char *mhz_text(unsigned long khz)
{
    static char t[16];
    if (khz % 1000UL == 0UL)
    {
        snprintf(t, sizeof t, "%lu MHz", khz / 1000UL);
    }
    else
    {
        snprintf(t, sizeof t, "%lu.%lu MHz",
                 khz / 1000UL, (khz % 1000UL) / 100UL);
    }
    return t;
}

static void TEST_STAND(void)
{
    const uint32_t solid_color[5] = { C565_RED, C565_GREEN, C565_BLUE, C565_WHITE, C565_BLACK };
    int i;

    DispFrame();
    StopDelay(Delay_Time);

    DispGrayHor16();
    StopDelay(Delay_Time);

    DispBand();
    StopDelay(Delay_Time);

    for (i = 0; i < 5; i++)
    {
        uint32_t t0 = HAL_GetTick();
        DispColor(solid_color[i]);
        g_solid_ms[i] = HAL_GetTick() - t0;
        StopDelay(Delay_Time);
    }

    printf("[LCD] solid fills (ms): RED=%lu GREEN=%lu BLUE=%lu "
           "WHITE=%lu BLACK=%lu\r\n",
           (unsigned long)g_solid_ms[0], (unsigned long)g_solid_ms[1],
           (unsigned long)g_solid_ms[2], (unsigned long)g_solid_ms[3],
           (unsigned long)g_solid_ms[4]);
}

/* --------------------------------------------------------------------- */
/* Info page: compiler, clock rates, data lane mode and the solid-fill
 * durations, in the big 8x16 font (same as the banner page). `invert`
 * swaps fg/bg (white background page). */
static void info_demo(uint32_t ms, uint8_t invert)
{
    char buf[40];
    char comp[24];
    unsigned long mhz = (unsigned long)(SystemCoreClock / 1000000UL);
    uint32_t fg = invert ? LCD_BLACK : LCD_WHITE;
    uint32_t bg = invert ? LCD_WHITE : LCD_BLACK;
    int i, y;

#if defined(__GNUC__)
    snprintf(comp, sizeof comp, "GCC %d.%d.%d",
             __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    snprintf(comp, sizeof comp, "unknown");
#endif

    printf("[LCD] info%s: compiler=%s build=%s %s\r\n",
           invert ? " (inverted)" : "", comp, __DATE__, __TIME__);
    printf("[LCD] info: freq=%lu MHz spi=%s %s\r\n",
           mhz, mhz_text(QSPI_KHz()), (QSPI_GetMode() == LCD_BUS_SOFT)
                                       ? "soft" : "hw");
    printf("[LCD] info: solids(R,G,B,W,K)=%lu,%lu,%lu,%lu,%lu ms\r\n",
           (unsigned long)g_solid_ms[0], (unsigned long)g_solid_ms[1],
           (unsigned long)g_solid_ms[2], (unsigned long)g_solid_ms[3],
           (unsigned long)g_solid_ms[4]);

    /* Big font for the whole page (matches the banner page). */
    LCD_SetAsciiFont(&ASCII_Font16);
    LCD_SetColor(fg);
    LCD_SetBackColor(bg);
    LCD_Clear();

    /* FPS band in the page background color, then restore fg/bg. */
    LCD_SetColor(bg);
    LCD_SetBackColor(bg);
    LCD_FillRect(0, anim_h(), LCD_W(), FPS_BAND);
    LCD_SetColor(fg);
    LCD_SetBackColor(bg);
    g_fps_color = fg;                     /* FPS glyph matches the page */

    /* 8 px per glyph in the 8x16 font. */
    y = INFO_TOP;

    snprintf(buf, sizeof buf, "%s", comp);
    LCD_DisplayString((uint16_t)INFO_X, (uint16_t)y, buf);  y += INFO_DY;

    snprintf(buf, sizeof buf, "CPU %lu MHz", mhz);
    LCD_DisplayString((uint16_t)INFO_X, (uint16_t)y, buf);  y += INFO_DY;

    snprintf(buf, sizeof buf, "SPI %s", mhz_text(QSPI_KHz()));
    LCD_DisplayString((uint16_t)INFO_X, (uint16_t)y, buf);  y += INFO_DY;

    snprintf(buf, sizeof buf, "%s", mhz_text(QSPI_KHz()));
    LCD_DisplayString((uint16_t)INFO_X, (uint16_t)y, buf);  y += INFO_DY;

    snprintf(buf, sizeof buf, "%s", (QSPI_GetMode() == LCD_BUS_SOFT)
                                    ? "soft SPI" : "hw SPI");
    LCD_DisplayString((uint16_t)INFO_X, (uint16_t)y, buf);  y += INFO_DY;

    /* Fill durations, measured earlier this pass so they always reflect
     * the current lane mode. */
    for (i = 0; i < 5; i += 2)
    {
        if (i + 1 < 5)
        {
            snprintf(buf, sizeof buf, "%s %lu  %s %lu",
                     g_solid_name[i], (unsigned long)g_solid_ms[i],
                     g_solid_name[i + 1], (unsigned long)g_solid_ms[i + 1]);
        }
        else
        {
            snprintf(buf, sizeof buf, "%s %lu",
                     g_solid_name[i], (unsigned long)g_solid_ms[i]);
        }
        LCD_DisplayString((uint16_t)INFO_X, (uint16_t)y, buf);  y += INFO_DY;
    }

    g_fps_color = LCD_WHITE;              /* restore default FPS glyph color */

    delay_with_fps(ms);

    LCD_SetAsciiFont(&ASCII_Font12);      /* back to the normal font      */
}

/* --------------------------------------------------------------------- */
/* Big-font banner page (8x16 font), lines centered on the panel width.  */
static void banner_page(const char *l1, const char *l2,
                        uint32_t fg, uint32_t bg, uint32_t ms)
{
    g_fps_color = fg;

    LCD_SetAsciiFont(&ASCII_Font16);      /* bigger than the normal 6x12  */
    LCD_SetColor(fg);
    LCD_SetBackColor(bg);
    LCD_Clear();
    LCD_SetColor(bg);
    LCD_SetBackColor(bg);
    LCD_FillRect(0, anim_h(), LCD_W(), FPS_BAND);
    LCD_SetColor(fg);
    LCD_SetBackColor(bg);

    /* 8 px/glyph: center each line dynamically. */
    LCD_DisplayString((uint16_t)((LCD_W() - (int)strlen(l1) * 8) / 2), 40,
                      (char *)l1);
    LCD_DisplayString((uint16_t)((LCD_W() - (int)strlen(l2) * 8) / 2), 64,
                      (char *)l2);

    /* Touch polling keeps running during the banner dwell. */
    {
        uint32_t start = HAL_GetTick();
        while (HAL_GetTick() - start < ms)
        {
            touch_task();
            HAL_Delay(20);
        }
    }

    LCD_SetAsciiFont(&ASCII_Font12);      /* back to the normal font      */
    g_fps_color = LCD_WHITE;
}

/* --------------------------------------------------------------------- */
/* Checkerboard stress pattern - the sensitive test the solid-fill
 * screens are missing: it alternates every single pixel, so any bit
 * error is a broken cell, and every pixel goes out through the address-
 * window setup, so a framing slip shows as a shifted row. */
static void stress_pattern(void)
{
    static uint16_t row[SCREEN_W];
    int y, x;

    LCD_SetColor(LCD_WHITE);
    LCD_SetBackColor(BACK_COLOR);
    paint_fps_band();

    for (y = 0; y < (int)anim_h(); y++)
    {
        for (x = 0; x < (int)LCD_W(); x++)
        {
            /* 1-pixel checkerboard: adjacent pixels are opposite, so every
             * data bit toggles on every pixel. */
            row[x] = ((x ^ y) & 1) ? C565_WHITE : C565_BLACK;
        }
        LCD_CopyBuffer(0, (uint16_t)y, LCD_W(), 1, row);
    }
}

static void run_patterns(void)
{
    /* Sensitive pattern FIRST, right after init: clean checkerboard =
     * the init landed and the lane mode is usable; malformed = it did
     * not (or the link is marginal). */
    printf("[LCD] phase: STRESS (checkerboard @ %s)\r\n",
           mhz_text(QSPI_KHz()));
    stress_pattern();
    delay_with_fps(3000);

    printf("[LCD] phase: TEST_STAND\r\n");
    memset(g_solid_ms, 0, sizeof g_solid_ms);
    TEST_STAND();

    printf("[LCD] phase: info\r\n");
    info_demo(5000, 0);

    printf("[LCD] phase: info (inverted colors)\r\n");
    info_demo(5000, 1);

    printf("[LCD] phase: gradient\r\n");
    gradient_demo(4000);

    printf("[LCD] phase: LED test\r\n");
    led_test();
}

/* --------------------------------------------------------------------- */
static const char *mode_name(uint8_t m)
{
    return (m == LCD_BUS_SOFT) ? "SOFT (bit-banged)" : "HW (SPI1)";
}

/* One full bring-up + pattern pass on the currently selected bus mode. */
static void bus_pass(uint8_t mode, const char *title)
{
    QSPI_SetMode(mode);

    printf("\r\n==== bus: %s ====\r\n", title);

    /* Bring the panel up from scratch on this transport. */
    LCD_Reinit();

    printf("[LCD] SCK = %s\r\n", mhz_text(QSPI_KHz()));

    banner_page("NV3030B", mode_name(mode), LCD_BLACK, LCD_CYAN, 3000);

    printf("[LCD] running patterns on %s @ %s\r\n",
           mode_name(mode), mhz_text(QSPI_KHz()));
    run_patterns();
}

int main(void)
{
    /* Soft/Hard SPI loop. Both transports drive the same three wires
     * (CS=PA4, SCK=PA5, MOSI=PA7) with the same wrapped-command framing,
     * so a difference between the passes is a difference in the SPI
     * engine itself (bit-bang vs SPI1). */
    uint8_t st_buf[8];
    unsigned pass = 0U;
    int i;

    HAL_Init();
    Board_Init();          /* clocks (160 MHz), LEDs, button, console */

    printf("\r\n==== nucleo-u575 (STM32U575ZIT6) qspi_lcm_touch @ %lu MHz ====\r\n",
           (unsigned long)(SystemCoreClock / 1000000UL));
    printf("NV3030B 1.83\" 240x284 (wrapped-command SPI, MADCTL 0x08):\r\n");
    printf("CS=PA4 SCK=PA5 MOSI=PA7 (MISO not connected - write-only)\r\n");
    printf("loop: SOFT (bit-bang) <-> HW (SPI1), same 3 wires\r\n");
    printf("TOUCH: CST816D I2C1 SCL=PB8 SDA=PB9\r\n");

    Touch_Init();

    QSPI_SetMode(LCD_BUS_SOFT);   /* start on the bit-bang baseline */
    QSPI_Init();

    LCD_Init();            /* power settle + vendor init + clear */
    LCD_SetAsciiFont(&ASCII_Font12);
    paint_fps_band();

    printf("[SPI] soft SPI (bit-banged) SCK=%lu kHz, div=%u\r\n",
           QSPI_KHz(), (unsigned)LCD_SOFT_SPI_DIV);

    /* Bring-up diagnostic: does the CST816D answer at all? */
    printf("[TOUCH] self-test: %s\r\n",
           (Touch_SelfTest() != 0U) ? "ACK (chip present)"
                                    : "NO ACK (check wiring/addr)");
    Touch_Read(st_buf, 8);
    printf("[TOUCH] id regs:");
    for (i = 0; i < 8; i++)
    {
        printf(" %02X", st_buf[i]);
    }
    printf("\r\n");

    while (1)
    {
        /* Alternate the two transports; each gets a full bring-up from
         * scratch so neither inherits the other's panel state. */
        bus_pass(LCD_BUS_SOFT, "SOFT bit-banged SPI");
        bus_pass(LCD_BUS_HW,   "HW SPI1 peripheral");
        pass++;
    }

    return 0;
}
