/*
  touch.h - CST816D capacitive touch over hardware I2C1 (SCL = PB8,
  SDA = PB9, AF4) for the nucleo-u575 TK018F3716 module port.

  The touch data block is read from register 0x00 (auto-increment):
  byte 3 (register 0x03) = 0x80 marks an active touch, byte 4 = X
  (8-bit), bytes 5/6 = Y ([3:0] high nibble, [7:0] low). 7-bit slave
  address 0x15.
*/

#ifndef __TOUCH_H
#define __TOUCH_H

#include <stdint.h>

void    Touch_Init(void);
void    Touch_Read(uint8_t *buf, uint8_t len);
uint8_t Touch_SelfTest(void);   /* 1 = chip ACKed on the bus */

#endif /* __TOUCH_H */
