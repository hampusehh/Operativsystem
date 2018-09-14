#include <stdint.h>

#ifndef SERIAL_h
#define SERIAL_h

#define COM1 0x3f8
void init_serial();
void serialByte(uint8_t);
void serialString(uint8_t *);
void serialPrintf(const char *, ...);

#endif
