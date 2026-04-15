#include "io.h"
#include <unistd.h>

#define HEX0_BASE 0x04011190

int main() {

    while (1) {
        IOWR_32DIRECT(HEX0_BASE, 0, 0x3F);  // ON
        usleep(500000);

        IOWR_32DIRECT(HEX0_BASE, 0, 0x00);  // OFF
        usleep(500000);
    }

    return 0;
}
