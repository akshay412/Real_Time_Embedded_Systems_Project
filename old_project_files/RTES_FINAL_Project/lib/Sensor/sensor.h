#ifndef SENSOR_H  // Include guard start
#define SENSOR_H

#include "mbed.h"

// SPI control functions
void init_spi(SPI &spi, uint8_t *write_buf, uint8_t *read_buf);
void read_sensor_data(SPI &spi, uint8_t *write_buf, uint8_t *read_buf, float &gx, float &gy, float &gz);

#endif // Include guard end