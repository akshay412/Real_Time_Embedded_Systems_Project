
#include "mbed.h"

// Function declarations
void init_spi(SPI &spi, uint8_t *write_buf, uint8_t *read_buf);
void read_sensor_data(SPI &spi, uint8_t *write_buf, uint8_t *read_buf, float &gx, float &gy, float &gz);
void get_gyroscope_values(SPI &spi, float &gx, float &gy, float &gz);