/*
  touch.c - CHSC6417 capacitive touch over hardware I2C1 (PB8 = SCL,
  PB9 = SDA, AF4) for the nucleo-u575 TK0196M106 module port.

  PB8/PB9 are the I2C1 AF4 pins - the touch bus uses the hardware
  peripheral (100 kHz standard mode, TIMINGR computed for
  PCLK1 = 160 MHz; the vendor runs 400 kHz, which this timing can be
  raised to once the wiring is proven).

  Register pointer: the touch data block is read from register 0x00.
  Three independent vendor sources agree on that and on the byte layout;
  an earlier revision of this file used 0xE0, which is the odd one out
  and read back a constant (see the note above CTP_REG_DATA):

    - TK013F1327 vendor example (touch_CTP.c, FT6206_Read_Reg): writes
      pointer 0x00 then reads 3 bytes, X = ((b0&0x40)>>6)<<8 | b1,
      Y = ((b0&0x80)>>7)<<8 | b2.
    - espressif esp_lcd_touch_chsc6417: DATA_START_REG = 0x00 (its
      read_data() calls i2c_read_bytes(..., 0xE0, ...) - the two
      constants disagree inside that driver, and the header wins).
    - f4-demo jd9858_md130_360x360_chsc6417: bit-banged, pointer 0x00.

  Byte 0 = {[1:0] point count, bit 6 = X bit 8, bit 7 = Y bit 8},
  byte 1 = X[7:0], byte 2 = Y[7:0]. Chip ID at register 0xA7.
  7-bit slave address 0x2E.
*/
#include "touch.h"
#include "board.h"
#include "stm32u5xx_hal.h"
#include <string.h>

/* 7-bit slave address 0x2E; HAL wants the 8-bit address (0x5C). */
#define CTP_ADDR_7BIT   0x2EU
#define CTP_ADDR_8BIT   (CTP_ADDR_7BIT << 1)

#define CTP_REG_DATA    0x00U     /* touch data block start */
#define CTP_REG_CHIP_ID 0xA7U     /* chip id                */

/* I2C1 @ 100 kHz standard mode, PCLK1 = 160 MHz:
 * t_PRESC = (PRESC+1)/160 MHz = 100 ns with PRESC = 15;
 * SCLDEL = 4 (t = 500 ns >= t_SU;DAT 250 ns), SDADEL = 2 (t = 300 ns),
 * SCLH = SCLL = 49 (t = 50 * 100 ns = 5 us each -> 100 kHz). */
#define CTP_TIMINGR     0xF4203131U

static I2C_HandleTypeDef hi2c1;

void Touch_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    /* PB8 = I2C1_SCL, PB9 = I2C1_SDA (AF4, open-drain; the module
     * carries its own pull-ups, enable the internal ones as backup). */
    g.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    hi2c1.Instance             = I2C1;
    hi2c1.Init.Timing          = CTP_TIMINGR;
    hi2c1.Init.OwnAddress1     = 0U;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0U;
    hi2c1.Init.OwnAddress2Masks= I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* Probe the address byte and report the ACK, so a wiring/address
 * problem is distinguishable from "no finger down". */
uint8_t Touch_SelfTest(void)
{
    return (HAL_I2C_IsDeviceReady(&hi2c1, CTP_ADDR_8BIT, 2U, 100U)
            == HAL_OK) ? 1U : 0U;
}

/* Chip ID (register 0xA7), 0 on a bus error. */
uint8_t Touch_ChipId(void)
{
    uint8_t id = 0U;

    (void)HAL_I2C_Mem_Read(&hi2c1, CTP_ADDR_8BIT, CTP_REG_CHIP_ID,
                           I2C_MEMADD_SIZE_8BIT, &id, 1U, 100U);
    return id;
}

/* One combined bus probe for bring-up: does the part ACK on I2C, and what
 * does the data block actually look like? Returns 1 when the address ACKs
 * AND the data block is not the all-zero idle pattern. */
uint8_t Touch_Probe(uint8_t *buf, uint8_t len)
{
    memset(buf, 0, len);
    Touch_Read(buf, len);

    return ((Touch_SelfTest() != 0U) &&
            ((buf[0] | buf[1] | (uint8_t)((len > 2U) ? buf[2] : 0U)) != 0U))
           ? 1U : 0U;
}

/* Read len bytes starting at the touch data block (register 0x00,
 * auto-increment). On a bus error the buffer is zeroed (the caller then
 * sees "no touch"). */
void Touch_Read(uint8_t *buf, uint8_t len)
{
    memset(buf, 0, len);

    if (HAL_I2C_Mem_Read(&hi2c1, CTP_ADDR_8BIT, CTP_REG_DATA,
                         I2C_MEMADD_SIZE_8BIT, buf, len, 100U) != HAL_OK)
    {
        memset(buf, 0, len);
    }
}
