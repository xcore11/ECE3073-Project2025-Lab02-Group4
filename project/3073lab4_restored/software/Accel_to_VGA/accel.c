
#include "system.h"
#include "sys/alt_stdio.h"
#include "alt_types.h"
#include "altera_up_avalon_accelerometer_spi.h"

static alt_up_accelerometer_spi_dev *acc_dev = NULL;

int accel_init(void)
{
    acc_dev = alt_up_accelerometer_spi_open_dev("/dev/accelerometer_spi_0");

    if (acc_dev == NULL) {
        alt_putstr("[ACCEL] Failed to open device.\n");
        return -1;
    } else {
        alt_putstr("[ACCEL] Device initialized successfully\n");
    }

    return 0;
}

int accel_read_x(alt_32 *x)
{
    if (acc_dev == NULL || x == NULL)
        return -1;

    alt_up_accelerometer_spi_read_x_axis(acc_dev, x);
    return 0;
}

int accel_read_y(alt_32 *y)
{
    if (acc_dev == NULL || y == NULL)
        return -1;

    alt_up_accelerometer_spi_read_y_axis(acc_dev, y);
    return 0;
}

int accel_read_z(alt_32 *z)
{
    if (acc_dev == NULL || z == NULL)
        return -1;

    alt_up_accelerometer_spi_read_z_axis(acc_dev, z);
    return 0;
}
