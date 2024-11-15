#ifndef SENSOR_H
#define SENSOR_H

#include <mbed.h>

void init_spi(SPI &spi, uint8_t *write_buf, uint8_t *read_buf);
void read_sensor_data(SPI &spi, uint8_t *write_buf, uint8_t *read_buf, float &gx, float &gy, float &gz);
void print_data(float gx, float gy, float gz);

#endif // SENSOR_H