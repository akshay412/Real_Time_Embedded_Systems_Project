#include "mbed.h"
#include <string>
#include <unordered_set>

InterruptIn button(BUTTON1); // USER_BUTTON is usually predefined for on-board buttons
// Declare a DigitalOut object for the LED
DigitalOut led(LED1);            // LED1 is usually predefined for on-board LEDs
Timer debounce_timer;
// Function to toggle the LED state


#define CTRL_REG1 0x20               // Control register 1 address
#define CTRL_REG1_CONFIG 0b01'10'1'1'1'1  // Configuration: ODR=100Hz, Enable X/Y/Z axes, power on
#define CTRL_REG4 0x23               // Control register 4 address
#define CTRL_REG4_CONFIG 0b0'0'01'0'00'0  // Configuration: High-resolution, 2000dps sensitivity

// SPI communication completion flag
#define SPI_FLAG 1

// Address of the gyroscope's X-axis output lower byte register
#define OUT_X_L 0x28

// Scaling factor for converting raw sensor data in dps (deg/s) to angular velocity in rps (rad/s)
// Combines sensitivity scaling and conversion from degrees to radians
#define DEG_TO_RAD (17.5f * 0.0174532925199432957692236907684886f / 1000.0f)

// EventFlags object to synchronize asynchronous SPI transfers
EventFlags flags;
uint8_t write_buf[32], read_buf[32];
SPI spi(PF_9, PF_8, PF_7, PC_1, use_gpio_ssel);
// --- SPI Transfer Callback Function ---
// Called automatically when an SPI transfer completes
void spi_cb(int event) {
    flags.set(SPI_FLAG);  // Set the SPI_FLAG to signal that transfer is complete
}

void init_spi(){
    

    // Buffers for SPI data transfer:
    // - write_buf: stores data to send to the gyroscope
    // - read_buf: stores data received from the gyroscope
    

    // Configure SPI interface:
    // - 8-bit data size
    // - Mode 3 (CPOL = 1, CPHA = 1): idle clock high, data sampled on falling edge
    spi.format(8, 3);

    // Set SPI communication frequency to 1 MHz
    spi.frequency(1'000'000);

    // --- Gyroscope Initialization ---
    // Configure Control Register 1 (CTRL_REG1)
    // - write_buf[0]: address of the register to write (CTRL_REG1)
    // - write_buf[1]: configuration value to enable gyroscope and axes
    write_buf[0] = CTRL_REG1;
    write_buf[1] = CTRL_REG1_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, spi_cb);  // Initiate SPI transfer
    flags.wait_all(SPI_FLAG);  // Wait until the transfer completes

    // Configure Control Register 4 (CTRL_REG4)
    // - write_buf[0]: address of the register to write (CTRL_REG4)
    // - write_buf[1]: configuration value to set sensitivity and high-resolution mode
    write_buf[0] = CTRL_REG4;
    write_buf[1] = CTRL_REG4_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, spi_cb);  // Initiate SPI transfer
    flags.wait_all(SPI_FLAG);  // Wait until the transfer completes

}

volatile bool record = false;
float gx_arr[1000];
float gy_arr[1000];
float gz_arr[1000];

float test_x[1000];
float test_y[1000];
float test_z[1000];
int end_time;

void spi_read(int i, float* gx_arr, float* gy_arr, float* gz_arr){

    uint16_t raw_gx, raw_gy, raw_gz;  // Variables to store raw data
    float gx, gy, gz;  // Variables to store converted angular velocity values

    // Prepare to read gyroscope output starting at OUT_X_L
    // - write_buf[0]: register address with read (0x80) and auto-increment (0x40) bits set
    write_buf[0] = OUT_X_L | 0x80 | 0x40; // Read mode + auto-increment

    // Perform SPI transfer to read 6 bytes (X, Y, Z axis data)
    // - write_buf[1:6] contains dummy data for clocking
    // - read_buf[1:6] will store received data
    spi.transfer(write_buf, 7, read_buf, 7, spi_cb);
    flags.wait_all(SPI_FLAG);  // Wait until the transfer completes

    // --- Extract and Convert Raw Data ---
    // Combine high and low bytes for X-axis
    raw_gx = (((uint16_t)read_buf[2]) << 8) | read_buf[1];

    // Combine high and low bytes for Y-axis
    raw_gy = (((uint16_t)read_buf[4]) << 8) | read_buf[3];

    // Combine high and low bytes for Z-axis
    raw_gz = (((uint16_t)read_buf[6]) << 8) | read_buf[5];

    // --- Debug and Teleplot Output ---
    // Print raw values for debugging purposes
    // printf("RAW Angular Speed -> gx: %d deg/s, gy: %d deg/s, gz: %d deg/s\n", raw_gx, raw_gy, raw_gz);

    // Print formatted output for Teleplot
    // printf(">x_axis: %d|g\n", raw_gx);
    // printf(">y_axis: %d|g\n", raw_gy);
    // printf(">z_axis: %d|g\n", raw_gz);

    // --- Convert Raw Data to Angular Velocity ---
    // Scale raw data using the predefined scaling factor
    gx = raw_gx * DEG_TO_RAD;
    gy = raw_gy * DEG_TO_RAD;
    gz = raw_gz * DEG_TO_RAD;


    

    // Print converted values (angular velocity in rad/s)
    // printf("Angular Speed -> gx: %.2f rad/s, gy: %.2f rad/s, gz: %.2f rad/s\n", gx, gy, gz);

    if(gx > 10){
    gx -= 20;
    }
    if(gy > 10){
        gy -= 20;
    }
    if(gz > 10){
        gz -= 20;
    }

    gx_arr[i] = gx;
    gy_arr[i] = gy;
    gz_arr[i] = gz;


    // Delay for 100 ms before the next read
}

void record_fn(float* gx_arr, float* gy_arr, float* gz_arr){
    for(end_time = 0;end_time < 1000;end_time++){
        if(record){
            spi_read(end_time, gx_arr, gy_arr, gz_arr);
            thread_sleep_for(10);
        }
        else{
            break;
        }
    }
    record = false;
}

void toggle_led()
{
    if (debounce_timer.read_ms() > 200) // Check if 200ms have passed since the last interrupt
    {
        led = !led; // Toggle the LED
        record = !record;
        debounce_timer.reset(); // Reset the debounce timer
    }
}


std::string find_patterns(const float* angular_speed, int size = 1000, int threshold = 40) {
    std::string patterns;
    int count = 0;
    char last_sign = '\0';

    for (int i = 0; i < size; ++i) {
        char current_sign;
        if (angular_speed[i] > 0.15) {
            current_sign = '+';
        } else if (angular_speed[i] < -0.15) {
            current_sign = '-';
        } else {
            current_sign = '0';
        }

        if (current_sign != last_sign) {
            if (count >= threshold) {
                patterns += last_sign;
            }
            count = 1;
            last_sign = current_sign;
        } else {
            count++;
        }
    }

    if (count >= threshold) {
        patterns += last_sign;
    }

    // Remove duplicates while preserving order
    std::string unique_patterns;
    last_sign = '\0';
    for (char c : patterns) {
        if((c != last_sign)){
            if(c != '0'){
                unique_patterns += c;
            }
            last_sign = c;
        }
    }
    return unique_patterns;
}




int main()
{
    // Attach the toggle function to the button's rising edge (button press)
    debounce_timer.start();
    button.rise(&toggle_led);

    init_spi();
    thread_sleep_for(500);
    printf("Press Record to Start\n");
    while(!record);
    printf("Started Recording\n");
    record_fn(gx_arr, gy_arr, gz_arr);
    printf("Recording Done\n");

    // for(int i = 0;i < 1000; i++){
    //     printf("i : %d, X : %.2f, Y : %.2f, Z : %.2f\n", i, gx_arr[i], gy_arr[i], gz_arr[i]);
    // }
    // printf("X,Y,Z\n");
    // for(int i = 0; i < end_time;i++){
    //     printf("%.2f,%.2f,%.2f\n", gx_arr[i], gy_arr[i], gz_arr[i]);
    // }
    std::string resultX = find_patterns(gx_arr);
    std::string resultY = find_patterns(gy_arr);
    std::string resultZ = find_patterns(gz_arr);
    printf("X : %s\n", resultX.c_str());
    printf("Y : %s\n", resultY.c_str());
    printf("Z : %s\n", resultZ.c_str());

    

    std::string opX;
    std::string opY;
    std::string opZ;
    
    // Main loop does nothing; the interrupt handles everything
    while (true)
    {
        printf("Press button to test\n");
        while(!record);
        printf("Started Test\n");
        record_fn(test_x, test_y, test_z);
        printf("Test Done\n");
        opX = find_patterns(test_x);
        opY = find_patterns(test_y);
        opZ = find_patterns(test_z);
        printf("X : %s\n", opX.c_str());
        printf("Y : %s\n", opY.c_str());
        printf("Z : %s\n", opZ.c_str());
        if((resultX == opX) & (resultY == opY) & (resultZ == opZ)){
            printf("Gesture Matched. Unlocked!\n");
        }
        else{
            printf("Gesture Match Failed. Try Again!");
        }
        
    }
}

