#include "mbed.h"
#include "sensor.h" //Custom made library for gyro sensor
#include "led.h" //Custom made library for LED control
#include <vector>
#include <map>
#include <algorithm>
//Final Working Code 
//Calibration and Analysis
const int WINDOW_SIZE = 5; // Window size for median filter
std::vector<float> window_x(WINDOW_SIZE), window_y(WINDOW_SIZE), window_z(WINDOW_SIZE); // Window buffers
int window_index = 0; // Index for window buffers for gesture mapping
const int CALIBRATION_SAMPLES = 100; // Number of samples for calibration (We haven't used this but it can be used for calibration)
float gx_offset = 0, gy_offset = 0, gz_offset = 0; // Offset values for calibration (We haven't used this but it can be used for calibration)
bool analysis_required=false;//Flag to indicate if analysis is required after recording the gesture data for comparision
// SPI pins
SPI spi(PF_9, PF_8, PF_7, PC_1, use_gpio_ssel); // SPI instance
uint8_t write_buf[32], read_buf[32]; // Buffers for SPI communication

// LED pins
LEDController led_controller(LED2, LED1); // LED controller instance

// Button for recording
InterruptIn button(BUTTON1); // Button instance InterruptIn

// Constants for recording
const int MAX_SAMPLES = 200;               // Maximum number of samples
const int RECORDING_DURATION_MS = 3000;    // Recording duration in milliseconds
const int SAMPLING_INTERVAL_MS = RECORDING_DURATION_MS / MAX_SAMPLES; // Interval between samples

// Buffers for gesture data
float recorded_gesture_data[MAX_SAMPLES][3]; // Buffer for reference gesture
float gesture_data[MAX_SAMPLES][3];          // Buffer for test gesture

// Timer and Variables
Timer sample_timer;   // Timer for sampling intervals
int sample_count = 0; // Sample counter
volatile bool recording = false; // Flag to indicate recording state
volatile bool key_recording = true; // Flag to indicate recording state
float key_gesture_map[20][3]; // Buffer for gesture mapping
int gesture_map_window_size =30;
float recorded_gesture_map[20][3]; // Buffer for gesture mapping
//Function Declarations
void record_key();
void record_gesture_data();
void map_gesture_data(float gesture_data[MAX_SAMPLES][3], float threshold, int window_size, float gesture_map[20][3]);
void print_data(float buffer[MAX_SAMPLES][3]);
bool compare_gesture(float gesture_data[20][3], float gesture_map[20][3]);

// Function to find mode. This function is used for calibration of the sensor at stable position. Sensor was giving nosiy data so we took mode of the data to get the stable value.
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

// Function to find median. This function is for filtering the data. We have used median filter to filter the data to remove the outliers.
float find_median(std::vector<float>& data) {
    std::sort(data.begin(), data.end());
    if (data.size() % 2 == 0) {
        return (data[data.size() / 2 - 1] + data[data.size() / 2]) / 2.0;
    } else {
        return data[data.size() / 2];
    }
}

// Function to calibrate sensor. This function is used to calibrate the sensor at the stable position. We have used mode of the data to get the stable value.
void calibrate_sensor() {
    std::vector<float> gx_samples(CALIBRATION_SAMPLES), gy_samples(CALIBRATION_SAMPLES), gz_samples(CALIBRATION_SAMPLES);
    for (int i = 0; i < CALIBRATION_SAMPLES; ++i) {
        float gx, gy, gz;
        read_sensor_data(spi, write_buf, read_buf, gx, gy, gz);
        gx_samples[i] = gx;
        gy_samples[i] = gy;
        gz_samples[i] = gz;
        thread_sleep_for(10); // Adjust delay as needed
    }
    gx_offset = find_mode(gx_samples);
    gy_offset = find_mode(gy_samples);
    gz_offset = find_mode(gz_samples);
    printf("Calibration complete: gx_offset=%f, gy_offset=%f, gz_offset=%f\n", gx_offset, gy_offset, gz_offset);
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

//Printing data for debugging using teleplot
void print_data(float buffer[MAX_SAMPLES][3]) {
    for (int i = 0; i < MAX_SAMPLES; ++i) {
        printf(">gx:%f\n", buffer[i][0]);
        printf(">gy:%f\n", buffer[i][1]);
        printf(">gz:%f\n", buffer[i][2]);
    }
}
// Function to record the key gesture into the target buffer. We have used this function to record the key gesture which is used for comparision with the recorded gesture.
void record_key() {
    sample_timer.start();// Start the timer
    if(sample_timer.read_ms() >= SAMPLING_INTERVAL_MS) {// Check if the sampling interval has elapsed
        if(sample_count < MAX_SAMPLES) {// Check if the sample count is less than the maximum samples
            float gx, gy, gz;
            read_sensor_data(spi, write_buf, read_buf, gx, gy, gz);
            gx-=gx_offset; // Subtract offset. If calibration is not done. The offset will be zero.
            gy-=gy_offset;// Subtract offset. If calibration is not done. The offset will be zero.
            gz-=gz_offset;// Subtract offset. If calibration is not done. The offset will be zero.
            apply_median_filter(gx, gy, gz);// Apply median filter to filter the data(outliers)
            recorded_gesture_data[sample_count][0] = gx;// Store the filtered data in the buffer
            recorded_gesture_data[sample_count][1] = gy;// Store the filtered data in the buffer
            recorded_gesture_data[sample_count][2] = gz;// Store the filtered data in the buffer
            sample_count++;// Increment the sample count
            sample_timer.reset();// Reset the timer
            led_controller.turn_on_green();// Turn on the green LED to indicate the recording is in progress
        } else {
            key_recording = false;// Set the key recording flag to false
            led_controller.turn_off_green();// Turn off the green LED to indicate the recording is completed
            sample_count = 0;// Reset the sample count
            sample_timer.stop();// Stop the timer
            map_gesture_data(recorded_gesture_data, 15, 30, key_gesture_map);// Map the recorded gesture data into direction values 0,1,-1 to compare directions at certain timestamps
            print_data(key_gesture_map); // Print the key gesture data for debugging. While implementation it makes the execution slower due to serial Communication.
        }
    }
}

void record_gesture_data() { // Function to record the gesture data into the target buffer. We have used this function to record the gesture which is used for comparision with the key gesture.
    sample_timer.start();// Start the timer
    if (recording) {
        if(sample_timer.read_ms() >= SAMPLING_INTERVAL_MS) {// Check if the sampling interval has elapsed
            if(sample_count < MAX_SAMPLES) {// Check if the sample count is less than the maximum samples
                float gx, gy, gz;
                read_sensor_data(spi, write_buf, read_buf, gx, gy, gz);// Read the sensor data
                            gx-=gx_offset;// Subtract offset. If calibration is not done. The offset will be zero.
            gy-=gy_offset;// Subtract offset. If calibration is not done. The offset will be zero.
            gz-=gz_offset;// Subtract offset. If calibration is not done. The offset will be zero.
            apply_median_filter(gx, gy, gz);// Apply median filter to filter the data(outliers)
                gesture_data[sample_count][0] = gx;// Store the filtered data in the buffer
                gesture_data[sample_count][1] = gy;// Store the filtered data in the buffer
                gesture_data[sample_count][2] = gz;// Store the filtered data in the buffer
                sample_count++;// Increment the sample count
                sample_timer.reset();// Reset the timer
                led_controller.turn_on_green();// Turn on the green LED to indicate the recording is in progress
            } else {
                recording = false;// Set the recording flag to false
                led_controller.turn_off_green();// Turn off the green LED to indicate the recording is completed
                sample_count = 0;// Reset the sample count
                sample_timer.stop();// Stop the timer
                map_gesture_data(gesture_data, 15, 30, recorded_gesture_map);// Map the recorded gesture data into direction values 0,1,-1 to compare directions at certain timestamps
                print_data(recorded_gesture_map);// Print the recorded gesture data for debugging. While implementation it makes the execution slower due to serial Communication.
            }
        }
    }
}


// Timer for adjustment time
Timer adjustment_timer; // Timer for adjustment time
bool adjustment_time_elapsed = false;// Flag to indicate if the adjustment time has elapsed

// ISR for button press (start recording)
void button_pressed_isr() { // ISR for button press (start recording)
    recording = true;// Set the recording flag to true
    adjustment_timer.reset();// Reset the adjustment timer
    adjustment_timer.start();// Start the adjustment timer
    adjustment_time_elapsed = false;// Set the adjustment time elapsed flag to false
    led_controller.turn_on_red();// Turn on the red LED to indicate the recording is in progress
    analysis_required=true;// Set the analysis required flag to true
}

// Main function
int main() {
    analysis_required=false;// Set the analysis required flag to false
    led_controller.turn_on_red();// Turn on the red LED to indicate that the system is not unlocked.
    led_controller.turn_off_green();// Turn off the green LED to indicate that recording is not in progress.
    key_recording = true;// Set the key recording flag to true. So, the system will record the key gesture first in each start. So, we can use black button to record the key gesture. 
    recording = false; // Set the recording flag to false
    // Initialize SPI
    init_spi(spi, write_buf, read_buf);// Initialize the SPI communication
    thread_sleep_for(1500);// Sleep for 1.5 seconds to stabilize the sensor and give time for user to adjust for gesture recording.
    // Calibrate sensor
    //calibrate_sensor();
    //thread_sleep_for(5000);
    // Start the sample timer once, not resetting it until necessary
    
    // Attach the ISR to the button press event
    button.fall(&button_pressed_isr); // Use falling edge for button press
    while (1) {
        // Call the appropriate function based on the recording state
        if (key_recording) {// If the key recording flag is true then record the key gesture
            record_key(); // Record the key gesture. It will be triggered by black button press/Every time the system is started.
        } else {
            if (recording) { // If the recording flag is true then record the gesture
                if (!adjustment_time_elapsed) {// If the adjustment time has not elapsed then wait.
                    if (adjustment_timer.read_ms() >= 1500) { // Wait for 1.5 seconds for user to adjust.
                        adjustment_time_elapsed = true;// Set the adjustment time elapsed flag to true
                        adjustment_timer.stop();// Stop the adjustment timer
                    }
                } else {
                    record_gesture_data();// Record the gesture data
                }
            }
        }
        if (!key_recording && !recording && analysis_required) { // If the key recording and recording flags are false and analysis required flag is true then compare the gestures.
            if (compare_gesture(key_gesture_map, recorded_gesture_map)) {// Compare the gestures
                led_controller.turn_off_red();//If the gestures are same then turn off the red LED
                analysis_required=false;// Set the analysis required flag to false. Analysis falg is set to true only when gestures are recorded to be compared with key gesture.
            }
        }
    }
}

//Create a function to crete a sliding window of size 20, take average of that window and then compare it with the threshold value. If the average value is greater than +threshold value for particular axis map it as 1, if it is less than -threshold value for particular axis map it as -1 and if it is between -threshold and +threshold map it as 0.
/*This is our main logic for the gesture recognition. We made a sliding window that slides over the gesture data in a window size/certain time interval in milliseconds and take the average of the angular velocity. The sliding window will account for the short delays between the gestures. The sliding window also accounts for delays by sliding over the gesture data every windowsize/2 so that delays have less effect. Taking average will account for the amplitude difference.i.e we dont care how fast people move hand if it is above certain threshold it will map a gesture. The threshold is set in such a way that it is minimum to perform a gesture in 3 seconds. With this method we get the direction of movement of hand at different instances and check if the directions are same in both vectors containing recorded and key gesture*/
void map_gesture_data(float gesture_data[MAX_SAMPLES][3], float threshold, int window_size, float gesture_map[20][3]) {
    int window_start = 0;// Start of the window
    int window_end = window_size;// End of the window
    int gesture_map_counter = 0;// Counter for gesture map

    while (window_end <= MAX_SAMPLES) {// Loop until the window end is less than the maximum samples
        float gx_sum = 0, gy_sum = 0, gz_sum = 0;// Sum of the angular velocity

        for (int i = window_start; i < window_end; i++) {// Loop over the window
            gx_sum += gesture_data[i][0];// Sum of the angular velocity
            gy_sum += gesture_data[i][1];// Sum of the angular velocity
            gz_sum += gesture_data[i][2];// Sum of the angular velocity
        }

        float gx_avg = gx_sum / window_size;// Average of the angular velocity
        float gy_avg = gy_sum / window_size;// Average of the angular velocity
        float gz_avg = gz_sum / window_size;// Average of the angular velocity

        gesture_map[gesture_map_counter][0] = (gx_avg > threshold) ? 1 : (gx_avg < -threshold) ? -1 : 0;// Map the gesture data into direction values 0,1,-1
        gesture_map[gesture_map_counter][1] = (gy_avg > threshold) ? 1 : (gy_avg < -threshold) ? -1 : 0;// Map the gesture data into direction values 0,1,-1
        gesture_map[gesture_map_counter][2] = (gz_avg > threshold) ? 1 : (gz_avg < -threshold) ? -1 : 0;// Map the gesture data into direction values 0,1,-1

        window_start += window_size / 2;// Start of the window
        window_end += window_size / 2;// End of the window
        gesture_map_counter++;// Increment the gesture map counter
    }
}
//The gesture mapped data are -1,1,0 so use a simple algorithm to account for delay and check if the two mapped data are same or not. If they are same then the gesture is same. If they are not same then the gesture is different.
//We accounted for threshold of 2 to account for some error. We can change the threshold value to make the algorithm more robust(by decreasing the threshold value) or less robust(by increasing the threshold value). value of 2 worked best for us.
bool compare_gesture(float gesture_map1[20][3], float gesture_map2[20][3]) {
    int false_counter=0;// Counter for false values
    for (int i = 0; i < 20; i++) {// Loop over the gesture map
        if (gesture_map1[i][0] != gesture_map2[i][0] || gesture_map1[i][1] != gesture_map2[i][1] || gesture_map1[i][2] != gesture_map2[i][2]) {// Check if the gesture map values are same
            false_counter++;// Increment the false counter
        }
    }
    if (false_counter>2) {// Check if the false counter is greater than 2
        return false;// Return false
    } else {
        return true;// Return true
    }
}