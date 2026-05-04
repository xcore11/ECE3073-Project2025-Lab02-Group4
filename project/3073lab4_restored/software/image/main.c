#include <stdio.h>
#include <unistd.h>
#include "spi.h"
#include "decoder.h"

int main(void)
{
    printf("Image/SPI processor started\n");
    fflush(stdout);

    spi_init_manual();
    decoder_init();

    while (1)
    {
        spi_request_text_from_esp();

        /*
           If SPI stored new text into TEXT_BUFFER_BASE,
           decoder_decode_from_sdram() will decode it.
        */
        decoder_decode_from_sdram();

        usleep(1000000);
    }

    return 0;
}
