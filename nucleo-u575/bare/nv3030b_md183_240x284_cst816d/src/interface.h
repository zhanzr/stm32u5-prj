/*
  interface.h - LCD bus primitives for the nv3030b_md183_240x284_cst816d project
  (nucleo-u575, NV3030B 1.83" 240x284 module).

  PLAIN single-lane SPI on three wires - the OCTOSPI/QuadSPI path is gone.
  The panel is driven the way the vendor SPI examples do it: every
  transaction is ONE chip-select frame carrying an 8-bit wrapped command:

      CS low
      byte 0x02            (transfer opcode: 1 data line)
      byte 0x00
      byte <cmd>           (the register/RAM command)
      byte 0x00
      ... parameter / pixel bytes ...
      CS high

  The 0x02 prefix IS the protocol header (the vendor's WriteComm sends
  exactly `02 00 <cmd> 00`); it is not an OCTOSPI instruction phase. Keeping
  the pixel bytes in the same CS frame as their command is what lets a row
  be streamed without re-sending the header.

  TWO transports, selected at runtime (QSPI_SetMode) so the panel can be
  brought up along a ladder:

    SOFT - bit-banged SPI on plain GPIOs (CS/SCK/MOSI). The vendor TK499
           sequence, bit for bit (see interface.c). The forgiving link.
    HW   - the SPI1 peripheral, 8-bit frames, mode 3, MSB first.

  Signals - the same three pins for both transports:
    CS   = PA4  (driven by software; SPI1_NSS is NOT used)
    SCK  = PA5  (GPIO on SOFT; SPI1_SCK,  AF5 on HW)
    MOSI = PA7  (GPIO on SOFT; SPI1_MOSI, AF5 on HW)
  MISO - NOT CONNECTED. The driver is write-only by design: the NV3030B
  wrapped protocol needs no readback (no status polling, no RAM read).

  Touch: CST816D on hardware I2C1, SCL=PB8 / SDA=PB9 (unchanged).
*/

#ifndef __INTERFACE_H
#define __INTERFACE_H

#include <stdint.h>
#include "stm32u5xx_hal.h"

/* Bus modes for QSPI_SetMode(). */
#define LCD_BUS_SOFT   0U  /* bit-banged GPIO SPI                */
#define LCD_BUS_HW     1U  /* SPI1 peripheral                    */

/* SPI1 baud prescaler (SPI1 is on APB2 = 160 MHz, /1). SPI1's kernel
 * clock here is PCLK2, so /2 = 80 MHz, /4 = 40 MHz (default), /8 = 20 MHz.
 * The NV3030B allows up to 64.9 MHz on SCL (15.4 ns write cycle) at
 * 3.3 V, but flying wires will not - drop to /8 if the checkerboard
 * stress pattern shows noise. */
#ifndef LCD_SPI_PRESC
#define LCD_SPI_PRESC SPI_BAUDRATEPRESCALER_4
#endif

/* The wrapped-command header byte. 0x02 is the vendor's 1-data-line
 * opcode; every SPI example for this module uses it. */
#ifndef LCD_SPI_OPCODE
#define LCD_SPI_OPCODE 0x02U
#endif

/* Soft-SPI half-bit delay, in loop iterations (vendor TK499 default 4).
 * Raise to slow the bit-bang down; the achieved SCK is measured and
 * printed at init. */
#ifndef LCD_SOFT_SPI_DIV
#define LCD_SOFT_SPI_DIV 4U
#endif

/* Soft-SPI CS-high settle, in loop iterations. The vendor waits ~5 us
 * between frames (CS high, then low) before the next header. */
#ifndef LCD_SOFT_CS_SETTLE
#define LCD_SOFT_CS_SETTLE 200U
#endif

void          QSPI_Init(void);
void          QSPI_SetMode(uint8_t mode);     /* LCD_BUS_SOFT / LCD_BUS_HW */
uint8_t       QSPI_GetMode(void);
void          QSPI_EnableBlock(void);         /* vendor register unlock  */
void          QSPI_Cmd(uint8_t cmd);          /* wrapped, no data        */
void          QSPI_Write(uint8_t cmd, const uint8_t *data, uint32_t len);
void          QSPI_WritePixel(uint8_t cmd, const uint8_t *data, uint32_t len);
unsigned long QSPI_KHz(void);                 /* active SCK in kHz       */

#endif /* __INTERFACE_H */
