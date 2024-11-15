#include "sensor.h"

#define CTRL_REG1 0x20
#define CTRL_REG1_CONFIG 0b01'10'1'1'1'1
#define CTRL_REG4 0x23
#define CTRL_REG4_CONFIG 0b0'0'01'0'00'0
#define SPI_FLAG 1
#define OUT_X_L 0x28
#define SCALING_FACTOR (17.5f * 0.0174532925199432957692236907684886f / 1000.0f)

EventFlags flags;

void spi_cb(int event) {
    flags.set(SPI_FLAG);
}

void init_spi(SPI &spi, uint8_t *write_buf, uint8_t *read_buf) {
    spi.format(8, 3);
    spi.frequency(1'000'000);

    write_buf[0] = CTRL_REG1;
    write_buf[1] = CTRL_REG1_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, spi_cb);
    flags.wait_all(SPI_FLAG);
}

void read_sensor_data(SPI &spi, uint8_t *write_buf, uint8_t *read_buf, float &gx, float &gy, float &gz) {
    uint16_t raw_gx, raw_gy, raw_gz;

    write_buf[0] = OUT_X_L | 0x80 | 0x40;
    spi.transfer(write_buf, 7, read_buf, 7, spi_cb);

    raw_gx = (((uint16_t)read_buf[2]) << 8) | ((uint16_t)read_buf[1]);
    raw_gy = (((uint16_t)read_buf[4]) << 8) | ((uint16_t)read_buf[3]);
    raw_gz = (((uint16_t)read_buf[6]) << 8) | ((uint16_t)read_buf[5]);

    gx = ((float)raw_gx) * SCALING_FACTOR;
    gy = ((float)raw_gy) * SCALING_FACTOR;
    gz = ((float)raw_gz) * SCALING_FACTOR;
}

void print_data(float gx, float gy, float gz) {
    printf("teleplot:gx=%f,gy=%f,gz=%f\n", gx, gy, gz);
}