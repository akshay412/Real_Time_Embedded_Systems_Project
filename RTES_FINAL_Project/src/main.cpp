#include "mbed.h"
#include "sensor.h"

int main() {
    SPI spi(PF_9, PF_8, PF_7, PC_1, use_gpio_ssel);
    uint8_t write_buf[32], read_buf[32];
    float gx, gy, gz;

    init_spi(spi, write_buf, read_buf);

    while (1) {
        read_sensor_data(spi, write_buf, read_buf, gx, gy, gz);
        print_data(gx, gy, gz);
        thread_sleep_for(500);
    }
}