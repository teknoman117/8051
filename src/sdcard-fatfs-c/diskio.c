/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for Petit FatFs (C)ChaN, 2014      */
/*-----------------------------------------------------------------------*/

#include "diskio.h"

#include <8051.h>

//#define DISKIO_DEBUG

#ifndef DISKIO_DEBUG
#include <stdio.h>
#endif /* DISKIO_DEBUG */

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

uint8_t spi_transfer(uint8_t b) {
    spi.data = b;
    return spi.data;
}

inline uint8_t spi_transfer_fast(uint8_t b) {
    spi.data = b;
    return spi.data;
}

// timing
extern volatile uint32_t centiseconds;

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

inline uint8_t sd_wait_busy(uint8_t timeout) __reentrant {
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

inline uint8_t sd_wait_block_start(uint8_t timeout) __reentrant {
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

uint8_t sd_cmd(uint8_t command, uint32_t argument) __reentrant {
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

inline uint8_t sd_acmd(uint8_t command, uint32_t argument) __reentrant {
    sd_cmd(55, 0);
    return sd_cmd(command, argument);
}

/*uint8_t sd_read_register(uint8_t idx, __xdata uint8_t* data) {
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
}*/

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (void) __reentrant
{
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
    //do {
        response = sd_cmd(SD_CARD_CMD0, 0);
    //} while (response != SD_CARD_R1_IDLE_STATE && (centiseconds - now) < 100);
    if (response != SD_CARD_R1_IDLE_STATE) {
#ifndef DISKIO_DEBUG
        printf_tiny("failed to enter idle state\r\n");
#endif /* DISKIO_DEBUG */	
        spi.control.ss = 0;
        return STA_NODISK;
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
#ifndef DISKIO_DEBUG
            printf_tiny("failed to receive check pattern - %x\r\n", response);
#endif /* DISKIO_DEBUG */	
            spi.control.ss = 0;
            return STA_NOINIT;
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
#ifndef DISKIO_DEBUG
        printf_tiny("failed to enter ready state\r\n");
#endif /* DISKIO_DEBUG */	
        spi.control.ss = 0;
        return STA_NOINIT;
    }

    // check if SDHC
    if (sd_ver2) {
        if (sd_cmd(58, 0)) {
#ifndef DISKIO_DEBUG
            printf_tiny("failed to send cmd 58\r\n");
#endif /* DISKIO_DEBUG */	
            spi.control.ss = 0;
            return STA_NOINIT;
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
}



/*-----------------------------------------------------------------------*/
/* Read Partial Sector                                                   */
/*-----------------------------------------------------------------------*/

DRESULT disk_readp (
	__xdata BYTE* buff,		/* Pointer to the destination object */
	DWORD sector,	/* Sector number (LBA) */
	UINT offset,	/* Offset in the sector */
	UINT count		/* Byte count (bit15:destination) */
) __reentrant
{
    // sanity check
    if (count + offset > 512) {
        return RES_PARERR;
    }

    // if not SDHC, use byte addressing vs LBA
    if (!sd_hc) {
        sector <<= 9;
    }

    // start read
    if (sd_cmd(17, sector)) {
        spi.control.ss = 0;
        return 1;
    }

    if (sd_wait_block_start(30)) {
        spi.control.ss = 0;
        return 2;
    }

    // skip over offset
    uint16_t i = 0;
    while (offset--) {
        spi_transfer_fast(0xFF);
        i++;
    }

    // read in data
    while (count--) {
        *(buff++) = spi_transfer_fast(0xFF);
        i++;
    }

    // skip trailing and dump crc
    while (i++ < 514) {
        spi_transfer_fast(0xFF);
    }
    spi.control.ss = 0;
    return 0;
}



/*-----------------------------------------------------------------------*/
/* Write Partial Sector                                                  */
/*-----------------------------------------------------------------------*/

DRESULT disk_writep (
	const __xdata BYTE* buff,	/* Pointer to the data to be written, NULL:Initiate/Finalize write operation */
	DWORD sc			/* Sector number (LBA) or Number of bytes to send */
) __reentrant
{
	/*DRESULT res;


	if (!buff) {
		if (sc) {

			// Initiate write process

		} else {

			// Finalize write process

		}
	} else {

		// Send data to the disk

	}

	return res;*/
	return RES_NOTRDY;
}

