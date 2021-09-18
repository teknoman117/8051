#include <8051.h>

#include <stdint.h>
#include <stdio.h>

#include "SdInfo.h"

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
static volatile uint32_t centiseconds = 0;

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

uint8_t spi_transfer(uint8_t b) {
    spi.data = b;
    return spi.data;
}

inline uint8_t spi_transfer_fast(uint8_t b) {
    spi.data = b;
    return spi.data;
}


#define SD_CARD_SELECT 3

#define SD_CARD_CMD0 0x00
#define SD_CARD_CMD8 0x08

#define SD_CARD_CMD0_CRC 0x95
#define SD_CARD_CMD8_CRC 0x87

#define SD_CARD_R1_IDLE_STATE 0x01
#define SD_CARD_R1_ERASE_RESET 0x02
#define SD_CARD_R1_ILLEGAL_CMD 0x04
#define SD_CARD_R1_CMD_CRC_ERROR 0x08
#define SD_CARD_R1_ERASE_SEQUENCE_ERROR 0x10
#define SD_CARD_R1_ADDRESS_ERROR 0x20
#define SD_CARD_R1_PARAMETER_ERROR 0x40

#define SD_CARD_DATA_BLOCK_START 0xFE

static uint8_t sd_ver2 = 0;
static uint8_t sd_hc = 0;

inline uint8_t sd_wait_busy(uint8_t timeout) {
    // early success path
    if (spi_transfer(0xFF) == 0xFF) {
        return 0;
    }

    uint32_t start = centiseconds;
    do {
        if (spi_transfer(0xFF) == 0xFF) {
            return 0;
        }
    } while ((centiseconds - start) < timeout);
    return 1;
}

inline uint8_t sd_wait_block_start(uint8_t timeout) {
    // early success path
    if (spi_transfer(0xFF) == SD_CARD_DATA_BLOCK_START) {
        return 0;
    }

    // wait while idle
    uint32_t start = centiseconds;
    uint8_t response;
    do {
        response = spi_transfer(0xFF);
    } while ((response == 0xFF) && (centiseconds - start) < timeout);

    // return success, hopefully
    if (response == SD_CARD_DATA_BLOCK_START) {
        return 0;
    } else {
        spi.control.ss = 0;
        return 1;
    }
}

uint8_t sd_cmd(uint8_t command, uint32_t argument) {
    // select card
    spi.control.ss = SD_CARD_SELECT;
    sd_wait_busy(30);

    // send command
    spi_transfer(0x40 | command);
    spi_transfer(argument >> 24);
    spi_transfer(argument >> 16);
    spi_transfer(argument >> 8);
    spi_transfer(argument);

    // send crc (SPI mode doesn't use CRC, mostly)
    if (command == SD_CARD_CMD0) {
        spi_transfer(SD_CARD_CMD0_CRC);
    } else if (command == SD_CARD_CMD8) {
        spi_transfer(SD_CARD_CMD8_CRC);
    } else {
        spi_transfer(0xFF);
    }

    // await response
    uint8_t i = 255;
    uint8_t response = 255;
    do {
        response = spi_transfer(0xFF);
    } while ((response & 0x80) && --i);

    // return reponse
    return response;
}

inline uint8_t sd_acmd(uint8_t command, uint32_t argument) {
    sd_cmd(55, 0);
    return sd_cmd(command, argument);
}

uint8_t sd_read_register(uint8_t idx, __xdata uint8_t* data) {
    if (sd_cmd(idx, 0)) {
        spi.control.ss = 0;
        printf_tiny("failed to issue read register\r\n");
        return 1;
    }

    // wait for block to start
    if (sd_wait_block_start(30)) {
        spi.control.ss = 0;
        printf_tiny("block timed out\r\n");
        return 2;
    }

    // transfer data
    for (uint8_t i = 0; i < 16; i++) {
        data[i] = spi_transfer(0xFF);
    }

    // check
    for (uint8_t i = 0; i < 16; i++) {
        printf_tiny("(check) data[i] = %x\r\n", data[i]);
    }

    // discard crc
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    spi.control.ss = 0;
    return 0;
}

uint8_t sd_read_csd(__xdata union csd_t* csd) {
    return sd_read_register(9, (__xdata uint8_t*) csd);
}

uint32_t sd_size() {
    __xdata union csd_t csd;
    if (sd_read_csd(&csd)) {
        printf_tiny("failed to read csd\r\n");
        return 0;
    }
    if (csd.v1.csd_ver == 0) {
        printf_tiny("csd v1\r\n");
        uint8_t read_bl_len = csd.v1.read_bl_len;
        uint16_t c_size = (csd.v1.c_size_high << 10)
                | (csd.v1.c_size_mid << 2) | csd.v1.c_size_low;
        uint8_t c_size_mult = (csd.v1.c_size_mult_high << 1)
                | csd.v1.c_size_mult_low;
        return (uint32_t)(c_size + 1) << (c_size_mult + read_bl_len - 7);
    } else if (csd.v2.csd_ver == 1) {
        printf_tiny("csd v2\r\n");
        uint32_t c_size = ((uint32_t)csd.v2.c_size_high << 16)
                | (csd.v2.c_size_mid << 8) | csd.v2.c_size_low;
        return (c_size + 1) << 10;
    } else {
        printf_tiny("bad csd\r\n");
        return 0;
    }   
}

uint8_t sd_init() {
    // clear flags, set prescaler to clk / 128
    spi.control.value = 0x80;

    // dummy clocks
    uint8_t i = 255;
    do {
        spi_transfer(0xFF);
    } while (--i);

    // send CMD0 to reset SD card
    uint8_t response = 0;
    uint32_t now = centiseconds;
    do {
        response = sd_cmd(SD_CARD_CMD0, 0);
    } while (response != SD_CARD_R1_IDLE_STATE && (centiseconds - now) < 100);
    if (response != SD_CARD_R1_IDLE_STATE) {
        printf_tiny("failed to enter idle state\r\n");
        goto fail;
    }

    // go to full SCLK
    spi.control.prescaler = 0;

    // send CMD8 to check SD version
    response = sd_cmd(SD_CARD_CMD8, 0x000001AA);
    if (!(response & SD_CARD_R1_ILLEGAL_CMD)) {
        // get R7 response (ignore most bits)
        spi_transfer(0xFF);
        spi_transfer(0xFF);
        spi_transfer(0xFF);
        response = spi_transfer(0xFF);

        // ensure we receive the check pattern from argument (0xAA)
        if (response != 0xAA) {
            printf_tiny("failed to receive check pattern - %x\r\n", response);
            goto fail;
        }

        // we have a version two card
        sd_ver2 = 1;
    }

    // put card in ready state
    now = centiseconds;
    do {
        response = sd_acmd(41, sd_ver2 ? 0x40000000 : 0);
    } while (response && (centiseconds - now) < 100);
    if (response) {
        printf_tiny("failed to enter ready state\r\n");
        goto fail;
    }

    // check if SDHC
    if (sd_ver2) {
        if (sd_cmd(58, 0)) {
            printf_tiny("failed to send cmd 58\r\n");
            goto fail;
        }
        response = spi_transfer(0xFF);
        spi_transfer(0xFF);
        spi_transfer(0xFF);
        spi_transfer(0xFF);
        if ((response & 0xC0) == 0xC0) {
            sd_hc = 1;
        }
    }
    spi.control.ss = 0;
    return 0;

fail:
    spi.control.ss = 0;
    return 1;
}

uint8_t sd_read(uint32_t block, __xdata uint8_t* data) {
    // if not SDHC, use byte addressing vs LBA
    if (!sd_hc) {
        block <<= 9;
    }

    // start read
    if (sd_cmd(17, block)) {
        spi.control.ss = 0;
        return 1;
    }

    if (sd_wait_block_start(30)) {
        spi.control.ss = 0;
        return 2;
    }

    // read in data
    for (uint16_t i = 0; i < 512; i++) {
        data[i] = spi_transfer_fast(0xFF);
    }

    // dump crc
    spi_transfer_fast(0xFF);
    spi_transfer_fast(0xFF);
    spi.control.ss = 0;
    return 0;
}

void copy_pdata(uint8_t __pdata* a, uint8_t __pdata* b, uint8_t size) {
    do {
        *(a++) = *(b++);
    } while (size--);
}

void main(void) {
    setup_uart();
    setup_timer();
    EA = 1;

    uint8_t sp = SP;
    printf_tiny("stack pointer = %x\r\n", sp);

    // start sd card
    if (sd_init()) {
        printf_tiny("failed to init sd card\r\n");
        while (1);
    }

    // print what we've found
    if (sd_hc) {
        printf_tiny("found SDHC card\r\n");
    } else if (sd_ver2) {
        printf_tiny("found SD version 2 card\r\n");  
    } else {
        printf_tiny("found SD version 1 card\r\n");
    }

    // get card size
    //printf("card size: %lu\r\n", sd_size());

    // read LBA 0
    __xdata uint8_t buffer[512];
    if (sd_read(0, buffer)) {
        printf_tiny("failed to read sd card\r\n");
        while (1);
    }

    // dump data
    for (uint16_t i = 0; i < 512; i += 16) {
        printf_tiny("%x: ", i);
        for (uint8_t j = 0; j < 16; j++) {
            printf_tiny("%x ", buffer[i+j]);
        }
        printf_tiny("\r\n");
    }

    // read 64 KiB as a benchmark
    uint32_t address = 0;
    centiseconds = 0;
    for (address = 0; address != 65536; address += 512) {
        if (sd_read(address, buffer)) {
            printf_tiny("failed to read sd card\r\n");
            while (1);
        }
    }
    uint32_t duration = centiseconds;
    printf_tiny("64 KiB read took %u.%u seconds", (uint16_t) duration / 100, (uint16_t) duration % 100);

    // spin forever
    while (1);
}
