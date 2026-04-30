#include "vga.h"
#include "system.h"
#include "sys/alt_stdio.h"
#include "alt_types.h"
#include "altera_up_avalon_accelerometer_spi.h"

static alt_up_accelerometer_spi_dev *acc_dev = NULL;

int accel_init(void)
{
    acc_dev = alt_up_accelerometer_spi_open_dev(ACCELEROMETER_SPI_0_NAME);

    if (acc_dev == NULL) {
        alt_putstr("ERROR: could not open accelerometer_spi_0\n");
        return -1;
    }

    return 0;
}

int accel_read_x(int *x)
{
    if (acc_dev == NULL || x == 0)
        return -1;

    alt_up_accelerometer_spi_read_x_axis(acc_dev, x);
    return 0;
}

int accel_read_y(int *y)
{
    if (acc_dev == NULL || y == 0)
        return -1;

    alt_up_accelerometer_spi_read_y_axis(acc_dev, y);
    return 0;
}

int accel_read_z(int *z)
{
    if (z == 0)
        return -1;

    *z = 0;   /* this IP usually gives X and Y only */
    return 0;
}
