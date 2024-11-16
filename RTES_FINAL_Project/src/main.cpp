#include "mbed.h"
#include "sensor.h"
#include "led.h"
#include <vector>
#include <map>
#include <algorithm>

const int WINDOW_SIZE = 5;
std::vector<float> window_x(WINDOW_SIZE), window_y(WINDOW_SIZE), window_z(WINDOW_SIZE);
int window_index = 0;

// SPI pins
SPI spi(PF_9, PF_8, PF_7, PC_1, use_gpio_ssel); // SPI instance
uint8_t write_buf[32], read_buf[32];
// LED pins
LEDController led_controller(LED2, LED1);

// Calibration
const int CALIBRATION_SAMPLES = 100;
float gx_offset = 0, gy_offset = 0, gz_offset = 0;
// Initialize button
InterruptIn button(BUTTON1);

// Variable for button press
volatile bool recording = false;

// Variables for recording gesture
const int MAX_SAMPLES = 100;
const int RECORDING_DURATION_MS = 5000; // 5 seconds
int sample_count = 0;
float gesture_data[MAX_SAMPLES][3]; // Buffer to store 100 samples of gx, gy, gz

// Initialize Timer
Timer recording_timer;

// Function to find mode
float find_mode(std::vector<float>& data) {
    std::map<float, int> frequency_map;
    for (float value : data) {
        frequency_map[value]++;
    }
    return std::max_element(frequency_map.begin(), frequency_map.end(), 
                            [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
                                return a.second < b.second;
                            })->first;
}

// Function to find median
float find_median(std::vector<float>& data) {
    std::sort(data.begin(), data.end());
    if (data.size() % 2 == 0) {
        return (data[data.size() / 2 - 1] + data[data.size() / 2]) / 2.0;
    } else {
        return data[data.size() / 2];
    }
}

// Function to calibrate sensor
void calibrate_sensor() {
    std::vector<float> gx_samples(CALIBRATION_SAMPLES), gy_samples(CALIBRATION_SAMPLES), gz_samples(CALIBRATION_SAMPLES);
    for (int i = 0; i < CALIBRATION_SAMPLES; ++i) {
        float gx, gy, gz;
        read_sensor_data(spi, write_buf, read_buf, gx, gy, gz);
        gx_samples[i] = gx;
        gy_samples[i] = gy;
        gz_samples[i] = gz;
        ThisThread::sleep_for(10ms); // Adjust delay as needed
    }
    gx_offset = find_mode(gx_samples);
    gy_offset = find_mode(gy_samples);
    gz_offset = find_mode(gz_samples);
    printf("Calibration complete. Offsets: gx=%f, gy=%f, gz=%f\n", gx_offset, gy_offset, gz_offset);
}

// Function to apply median filter
void apply_median_filter(float& gx, float& gy, float& gz) {
    window_x[window_index] = gx;
    window_y[window_index] = gy;
    window_z[window_index] = gz;
    window_index = (window_index + 1) % WINDOW_SIZE;

    std::vector<float> temp_x = window_x;
    std::vector<float> temp_y = window_y;
    std::vector<float> temp_z = window_z;

    gx = find_median(temp_x);
    gy = find_median(temp_y);
    gz = find_median(temp_z);
}

// Function to record gesture
void start_recording() {
    if (!recording) {
        recording = true;
        sample_count = 0;
        led_controller.turn_on_red(); // Turn on red LED to indicate recording
        recording_timer.start();
        printf("Recording started. Perform the gesture now.\n");
    }
}

// Function to record gesture
void record_sample() {
    if (recording && sample_count < MAX_SAMPLES) {
        float gx, gy, gz;
        read_sensor_data(spi, write_buf, read_buf, gx, gy, gz);
        
        // Apply offsets
        gx -= gx_offset;
        gy -= gy_offset;
        gz -= gz_offset;

        // Apply median filter
        apply_median_filter(gx, gy, gz);

        gesture_data[sample_count][0] = gx;
        gesture_data[sample_count][1] = gy;
        gesture_data[sample_count][2] = gz;
        
        sample_count++;
        
        // Toggle green LED to show activity
        led_controller.toggle_green();
        
        if (sample_count >= MAX_SAMPLES || recording_timer.read_ms() >= RECORDING_DURATION_MS) {
            recording = false;
            recording_timer.stop();
            recording_timer.reset();
            led_controller.turn_off_green();
            led_controller.turn_off_red();
            printf("Recording complete. %d samples recorded.\n", sample_count);
        }
    }
}

// Read and output sensor data
void read_and_output_sensor() {
    float gx, gy, gz;
    read_sensor_data(spi, write_buf, read_buf, gx, gy, gz);
    gx-=gx_offset;
    gy-=gy_offset;
    gz-=gz_offset;
    apply_median_filter(gx, gy, gz);
    printf("Gyroscope values: gx=%f, gy=%f, gz=%f\n", gx, gy, gz);
}

int main() {
    init_spi(spi, write_buf, read_buf);
    
    // Calibrate sensor
    calibrate_sensor();
    
    button.fall([]() { start_recording(); });
    
    while (1) {
        if (recording) {
            record_sample();
        } else {
            read_and_output_sensor();
        }
        ThisThread::sleep_for(50ms); // Adjust this delay to get approximately 100 samples in 5 seconds
    }
}