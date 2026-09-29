/*
  co5300_md196_368x448_chsc6417 main for the nucleo-u575
  (STM32U575ZIT6 @ 160 MHz). TK0196M106 module: CO5300 1.96" 368x448
  panel over the wrapped-command single-lane SPI protocol (SOFT
  bit-bang <-> HW SPI1 passes on the same three wires), plus CHSC6417
  capacitive touch over hardware I2C1 (PB8/PB9), with the touch state
  printed on the serial console. Behavior copies
  nv3030b_md183_240x284_cst816d; vendor references: the TK499
  TK0196M106 single-lane example (display, vendor verbatim) and the
  vendor ESP32 QSPI example's CHSC6417 touch component (touch).

  Demo phases per pass (live FPS counter throughout): checkerboard
  stress, TEST_STAND vendor screens (timed solid fills), info pages
  (normal + inverted), HSV gradient sweep, LED test.

  Wiring: CS=PA4 SCK=PA5 MOSI=PA7 (MISO not connected - write-only).
  No DC/reset/backlight pin (wrapped protocol; brightness is command
  51h). Touch: CHSC6417 I2C1 SCL=PB8 SDA=PB9 (addr 0x2E).
*/

#include <string.h>
#include <stdio.h>
#include "board.h"
#include "lcd.h"
#include "interface.h"
#include "touch.h"
#include "lcd_font_1608.h"
#include "asset_test1.h"

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
/* Touch printout: polls the CHSC6417 and prints state/X/Y on the serial
 * port (on touch-down, move, and release). Reads from register 0x00:
 * bits [2:0] = touch-point count (0 = no touch, 1..5 valid), bit 6 =
 * X bit 8, bit 7 = Y bit 8; a[1] = X[7:0], a[2] = Y[7:0].
 *
 * Noise hardening (ported from the working f4-demo
 * jd9858_md130_360x360_chsc6417 project): each poll takes two reads and
 * only accepts them when they agree, then debounces - 3 consecutive
 * valid reads to register a touch, 6 consecutive idle reads to register
 * a release. Single reads on a fast display bus occasionally return junk;
 * without the double-read a wedged/glitching bus reads as a finger held
 * on the panel forever. */
static uint8_t s_touch_down;
static uint8_t s_down_cnt, s_up_cnt;

static void touch_task(void)
{
    uint8_t a[4], b[4];
    uint16_t x, y;
    uint8_t status;

    Touch_Read(a, 4);
    Touch_Read(b, 4);
    if (memcmp(a, b, sizeof a) != 0)
    {
        return;                          /* reads disagree: bus glitch */
    }

    status = (uint8_t)(a[0] & 0x07U);
    x = (uint16_t)((uint16_t)((a[0] & 0x40U) >> 6) << 8) | a[1];
    y = (uint16_t)((uint16_t)((a[0] & 0x80U) >> 7) << 8) | a[2];

    if ((status != 0U) && (status <= 5U))
    {
        if (s_down_cnt < 255U) { s_down_cnt++; }
        s_up_cnt = 0U;

        if ((s_touch_down == 0U) && (s_down_cnt >= 3U))
        {
            printf("[TOUCH] down X=%u Y=%u (448-Y=%u, points=%u)\r\n",
                   (unsigned)x, (unsigned)y, (unsigned)(448U - y),
                   (unsigned)status);
            s_touch_down = 1U;
        }
    }
    else
    {
        if (s_up_cnt < 255U) { s_up_cnt++; }
        s_down_cnt = 0U;

        if ((s_touch_down != 0U) && (s_up_cnt >= 6U))
        {
            printf("[TOUCH] release\r\n");
            s_touch_down = 0U;
        }
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

/* Cold-boot panel bring-up, ported from the ch32v307 reference.
 *
 * How long a cold-booted panel needs before it accepts commands varies
 * run to run and cannot be measured (the module is write-only by
 * design). Rather than sleeping or counting retries, this fills the
 * screen with a tiled asset: the drawing IS the wait, and it doubles as
 * the "the panel came up" test - the moment the artwork appears, the
 * panel is live. Each cycle interleaves a fill with an LCD_Reinit(), so
 * every init gets a chance; a late-starting panel catches one of them.
 *
 * The final fill is deliberate: it happens after the last Reinit, so the
 * image left on screen was sent with the most recent init in effect. */
/* Draw/re-init cycles. This is the bring-up time lever: each cycle costs
 * one fill (~87 ms on HW SPI1, ~1.4 s on the bit-bang) plus an LCD_Reinit
 * (~250 ms of datasheet delays), and the re-inits are what give a
 * slow-starting panel repeated chances to catch an init.
 *
 * 3 is the conservative value carried over from the reference. The panel
 * on this board reliably comes up on the first init (LCD_Init() already
 * did one before the bring-up runs), so 2 is the default here and 1 is
 * usually enough - see the bring-up notes in README.md. Raise it back to
 * 3 if a cold power-on ever shows a blank panel:
 *
 *   bash build.sh -DLCD_BRINGUP_PASSES=3 */
#ifndef LCD_BRINGUP_PASSES
#define LCD_BRINGUP_PASSES  2u
#endif
#define BRINGUP_TILE_GAP    4    /* gap between tiles, px (keeps a border) */

/* Fill y0..y1 with the 64x64 asset tiled on a gap-spaced grid, centred.
 * Each tile is whole, so the panel's rounded corners never clip one. */
static void bringup_fill_range(uint16_t y0, uint16_t y1)
{
    int span_h = (int)y1 - (int)y0;
    int cols, rows, used_w, used_h, x_off, y_off, r, c;

    if ((span_h < ASSET_TEST1_H) || ((int)LCD_W() < ASSET_TEST1_W))
    {
        return;                          /* no room for even one tile */
    }

    cols = ((int)LCD_W() - BRINGUP_TILE_GAP) / (ASSET_TEST1_W + BRINGUP_TILE_GAP);
    rows = (span_h      - BRINGUP_TILE_GAP) / (ASSET_TEST1_H + BRINGUP_TILE_GAP);
    if ((cols <= 0) || (rows <= 0))
    {
        return;
    }

    used_w = cols * ASSET_TEST1_W + (cols - 1) * BRINGUP_TILE_GAP;
    used_h = rows * ASSET_TEST1_H + (rows - 1) * BRINGUP_TILE_GAP;
    x_off  = ((int)LCD_W() - used_w) / 2;
    y_off  = (int)y0 + (span_h - used_h) / 2;

    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < cols; c++)
        {
            LCD_CopyBuffer((uint16_t)(x_off + c * (ASSET_TEST1_W + BRINGUP_TILE_GAP)),
                           (uint16_t)(y_off + r * (ASSET_TEST1_H + BRINGUP_TILE_GAP)),
                           (uint16_t)ASSET_TEST1_W, (uint16_t)ASSET_TEST1_H,
                           asset_test1);
        }
    }
}

static void panel_bringup(void)
{
    uint32_t pass;

    LCD_Clear();

    for (pass = 0; pass < LCD_BRINGUP_PASSES; pass++)
    {
        printf("[LCD] bring-up %lu/%lu: draw\r\n",
               (unsigned long)(pass + 1U), (unsigned long)LCD_BRINGUP_PASSES);
        bringup_fill_range(INFO_TOP, anim_h());

        printf("[LCD] bring-up: Reinit\r\n");
        LCD_Reinit();
    }

    /* Last fill with the most recent init in effect. */
    bringup_fill_range(INFO_TOP, anim_h());
    printf("[LCD] bring-up: done\r\n");
}

/* One full bring-up + pattern pass on the currently selected bus mode. */
static void bus_pass(uint8_t mode, const char *title)
{
    QSPI_SetMode(mode);

    printf("\r\n==== bus: %s ====\r\n", title);

    /* Bring the panel up from scratch on this transport. */
    LCD_Reinit();

    printf("[LCD] SCK = %s\r\n", mhz_text(QSPI_KHz()));

    /* The cold-boot wait: fill + reinit cycles instead of a delay. */
    panel_bringup();

    LCD_Reinit();         /* re-frame the panel */
    banner_page("CO5300", mode_name(mode), LCD_BLACK, LCD_CYAN, 3000);

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
    uint8_t st_buf[3];
    unsigned pass = 0U;

    HAL_Init();
    Board_Init();          /* clocks (160 MHz), LEDs, button, console */

    printf("\r\n==== nucleo-u575 (STM32U575ZIT6) co5300_md196_368x448_chsc6417 @ %lu MHz ====\r\n",
           (unsigned long)(SystemCoreClock / 1000000UL));
    printf("CO5300 1.96\" 368x448 (wrapped-command SPI):\r\n");
    printf("CS=PA4 SCK=PA5 MOSI=PA7 (MISO not connected - write-only)\r\n");
    printf("loop: SOFT (bit-bang) <-> HW (SPI1), same 3 wires\r\n");
    printf("TOUCH: CHSC6417 I2C1 SCL=PB8 SDA=PB9 (addr 0x2E)\r\n");

    Touch_Init();

    QSPI_SetMode(LCD_BUS_SOFT);   /* start on the bit-bang baseline */
    QSPI_Init();

    LCD_Init();            /* power settle + vendor init + clear */
    LCD_SetAsciiFont(&ASCII_Font12);
    paint_fps_band();

    printf("[SPI] soft SPI (bit-banged) SCK=%lu kHz, div=%u\r\n",
           QSPI_KHz(), (unsigned)LCD_SOFT_SPI_DIV);

    /* Bring-up diagnostic: does the CHSC6417 answer at all? */
    printf("[TOUCH] self-test: %s\r\n",
           (Touch_SelfTest() != 0U) ? "ACK (chip present)"
                                    : "NO ACK (check wiring/addr)");
    printf("[TOUCH] chip id (0xA7): %02X\r\n", (unsigned)Touch_ChipId());
    printf("[TOUCH] probe: %s\r\n",
           (Touch_Probe(st_buf, 3) != 0U) ? "data block live"
                                          : "no usable data");
    printf("[TOUCH] data regs (0x00):");
    for (int i = 0; i < 3; i++)
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
