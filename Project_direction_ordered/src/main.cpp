#include "mbed.h"
#include <string>
#include <vector>
#include <utility>
#include <stdlib.h>

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
// float gx_arr[1000];
// float gy_arr[1000];
// float gz_arr[1000];

float gx_arr[1000];
float gy_arr[1000];
float gz_arr[1000];
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


std::pair<std::string, std::vector<int>> find_patterns(const float* angular_speed, int size = 1000, int threshold = 50, float val_cutoff = 0.2) {
    std::string patterns;
    std::vector<int> end_times;
    int count = 0;
    char last_sign = '\0';

    for (int i = 0; i < size; ++i) {
        char current_sign;
        if (angular_speed[i] > val_cutoff) {
            current_sign = '+';
        } 
        else if (angular_speed[i] < -val_cutoff) {
            current_sign = '-';
        } 
        else {
            current_sign = '0';
        }

        if (current_sign != last_sign) {
            if (count >= threshold) {
                patterns += last_sign;
                end_times.push_back(i - 1);
            }
            count = 1;
            last_sign = current_sign;
        } 
        else {
            count++;
        }
    }

    if (count >= threshold) {
        patterns += last_sign;
        end_times.push_back(size - 1);
    }

    // Remove duplicates while preserving order
    std::string unique_patterns;
    std::vector<int> unique_end_times;
    last_sign = '\0';
    for (size_t i = 0; i < patterns.length(); ++i) {
        char c = patterns[i];
        if (c != last_sign && c != '0') {
            unique_patterns += c;
            unique_end_times.push_back(end_times[i]);
            last_sign = c;
        }
    }

    return std::make_pair(unique_patterns, unique_end_times);
}

std::string final_pattern(const std::pair<std::string, std::vector<int>> x, const std::pair<std::string, std::vector<int>> y, const std::pair<std::string, std::vector<int>> z, int threshold = 40){

    std::string result;
    size_t x_index = 0, y_index = 0, z_index = 0;
    int prev_val = -1000;
    int x_val, y_val, z_val;
    while (true){
        x_val = x_index < x.second.size() ? x.second[x_index] : 1000;
        y_val = y_index < y.second.size() ? y.second[y_index] : 1000;
        z_val = z_index < z.second.size() ? z.second[z_index] : 1000;

        if((x_index == x.second.size()) && (y_index == y.second.size()) && (z_index == z.second.size())) break;

        if(x_val <= y_val && x_val <= z_val){
            if(x_val > prev_val + threshold){
                result += ",";
            }
            result += "X";
            result += x.first[x_index];
            prev_val = x_val;
            x_index++;
        }
        else if(y_val <= x_val && y_val <= z_val){
            if(y_val > prev_val + threshold){
                result += ",";
            }
            result += "Y";
            result += y.first[y_index];
            prev_val = y_val;
            y_index++;
        }
        else{
            if(z_val > prev_val + threshold){
                result += ",";
            }
            result += "Z";
            result += z.first[z_index];
            prev_val = z_val;
            z_index++;
        }
    }
    return result;

}

std::string sortSubstring(const std::string input){
    std::string op, substring;
    for(char c : input){
        if(c == ','){
            std::sort(substring.begin(), substring.end());
            op += substring;
            op += c;
            substring = "";
        }
        else{
            substring += c;
        }
    }
    std::sort(substring.begin(), substring.end());
    op += substring;
    return op;
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


    std::pair<std::string, std::vector<int>> resultX;
    std::pair<std::string, std::vector<int>> resultY;
    std::pair<std::string, std::vector<int>> resultZ;

    resultX = find_patterns(gx_arr);
    resultY = find_patterns(gy_arr);
    resultZ = find_patterns(gz_arr);


    std::string actual, test;
    printf("X : %s\t", resultX.first.c_str());
    for(int i : resultX.second){
        printf("%d ", i);
    }
    printf("\n");
    printf("Y : %s\t", resultY.first.c_str());
    for(int i : resultY.second){
        printf("%d ", i);
    }
    printf("\n");
    printf("Z : %s\t", resultZ.first.c_str());
    for(int i : resultZ.second){
        printf("%d ", i);
    }
    printf("\n");


    actual = final_pattern(resultX, resultY, resultZ);
    printf("OP : %s\n", actual.c_str());
    actual = sortSubstring(actual);
    printf("OP : %s\n", actual.c_str());
    // printf("Z : %s\n", resultZ.c_str());;
    
    while (true)
    {
        printf("Press button to test\n");
        while(!record);
        printf("Started Test\n");
        record_fn(gx_arr, gy_arr, gz_arr);
        printf("Test Done\n");
        resultX = find_patterns(gx_arr);
        resultY = find_patterns(gy_arr);
        resultZ = find_patterns(gz_arr);
        printf("X : %s\t", resultX.first.c_str());
        for(int i : resultX.second){
            printf("%d ", i);
        }
        printf("\n");
        printf("Y : %s\t", resultY.first.c_str());
        for(int i : resultY.second){
            printf("%d ", i);
        }
        printf("\n");
        printf("Z : %s\t", resultZ.first.c_str());
        for(int i : resultZ.second){
            printf("%d ", i);
        }
        printf("\n");


        test = final_pattern(resultX, resultY, resultZ);
        test = sortSubstring(test);
        printf("OP : %s\n", test.c_str());

        if(actual == test){
             printf("Gesture Matched. Unlocked!\n");
        }
        else{
            printf("Gesture Match Failed. Try Again!");
        }
        
    }
}

