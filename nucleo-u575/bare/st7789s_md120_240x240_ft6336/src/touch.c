/*
  touch.c - FT6336 capacitive touch over hardware I2C1 (PB8 = SCL,
  PB9 = SDA, AF4) for the nucleo-u575 TK012F6 module port.

  Unlike the f411 CST816D port (whose PA2/PA3 pins had no hardware I2C,
  so that bus was bit-banged), PB8/PB9 are the I2C1 AF4 pins - so this
  touch bus uses the hardware peripheral (100 kHz standard mode,
  TIMINGR computed for PCLK1 = 160 MHz).

  Reads `len` bytes starting at register 0x00: 0x00 Device_Mode,
  0x01 GEST_ID, 0x02 TD_STATUS (number of touch points; 1 = valid),
  0x03 P1_XH ([3:0] = X high nibble), 0x04 P1_XL, 0x05 P1_YH
  ([3:0] = Y high nibble), 0x06 P1_YL. 7-bit slave address 0x38.
*/
#include "touch.h"
#include "board.h"
#include "stm32u5xx_hal.h"
#include <string.h>

/* 7-bit slave address 0x38; HAL wants the 8-bit address (0x70). */
#define CTP_ADDR_7BIT   0x38U
#define CTP_ADDR_8BIT   (CTP_ADDR_7BIT << 1)

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

/* Read len bytes starting at register 0x00 (the FT6336 touch data
 * block). On a bus error the buffer is zeroed (the caller then sees
 * "no touch"). */
void Touch_Read(uint8_t *buf, uint8_t len)
{
    memset(buf, 0, len);

    if (HAL_I2C_Mem_Read(&hi2c1, CTP_ADDR_8BIT, 0x00U,
                         I2C_MEMADD_SIZE_8BIT, buf, len, 100U) != HAL_OK)
    {
        memset(buf, 0, len);
    }
}
