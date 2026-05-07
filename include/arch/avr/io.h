#ifndef IO_H
#define IO_H

#include <stdint.h>

#define DDRB  (*(volatile uint8_t *)0x24)
#define PORTB (*(volatile uint8_t *)0x25)
#define PINB  (*(volatile uint8_t *)0x23)

#endif
