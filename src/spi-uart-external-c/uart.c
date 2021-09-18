#include <8051.h>
#include <stdint.h>

struct AM85C30 {
    uint8_t control_b;
    uint8_t data_b;
    uint8_t control_a;
    uint8_t data_a;
};

struct SPI {
    uint8_t data;
    union {
        struct {
            uint8_t prescaler : 2;
            uint8_t ss : 2;
            uint8_t zero1 : 1;
            uint8_t busy : 1;
            uint8_t interrupt_enabled : 1;
            uint8_t interrupt_flag : 1;
        };
        uint8_t value;
    } control;
};

// spi location
__xdata __at(0x8400) volatile struct SPI spi;

// uart location
__xdata __at(0x9400) volatile struct AM85C30 uart;

// centisecond count
static volatile uint8_t centiseconds = 0;

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

inline void putchar(uint8_t b) {
    uart.data_b = b;
    while (!(uart.control_b & 0x04));
}

inline uint8_t haschar() {
    return !!(uart.control_b & 0x01);
}

inline uint8_t getchar() {
    return uart.data_b;
}

void print(const __code char* str) {
    uint8_t i = 0;
    while (str[i]) {
        putchar(str[i++]);
    }
}

void main(void) {
    setup_uart();
    setup_timer();
    EA = 1;

    // divide clk by 128 for sck, send zero
    spi.control.value = 0x83;
    spi.data = 0x00;
    while (!spi.control.interrupt_flag);
    spi.control.value = spi.control.value;

    while (1) {
        // dump everything in the FIFO
        while (haschar()) {
            // write stuff out to uart
            uint8_t b = getchar();

            // echo the char
            print("sending char (on spi): ");
            putchar(b);
            print("\r\n");

            // send via spi
            spi.data = b;
            while (!spi.control.interrupt_flag);
            spi.control.value = spi.control.value;

            // print the char we get back
            print("got char (on spi): ");
            putchar(spi.data);
            print("\r\n");
        }

        // print message
        print("Hello, 8051 SDCC\r\n");

        // wait one second
        while (centiseconds < 100);
        centiseconds = 0;
    }
}
