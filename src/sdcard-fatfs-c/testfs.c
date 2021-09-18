#include <8051.h>

#include <stdint.h>
#include <stdio.h>

#include "pff.h"

struct AM85C30 {
    uint8_t control_b;
    uint8_t data_b;
    uint8_t control_a;
    uint8_t data_a;
};

// uart location
__xdata __at(0x9400) volatile struct AM85C30 uart;

// centisecond count
volatile uint32_t centiseconds = 0;

// increment centiseconds on timer overflow
void timer0_overflow_interrupt() __interrupt(TF0_VECTOR) __using(0) {
    TH0 = 0xdc;
    centiseconds++;
}

// setup the timer
void setup_timer() {
    TMOD = 0x01;
    TL0 = 0x00;
    TH0 = 0xdc;
    ET0 = 1;
    TR0 = 1;
}

// setup the uart for 230400 8N1 (11.0592 MHz PCLK on DUART)
void setup_uart() {
    __code const uint8_t init_data[] = {
        9,  0xC0,
        4,  0x04,
        2,  0x00,
        3,  0xC0,
        5,  0x60,
        9,  0x00,
        10, 0x00,
        11, 0x56,
        12, 22,
        13, 0,
        14, 0x02,
        14, 0x03,
        3,  0xC1,
        5,  0x68
    };

    uint8_t i;
    for (i = 0; i != sizeof init_data; i++) {
        uart.control_b = init_data[i];
    }
}

inline void putbyte(uint8_t b) {
    uart.data_b = b;
    while (!(uart.control_b & 0x04));
}

inline uint8_t hasbyte() {
    return !!(uart.control_b & 0x01);
}

inline uint8_t getbyte() {
    while (!hasbyte());
    return uart.data_b;
}

int putchar(int c) {
    putbyte(c);
    return 0;
}

void main(void) {
    setup_uart();
    setup_timer();
    EA = 1;

    // mount SD card
    __xdata FATFS fs;
    if (pf_mount(&fs) != FR_OK) {
        printf_tiny("failed to mount sd card\r\n");
        goto end;
    }

    // say we succeeded
    printf_tiny("successfully mounted sd card\r\n");

    // spin forever
end:
    while (1);
}
