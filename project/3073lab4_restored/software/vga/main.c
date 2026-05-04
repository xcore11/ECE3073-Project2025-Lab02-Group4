#include "io.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "vga.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"

#include <unistd.h>
#include <stdint.h>
#define ACCEL_THRESHOLD 5000
/* =========================
   Shared SDRAM buffer space
   ========================= */

#define TEXT_BUFFER_BASE        0x05200000
#define TEXT_BUFFER_SIZE        256

#define STATUS_BASE             0x05201000

#define STATUS_TEXT_READY       0x00
#define STATUS_CMD_READY        0x04
#define STATUS_SPI_RX_COUNT     0x0C

#define STATUS_REG(offset)      (*(volatile uint32_t *)(STATUS_BASE + (offset)))

int main(void)
{
    volatile char *text_buffer = (volatile char *)TEXT_BUFFER_BASE;

    printf("VGA processor started\n");
    printf("Reading text buffer at 0x%08X\n", TEXT_BUFFER_BASE);
    fflush(stdout);
    int accel_counter = ACCEL_THRESHOLD;
    int x, y, z;

	    // Failed Accel Initialization Check
	    if (accel_init() != 0) {
	        printf("Accelerometer init failed\n");
	        while (1);
	    }
    while (1)
    {

    	 accel_counter++;

    	        if (accel_counter >= 5000) {
    	            accel_counter = 0;

    	            accel_read_x(&x);
    	            accel_read_y(&y);
    	            accel_read_z(&z);

    	            printf("Accel X = %d, Y = %d, Z = %d\n", x, y, z);
    	        }
//
//        printf("\n========== VGA PROC BUFFER TEST ==========\n");
//
//        printf("STATUS_TEXT_READY   = %lu\n", (unsigned long)STATUS_REG(STATUS_TEXT_READY));
//        printf("STATUS_CMD_READY    = %lu\n", (unsigned long)STATUS_REG(STATUS_CMD_READY));
//        printf("STATUS_SPI_RX_COUNT = %lu\n", (unsigned long)STATUS_REG(STATUS_SPI_RX_COUNT));
//
//        printf("TEXT_BUFFER content = ");
//
//        for (int i = 0; i < TEXT_BUFFER_SIZE; i++)
//        {
//            char c = text_buffer[i];
//
//            if (c == '\0')
//            {
//                break;
//            }
//
//            putchar(c);
//        }
//
//        printf("\n==========================================\n");
//        fflush(stdout);

       //usleep(1000000);
    }

    return 0;
}
