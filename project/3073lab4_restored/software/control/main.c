#include "io.h"
#include <unistd.h>

// HEX base addresses
#define HEX0_BASE 0x08011190
#define HEX1_BASE 0x08011180
#define HEX2_BASE 0x08011170
#define HEX3_BASE 0x08011160
#define HEX4_BASE 0x08011150
#define HEX5_BASE 0x08011140

// Active-low 7-segment codes
#define SEG_1 0xF9
#define SEG_2 0xA4
#define SEG_3 0xB0
#define SEG_4 0x99
#define SEG_5 0x92
#define SEG_6 0x82

int main(void)
{
    while (1)
    {
        IOWR_32DIRECT(HEX0_BASE, 0, SEG_1); // HEX0 = 1
        IOWR_32DIRECT(HEX1_BASE, 0, SEG_2); // HEX1 = 2
        IOWR_32DIRECT(HEX2_BASE, 0, SEG_3); // HEX2 = 3
        IOWR_32DIRECT(HEX3_BASE, 0, SEG_4); // HEX3 = 4
        IOWR_32DIRECT(HEX4_BASE, 0, SEG_5); // HEX4 = 5
        IOWR_32DIRECT(HEX5_BASE, 0, SEG_6); // HEX5 = 6
    }

    return 0;
}
