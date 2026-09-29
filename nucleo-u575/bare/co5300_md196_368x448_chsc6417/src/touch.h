/*
  touch.h - CHSC6417 capacitive touch over hardware I2C1 (SCL = PB8,
  SDA = PB9, AF4) for the nucleo-u575 TK0196M106 module port.

  The touch data block lives at register 0x00: byte 0 carries
  {[1:0] = point count, bit 6 = X bit 8, bit 7 = Y bit 8},
  byte 1 = X[7:0], byte 2 = Y[7:0]. Chip ID at register 0xA7.
  7-bit slave address 0x2E. See touch.c for the three vendor sources
  that agree on register 0x00.
*/

#ifndef __TOUCH_H
#define __TOUCH_H

#include <stdint.h>

void    Touch_Init(void);
void    Touch_Read(uint8_t *buf, uint8_t len);   /* len bytes from 0x00 */
uint8_t Touch_ChipId(void);                      /* register 0xA7       */
uint8_t Touch_SelfTest(void);                    /* 1 = chip ACKed      */
uint8_t Touch_Probe(uint8_t *buf, uint8_t len);  /* ACK + non-idle data */

#endif /* __TOUCH_H */
