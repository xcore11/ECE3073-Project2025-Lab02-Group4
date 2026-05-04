/*
 * "Hello World" example.
 *
 * This example prints 'Hello from Nios II' to the STDOUT stream. It runs on
 * the Nios II 'standard', 'full_featured', 'fast', and 'low_cost' example
 * designs. It runs with or without the MicroC/OS-II RTOS and requires a STDOUT
 * device in your system's hardware.
 * The memory footprint of this hosted application is ~69 kbytes by default
 * using the standard reference design.
 *
 * For a reduced footprint version of this template, and an explanation of how
 * to reduce the memory footprint for a given application, see the
 * "small_hello_world" template.
 *
 */

#include <stdio.h>
#include "system.h"
#include "io.h"
#include "altera_avalon_mutex.h"

//#define FRAMEBUFFER0_BASE 0x05000000 (first Image)
//#define FRAMEBUFFER1_BASE 0x05100000 (second Image for buffering)

// 32MB offset = 32 * 1024 * 1024 = 0x02000000
#define SHARED_OFF (0x02000000)

// Also, use the Bit-31 Cache Bypass trick to be 100% sure
// Add 0x80000000 to the address to tell Nios II: "Go to RAM, skip Cache!"
#define SHARED_BASE (NEW_SDRAM_CONTROLLER_0_BASE | 0x80000000)
int main() {
    // 1. Get the Mutex
    alt_mutex_dev* mutex = altera_avalon_mutex_open(MUTEX_0_NAME);

    printf("Image_proc: Writing to SDRAM...\n");

    altera_avalon_mutex_lock(mutex, 1);

    printf("Image_proc: Writing to Shared Zone...\n");
    IOWR_32DIRECT(SHARED_BASE, SHARED_OFF, 0xABC00000);
    unsigned int verify = IORD_32DIRECT(SHARED_BASE, SHARED_OFF);
    printf("Image_proc: Self-test (at 32MB offset): 0x%08X\n", verify);

    altera_avalon_mutex_unlock(mutex);
    printf("Image_proc: Done. Please check vga_proc console.\n");

    return 0;
}
