/*
  touch.h - FT6336 capacitive touch over hardware I2C1 (PB8 = SCL,
  PB9 = SDA, AF4) for the nucleo-u575 TK012F6 module port.

  The touch data block is read from register 0x00 (the vendor example
  reads 8 bytes from 0; registers: 0x00 Device_Mode, 0x01 GEST_ID,
  0x02 TD_STATUS, 0x03 P1_XH, 0x04 P1_XL, 0x05 P1_YH, 0x06 P1_YL).
  7-bit slave address 0x38.
*/

#ifndef __TOUCH_H
#define __TOUCH_H

#include <stdint.h>

void Touch_Init(void);
void Touch_Read(uint8_t *buf, uint8_t len);

#endif /* __TOUCH_H */
