/*
  interface.c - LCD bus primitives for the co5300_md196_368x448_chsc6417
  project (nucleo-u575, CO5300 1.96" 368x448 module). Identical bus to
  the nv3030b_md183_240x284_cst816d project (same three wires, same
  wrapped-command framing).

  PLAIN single-lane SPI. Two transports behind one byte API, selectable at
  runtime (QSPI_SetMode) so the panel can be brought up along a ladder:

    SOFT - bit-banged SPI on plain GPIOs, vendor TK499 sequence bit for
           bit: SCK falls, data is set, SCK rises (the panel samples on
           the rising edge). SCK is left high (mode-3 idle), so the next
           byte's first falling edge is a real edge.
    HW   - SPI1, 8-bit frames, mode 3 (CPOL=1/CPHA=2EDGE), MSB first,
           LCD_SPI_PRESC. CS stays a software GPIO in both cases.

  Wrapped command framing (vendor-verbatim): every transaction is one CS
  frame - CS high settle, CS low, then the 4-byte header
  `02 00 <cmd> 00`, then the parameter/pixel bytes, then CS high. The
  pixel bytes ride in the SAME frame as their command.

  Write-only by design: the driver never reads the panel and MISO is not
  connected. The CO5300 wrapped protocol needs no readback.

  Signals: CS = PA4, SCK = PA5 (SPI1_SCK AF5 on HW), MOSI = PA7
  (SPI1_MOSI AF5 on HW).
*/

#include <stdio.h>
#include "interface.h"
#include "board.h"

#define SPI_TIMEOUT 100U     /* ms per HAL SPI call */

static SPI_HandleTypeDef s_hspi;
static uint8_t s_mode = LCD_BUS_SOFT;

static uint32_t s_soft_khz;      /* measured soft SCK, kHz          */
static uint8_t  s_soft_cal;      /* soft SCK measured once          */
static uint8_t  s_spi_ready;     /* SPI1 initialized                */

/* ---- pin accessors. Single BSRR store: no read-modify-write. ---- */
#define LCD_CS_HI()    (GPIOA->BSRR = GPIO_PIN_4)
#define LCD_CS_LO()    (GPIOA->BSRR = (uint32_t)GPIO_PIN_4 << 16)
#define LCD_SCK_HI()   (GPIOA->BSRR = GPIO_PIN_5)
#define LCD_SCK_LO()   (GPIOA->BSRR = (uint32_t)GPIO_PIN_5 << 16)
#define LCD_MOSI_HI()  (GPIOA->BSRR = GPIO_PIN_7)
#define LCD_MOSI_LO()  (GPIOA->BSRR = (uint32_t)GPIO_PIN_7 << 16)

void QSPI_SetMode(uint8_t mode)
{
    if (mode <= LCD_BUS_HW)
    {
        s_mode = mode;
    }
}

uint8_t QSPI_GetMode(void)
{
    return s_mode;
}

/* ---------------- pin ownership: GPIO  <->  SPI1 --------------------- */

/* Reconfigure SCK/MOSI as plain GPIO outputs (soft path). CS is always a
 * GPIO output, so it is configured once for both paths. */
static void soft_pins_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pin   = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7;   /* CS, SCK, MOSI */
    HAL_GPIO_Init(GPIOA, &g);

    /* Mode-3 idle: CS high (deselected), SCK high, data low. */
    LCD_CS_HI();
    LCD_SCK_HI();
    LCD_MOSI_LO();
}

/* Configure SPI1 (idempotent) and hand SCK/MOSI to its AF. CS stays a
 * software GPIO. */
static void hw_pins_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* CS stays a plain output even on the hardware path. */
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pin   = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOA, &g);
    LCD_CS_HI();

    if (s_spi_ready == 0U)
    {
        __HAL_RCC_SPI1_CLK_ENABLE();

        s_hspi.Instance               = SPI1;
        s_hspi.Init.Mode              = SPI_MODE_MASTER;
        s_hspi.Init.Direction         = SPI_DIRECTION_1LINE;   /* TX only */
        s_hspi.Init.DataSize          = SPI_DATASIZE_8BIT;
        s_hspi.Init.CLKPolarity       = SPI_POLARITY_HIGH;     /* mode 3  */
        s_hspi.Init.CLKPhase          = SPI_PHASE_2EDGE;
        s_hspi.Init.NSS               = SPI_NSS_SOFT;
        s_hspi.Init.BaudRatePrescaler = LCD_SPI_PRESC;
        s_hspi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
        s_hspi.Init.TIMode            = SPI_TIMODE_DISABLE;
        s_hspi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
        s_hspi.Init.CRCPolynomial     = 7U;
        /* Keep the SCK/MOSI alternate functions driven while the
         * peripheral is disabled. HAL_SPI_Transmit() disables the SPI at
         * the end of every call (and re-enables it at the start of the
         * next), and without AFCNTR the new-generation SPI IP releases
         * the AF I/Os on disable. With CS already low that SCK edge is
         * latched by the panel as an extra bit and the whole stream
         * shifts by one, so the panel never initialises (all-white
         * display). Old-generation IP (L4/F7) has no such bit, which is
         * why this is U5-specific. */
        s_hspi.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
        if (HAL_SPI_Init(&s_hspi) != HAL_OK)
        {
            Error_Handler();
        }
        s_spi_ready = 1U;
    }

    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    /* HIGH slew: needed to toggle a 40 MHz SCK cleanly. VERY_HIGH edges
     * ring on flying wires (see the st7789s port notes). */
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF5_SPI1;                        /* PA5, PA7 */
    g.Pin       = GPIO_PIN_5 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &g);
}

/* ---------------- soft (bit-banged) SPI ------------------------------ */

/* One half SCK period. */
static void soft_spi_delay(void)
{
    volatile uint32_t n = (uint32_t)LCD_SOFT_SPI_DIV;

    while (n-- != 0U)
    {
        ;
    }
}

/* Shift one byte out, MSB first, mode 3 (vendor-verbatim sequence):
 * SCK falls, data is set, SCK rises (the panel samples here). SCK is
 * left high, the mode-3 idle level, so the next byte's first falling
 * edge is a real edge. */
static void soft_spi_byte(uint8_t b)
{
    uint8_t i;

    for (i = 0; i < 8U; i++)
    {
        LCD_SCK_LO();                       /* falling edge: panel shifts */
        if ((b & 0x80U) != 0U) { LCD_MOSI_HI(); } else { LCD_MOSI_LO(); }
        soft_spi_delay();
        LCD_SCK_HI();                       /* rising edge: panel samples */
        soft_spi_delay();
        b = (uint8_t)(b << 1);
    }
}

/* Measure the real SCK the bit-bang loop produces (whole bytes timed, so
 * the GPIO stores and loop overhead are counted, not just the delay).
 * SCK toggling with CS high is harmless, so this runs before any frame. */
static void soft_spi_calibrate(void)
{
    uint32_t t0, t1, ms;
    uint32_t n = 20000UL;
    uint32_t i;

    if (s_soft_cal != 0U)
    {
        return;
    }

    t0 = HAL_GetTick();
    for (i = 0; i < n; i++)
    {
        soft_spi_byte(0xAAU);
    }
    t1 = HAL_GetTick();

    ms = t1 - t0;
    if (ms == 0U)
    {
        ms = 1U;
    }

    /* bytes/s = n / (ms/1000); SCK = bytes/s * 8. */
    s_soft_khz = (uint32_t)((uint64_t)n * 8ULL * 1000ULL /
                            ((uint64_t)ms * 1000ULL));
    s_soft_cal = 1U;
}

/* One wrapped soft-SPI transaction: CS high settle, CS low, the 4-byte
 * header 02 00 <cmd> 00, then the data bytes, all inside the same CS
 * frame (vendor-verbatim). */
static void soft_xfer(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    LCD_CS_HI();
    for (i = 0; i < LCD_SOFT_CS_SETTLE; i++)
    {
        ;
    }
    LCD_CS_LO();

    soft_spi_byte(LCD_SPI_OPCODE);
    soft_spi_byte(0x00U);
    soft_spi_byte(cmd);
    soft_spi_byte(0x00U);

    for (i = 0; i < len; i++)
    {
        soft_spi_byte(data[i]);
    }

    LCD_CS_HI();
}

/* ---------------- hardware SPI1 -------------------------------------- */

/* One wrapped hardware transaction. The header and the data ride in the
 * SAME CS frame, so CS is lowered once and raised after the last byte:
 * HAL_SPI_Transmit() would toggle the peripheral between the two calls
 * and the panel would treat them as separate frames. */
static void hw_xfer(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    uint8_t hdr[4];

    hdr[0] = LCD_SPI_OPCODE;
    hdr[1] = 0x00U;
    hdr[2] = cmd;
    hdr[3] = 0x00U;

    LCD_CS_LO();

    if (HAL_SPI_Transmit(&s_hspi, hdr, 4U, SPI_TIMEOUT) != HAL_OK)
    {
        printf("[SPI] TX FAIL hdr cmd=%02X\r\n", (unsigned)cmd);
        LCD_CS_HI();
        return;
    }

    /* Data in chunks: one row is 368x2 = 736 bytes and HAL takes a
     * uint16_t Size. Chunking keeps the CS frame open across calls. */
    while (len != 0U)
    {
        uint16_t n = (len > 0xFFFFU) ? 0xFFFFU : (uint16_t)len;

        if (HAL_SPI_Transmit(&s_hspi, (const uint8_t *)data, n,
                             SPI_TIMEOUT) != HAL_OK)
        {
            printf("[SPI] TX FAIL data cmd=%02X len=%lu\r\n",
                   (unsigned)cmd, (unsigned long)len);
            break;
        }
        data += n;
        len  -= n;
    }

    LCD_CS_HI();
}

/* Drain the SPI1 TX path: wait for TXC (transmit complete, both the FIFO
 * and the shift register empty) and clear the RX overrun. Kept for
 * bring-up diagnostics, not on the transfer path. */
static void hw_drain(void)
{
    uint32_t guard = 100000U;

    while (((SPI1->SR & SPI_SR_TXC) == 0U) && (guard-- != 0U))
    {
        ;
    }
    (void)SPI1->RXDR;
}

/* ---------------- public API ----------------------------------------- */

void QSPI_Write(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    if (s_mode == LCD_BUS_SOFT)
    {
        soft_pins_init();
        soft_xfer(cmd, data, len);
    }
    else
    {
        hw_pins_init();
        hw_xfer(cmd, data, len);
    }
}

/* Pixel burst. Identical to a parameter write here: the whole point of
 * the wrapped framing is that pixels ride in the same CS frame as their
 * command, on the same single data line. Kept as a separate entry point
 * so lcd.c stays explicit about which transfers are pixel data. */
void QSPI_WritePixel(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    QSPI_Write(cmd, data, len);
}

void QSPI_Cmd(uint8_t cmd)
{
    QSPI_Write(cmd, NULL, 0U);
}

void QSPI_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    if (s_mode == LCD_BUS_SOFT)
    {
        soft_pins_init();
        soft_spi_calibrate();
    }
    else
    {
        hw_pins_init();
    }
}

/* Active SCK in kHz, e.g. 40000 = 40 MHz (for console/info prints). */
unsigned long QSPI_KHz(void)
{
    if (s_mode == LCD_BUS_SOFT)
    {
        return (unsigned long)s_soft_khz;
    }

    /* SPI1 is on APB2 (= 160 MHz here); the prescaler field holds a
     * two-level token, so decode it. */
    {
        uint32_t div = 2U << ((s_hspi.Init.BaudRatePrescaler & SPI_CFG1_MBR) >>
                              SPI_CFG1_MBR_Pos);
        return (unsigned long)(HAL_RCC_GetPCLK2Freq() / div / 1000U);
    }
}
