/*
================================================================
EMBEDDED CHALLENGE
================================================================

TEAM NUMBER     : 57

TEAM MEMBERS    : Akshay Hemant Paralikar (ap8235),
                  Rugved Rajendra Mhatre (rrm9598),
                  Bibek Poudel (bp2376)

VIDEO LINK      : https://youtu.be/Wg8uC9wbMRM
GITHUB          : https://github.com/rugvedmhatre/Embedded-Sentry

================================================================
*/

// Importing necessary libraries for LCD display, SPI communication, and other utilities
#include "drivers/LCD_DISCO_F429ZI.h"
#include "mbed.h"
#include <cmath>
#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>

// Defining constants for calculations and configurations
#define PI 3.14159

// --- Register Addresses and Configuration Values ---
#define CTRL_REG1 0x20                   // Control register 1 address
#define CTRL_REG1_CONFIG 0b01'10'1'1'1'1 // Configuration: ODR=100Hz, Enable X/Y/Z axes, power on
#define CTRL_REG4 0x23                   // Control register 4 address
#define CTRL_REG4_CONFIG 0b0'0'01'0'00'0 // Configuration: High-resolution, 2000dps sensitivity
#define CTRL_REG3 0x22                   // Control Register 3
#define CTRL_REG3_CONFIG 0b0'0'0'0'1'000 // Configuration for interrupt enable

#define SPI_FLAG 1        // Flag to indicate SPI transfer completion
#define DATA_READY_FLAG 2 // Flag for data ready interrupt

#define OUT_X_L 0x28 // Address for gyroscope's X-axis data output (low byte register)

// Scaling factor to convert raw sensor data to angular velocity in radians per second
#define SCALING_FACTOR (17.5f * 0.0174532925199432957692236907684886f / 1000.0f)

#define FILTER_COEFFICIENT 0.5f // Filter Coefficient for low-pass filters

EventFlags flags; // EventFlags for synchronizing asynchronous SPI transfers

InterruptIn button(BUTTON1); // Button input
DigitalOut led(LED1);        // LED output
Timer debounce_timer;        // Timer for button debounce

// Callback function for SPI transfer completion
void spi_cb(int event)
{
    flags.set(SPI_FLAG); // Set the SPI_FLAG to signal that transfer is complete
}

// Callback function for Data Ready Interrupt
void data_cb()
{
    flags.set(DATA_READY_FLAG); // Set DATA_READY_FLAG when data is ready
}

// Variables for raw and filtered gyroscope data
uint16_t raw_gx, raw_gy, raw_gz;                                  // Raw gyroscope values
float gx, gy, gz;                                                 // Converted gyroscope values
float filtered_gx = 0.0f, filtered_gy = 0.0f, filtered_gz = 0.0f; // Filtered gyroscope values

// A flag to indicate whether data recording is active.
// Declared as `volatile` to ensure the compiler does not optimize its access,
// as its value may be modified by an interrupt or another thread.
volatile bool record = false;

// Array to store filtered gyroscope data for the X-axis, Y-axis, Z-axis.
// Holds up to 1000 samples for analysis or further processing.
float gx_arr[1000];
float gy_arr[1000];
float gz_arr[1000];

// Variable to store the end time of a process or recording.
int end_time;

LCD_DISCO_F429ZI lcd;       // LCD object
const uint8_t spacing = 10; // Adjust spacing for characters on LCD

// Function to animate a string on the LCD
// Parameters:
// - str: The string to be animated
// - t: Time delay (in milliseconds) for each animation step
// - line: The line number on the LCD where the animation should appear
void animate_string(const std::string &str, uint32_t t, int line)
{
    int position = 0; // Initialize the starting position for the first character

    // Iterate through each character in the input string
    for (char c : str)
    {
        // Display a cursor character ('|') at the current position
        lcd.DisplayChar(position * spacing, LINE(line), '|');

        // Pause for the specified time delay to create an animation effect
        thread_sleep_for(t);

        // Replace the cursor with the actual character from the string
        lcd.DisplayChar(position * spacing, LINE(line), c);

        // Pause briefly to allow the animation effect to be noticeable
        thread_sleep_for(t / 2);

        // Move to the next character position
        position++;
    }
}

// Function to display the initial greeting message on the LCD
void display_greeting()
{
    lcd.Clear(LCD_COLOR_BLACK);            // Clear the entire LCD screen and set it to black
    lcd.SetTextColor(LCD_COLOR_DARKGREEN); // Set the text color to dark green for subsequent text
    lcd.SetBackColor(LCD_COLOR_BLACK);     // Set the background color to black for text clarity

    // Display the title "=== THE VAULT ===" centered on line 0
    // This uses the CENTER_MODE to align the text in the middle of the screen
    lcd.DisplayStringAt(0, LINE(0), (uint8_t *)"=== THE VAULT ===", CENTER_MODE);

    // Animate the greeting message "> Hello!" on line 2
    // The function animate_string gradually displays each character with a delay of 100 ms
    animate_string("> Hello!", 100, 2);
    thread_sleep_for(200); // Pause for 200 milliseconds before proceeding to the next animation

    // Animate the first part of the instruction "> Press Blue Button" on line 3
    animate_string("> Press Blue Button", 100, 3);
    // Animate the second part of the instruction "  to record key!" on line 4
    // Note: The double space at the beginning ensures alignment with the previous line
    animate_string("  to record key!", 100, 4);
    thread_sleep_for(200); // Pause briefly after displaying instructions

    // Animate a prompt "> " on line 5
    // This indicates readiness for user input or the next step
    animate_string("> ", 100, 5);
}

// Function to display the vault locked message on the LCD
void display_vault_locked()
{
    lcd.Clear(LCD_COLOR_BLACK);            // Clear the LCD screen and set it to black
    lcd.SetTextColor(LCD_COLOR_DARKGREEN); // Set the text color to dark green for the displayed content
    lcd.SetBackColor(LCD_COLOR_BLACK);     // Set the background color to black for consistent styling

    // Display the title "=== THE VAULT ===" centered on line 0
    // The CENTER_MODE ensures the text is horizontally aligned in the middle of the screen
    lcd.DisplayStringAt(0, LINE(0), (uint8_t *)"=== THE VAULT ===", CENTER_MODE);

    // Animate the message "> VAULT LOCKED!" on line 2
    // The animation function gradually displays the message with a delay of 100 ms per character
    animate_string("> VAULT LOCKED!", 100, 2);

    // Display user instructions in two parts with animation
    // First part: "> Press Blue Button" on line 3
    animate_string("> Press Blue Button", 100, 3);
    // Second part: "  to enter key!" on line 4
    // Note: The leading double space aligns this text visually with the previous line
    animate_string("  to enter key!", 100, 4);
    thread_sleep_for(200); // Pause for 200 milliseconds after the instructions are displayed

    // Animate a new prompt "> " on line 5
    // This provides a visual indicator that the system is ready for the next user action
    animate_string("> ", 100, 5);
}

// Function to display the loading screen on the LCD
void display_loading_screen()
{
    lcd.Clear(LCD_COLOR_BLACK);        // Clear the LCD screen and set it to black
    lcd.SetTextColor(LCD_COLOR_WHITE); // Set the text color to white for the displayed content
    lcd.SetBackColor(LCD_COLOR_BLACK); // Set the background color to black for consistent styling

    uint16_t centerX = lcd.GetXSize() / 2; // Get the horizontal center of the screen
    uint16_t centerY = lcd.GetYSize() / 2; // Get the vertical center of the screen
    uint16_t radius = 50;                  // Define the radius of the circular animation

    // Loop through angles from 0 to 360 degrees in 10-degree increments
    for (int angle = 0; angle < 360; angle += 10)
    {
        float rad = angle * PI / 180; // Convert the angle from degrees to radians

        // Calculate the end point of the line based on the angle and radius
        uint16_t endX = centerX + radius * cos(rad);
        uint16_t endY = centerY + radius * sin(rad);

        // Draw a line from the center to the calculated point
        lcd.DrawLine(centerX, centerY, endX, endY);

        thread_sleep_for(50); // Pause for 50 ms to create an animation effect

        lcd.SetTextColor(LCD_COLOR_BLACK); // Temporarily set the drawing color to black

        // Erase the previous line to create a smooth animation
        lcd.DrawLine(centerX, centerY, endX, endY);

        lcd.SetTextColor(LCD_COLOR_WHITE); // Reset the drawing color back to white
    }

    lcd.Clear(LCD_COLOR_BLACK);        // Clear the LCD screen and set it to black
    lcd.SetTextColor(LCD_COLOR_RED);   // Set the text color to red for the displayed content
    lcd.SetBackColor(LCD_COLOR_BLACK); // Set the background color to black for consistent styling
}

// Function to generate and display a random 8-character code on a specified LCD line
// Parameters:
// - seed: Seed for the random number generator 
// - line: The line number on the LCD where the code will be displayed
void display_random_code(int seed, int line)
{
    // Character set containing lowercase, uppercase letters, and digits
    char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    srand(seed); // Seed the random number generator

    char code[9]; // Array to store the generated 8-character code (plus null terminator)
    int i, index; // Variables for looping and random index selection

    // Generate an 8-character random code
    for (i = 0; i < 8; i++)
    {
        index = rand() % 62;      // Select a random index in the character set
        code[i] = charset[index]; // Retrieve the character at the random index
    }
    code[i] = '\0'; // Null-terminate the code string to make it a valid C-string

    // Display the generated code on the specified line, centered on the LCD
    lcd.DisplayStringAt(0, LINE(line), (uint8_t *)code, CENTER_MODE);
}

// Function to animate and display the vault unlocking process on the LCD
void display_vault_unlock()
{
    // Clear the screen and set the background to black
    lcd.Clear(LCD_COLOR_BLACK);

    // PART 1: Vault Unlock Animation

    // Define dimensions and position for the animated rectangle
    uint16_t rectWidth = 50;                              // Width of the rectangle
    uint16_t rectHeight = 20;                             // Height of the rectangle
    uint16_t rectY = lcd.GetYSize() / 2 - rectHeight / 2; // Center the rectangle vertically on the screen

    // Animate the rectangle sliding horizontally across the screen
    for (uint16_t x = 0; x < lcd.GetXSize() - rectWidth; x++)
    {
        lcd.SetTextColor(LCD_COLOR_GREEN);             // Use green for the rectangle color
        lcd.FillRect(x, rectY, rectWidth, rectHeight); // Draw the rectangle at the current position

        lcd.SetTextColor(LCD_COLOR_BLACK); // Use black to erase the previous rectangle
        if (x > 0)
        {
            lcd.FillRect(x - 1, rectY, 1, rectHeight); // Erase a one-pixel column from the previous position
        }

        thread_sleep_for(10); // Delay to control the animation speed
    }

    // PART 2: Display Random Codes

    // Clear the screen and prepare for code display
    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_RED);   // Set text color to red
    lcd.SetBackColor(LCD_COLOR_BLACK); // Set background color to black

    // Display the title at the top of the screen
    lcd.DisplayStringAt(0, LINE(0), (uint8_t *)"=== THE VAULT ===", CENTER_MODE);

    // Display the "NUCLEAR CODES" label on line 2
    lcd.DisplayStringAt(0, LINE(2), (uint8_t *)"NUCLEAR CODES", CENTER_MODE);
    thread_sleep_for(500); // Short delay before showing the codes

    // Generate and display three random codes on subsequent lines with pauses
    display_random_code(42, 3); // Display the first random code on line 3
    thread_sleep_for(1000);

    display_random_code(37, 4); // Display the second random code on line 4
    thread_sleep_for(1000);

    display_random_code(23, 5); // Display the third random code on line 5
    thread_sleep_for(1000);
}

// SPI Initialization and Communication Setup

// SPI object creation with pins:
// - PF_9: SPI clock (SCK)
// - PF_8: SPI MISO (Master In Slave Out)
// - PF_7: SPI MOSI (Master Out Slave In)
// - PC_1: SPI chip select (SS)
// - use_gpio_ssel: Use GPIO for Slave Select (SS)
SPI spi(PF_9, PF_8, PF_7, PC_1, use_gpio_ssel);

// Buffers for SPI data transfer:
// - write_buf: stores data to send to the gyroscope
// - read_buf: stores data received from the gyroscope
uint8_t write_buf[32], read_buf[32];

// Function to initialize SPI and configure gyroscope
void init_spi()
{
    // Configure SPI interface:
    // - Set data frame size to 8 bits (8-bit data)
    // - Set SPI mode 3 (CPOL = 1, CPHA = 1): idle clock high, data sampled on falling edge
    spi.format(8, 3);

    // Set the SPI communication frequency to 1 MHz
    spi.frequency(1'000'000);

    // --- Gyroscope Initialization ---

    // Configure Control Register 1 (CTRL_REG1) to enable the gyroscope and axes
    // - write_buf[0]: Address of CTRL_REG1 register
    // - write_buf[1]: Configuration value to enable the gyroscope and its axes
    write_buf[0] = CTRL_REG1;
    write_buf[1] = CTRL_REG1_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, &spi_cb); // Perform SPI transfer to write to CTRL_REG1
    flags.wait_all(SPI_FLAG);                         // Wait until SPI transfer completes

    // Configure Control Register 4 (CTRL_REG4) for sensitivity and high-resolution mode
    // - write_buf[0]: Address of CTRL_REG4 register
    // - write_buf[1]: Configuration value to set sensitivity and high-res mode
    write_buf[0] = CTRL_REG4;
    write_buf[1] = CTRL_REG4_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, &spi_cb); // Perform SPI transfer to write to CTRL_REG4
    flags.wait_all(SPI_FLAG);                         // Wait until SPI transfer completes

    // Configure Control Register 3 (CTRL_REG3) for additional settings
    write_buf[0] = CTRL_REG3;
    write_buf[1] = CTRL_REG3_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, &spi_cb); // Perform SPI transfer to write to CTRL_REG3
    flags.wait_all(SPI_FLAG);                         // Wait until SPI transfer completes

    // Set a dummy byte for write_buf[1], as a placeholder value for the write operation
    // This is necessary as SPI write operations require sending both an address and a value.
    // The placeholder value (0xFF) ensures the operation is valid.
    write_buf[1] = 0xFF;
}

// Function to read gyroscope data via SPI and store filtered values in provided arrays
// Parameters:
// - i: Current index for storing data in the arrays
// - gx_arr, gy_arr, gz_arr: Arrays to store filtered gyroscope data for X, Y, Z axes
void spi_read(int i, float *gx_arr, float *gy_arr, float *gz_arr)
{
    // Wait for the data ready flag with a 255 ms timeout (0xFF)
    // - If the flag is not set within the timeout, execution continues without blocking
    // - The flag is automatically cleared after being set
    flags.wait_all(DATA_READY_FLAG, 0xFF, true);

    // Prepare the write buffer to request gyroscope data (6 bytes total) using SPI
    // - OUT_X_L | 0x80 | 0x40: Command to read multiple bytes starting from X low register
    write_buf[0] = OUT_X_L | 0x80 | 0x40;

    // Perform SPI transfer to read gyroscope data
    // - Write buffer contains the command
    // - Read buffer receives the 6 bytes of data (X, Y, Z)
    spi.transfer(write_buf, 7, read_buf, 7, spi_cb);

    // Wait for the SPI transfer to complete
    flags.wait_all(SPI_FLAG);

    // Extract raw 16-bit gyroscope data for X, Y, and Z axes from the read buffer
    raw_gx = (read_buf[2] << 8) | read_buf[1]; // Combine high and low bytes for X-axis
    raw_gy = (read_buf[4] << 8) | read_buf[3]; // Combine high and low bytes for Y-axis
    raw_gz = (read_buf[6] << 8) | read_buf[5]; // Combine high and low bytes for Z-axis

    // Convert raw gyroscope data to radians per second using a scaling factor
    gx = raw_gx * SCALING_FACTOR;
    gy = raw_gy * SCALING_FACTOR;
    gz = raw_gz * SCALING_FACTOR;

    // Apply a manual offset correction for values exceeding 10
    if (gx > 10)
        gx -= 20;
    if (gy > 10)
        gy -= 20;
    if (gz > 10)
        gz -= 20;

    // Apply a low-pass filter (LPF) using an IIR filter formula
    // - FILTER_COEFFICIENT determines the weight of the current and previous data
    filtered_gx = FILTER_COEFFICIENT * gx + (1 - FILTER_COEFFICIENT) * filtered_gx;
    filtered_gy = FILTER_COEFFICIENT * gy + (1 - FILTER_COEFFICIENT) * filtered_gy;
    filtered_gz = FILTER_COEFFICIENT * gz + (1 - FILTER_COEFFICIENT) * filtered_gz;

    // Store the filtered gyroscope data into the respective arrays
    gx_arr[i] = filtered_gx;
    gy_arr[i] = filtered_gy;
    gz_arr[i] = filtered_gz;
}

// Function to record gyroscope data and store it in provided arrays
// Parameters:
// - gx_arr, gy_arr, gz_arr: Arrays to store the filtered gyroscope data for X, Y, and Z axes
void record_fn(float *gx_arr, float *gy_arr, float *gz_arr)
{
    // Loop to record data for 1000 iterations (until end_time reaches 1000)
    for (end_time = 0; end_time < 1000; end_time++)
    {
        // Check if the recording flag is set
        if (record)
        {
            // Read and store the gyroscope data for the current iteration
            spi_read(end_time, gx_arr, gy_arr, gz_arr);
            // Pause for 10 milliseconds to control data sampling rate
            thread_sleep_for(10);
        }
        else
        {
            // If record flag is false, break out of the loop and stop recording
            break;
        }
    }

    // Set the record flag to false after the loop completes or is broken
    record = false;
}

// Function to toggle LED state and the recording flag
void toggle_led()
{
    // Check if 200 milliseconds have passed since the last interrupt (debounce logic)
    if (debounce_timer.read_ms() > 200)
    {
        // Toggle the state of the LED (on/off)
        led = !led;

        // Toggle the record flag (start or stop recording)
        record = !record;

        // Reset the debounce timer to avoid multiple triggers in quick succession
        debounce_timer.reset();
    }
}

// Function to find patterns in angular speed data
// Returns a pair containing:
// - A string of unique patterns ('+', '-', '0')
// - A vector of end times corresponding to each unique pattern
std::pair<std::string, std::vector<int>> find_patterns(const float *angular_speed, int size = 1000, int threshold = 50,
                                                       float val_cutoff = 0.2)
{
    std::string patterns;       // String to store the detected patterns
    std::vector<int> end_times; // Vector to store the end times of each detected pattern
    int count = 0;              // Counter for consecutive occurrences of the same sign
    char last_sign = '\0';      // Variable to track the previous sign ('+', '-', or '0')

    // Loop through the angular speed data to identify patterns
    for (int i = 0; i < size; ++i)
    {
        char current_sign; // Variable to hold the current sign based on angular speed value

        // Determine the sign of the angular speed value based on the cutoff value
        if (angular_speed[i] > val_cutoff)
        {
            current_sign = '+'; // Positive angular speed
        }
        else if (angular_speed[i] < -val_cutoff)
        {
            current_sign = '-'; // Negative angular speed
        }
        else
        {
            current_sign = '0'; // No significant angular speed
        }

        // Check if the current sign differs from the last sign
        if (current_sign != last_sign)
        {
            // If the count of consecutive occurrences is above the threshold, add to patterns
            if (count >= threshold)
            {
                patterns += last_sign;      // Add the last sign to the pattern string
                end_times.push_back(i - 1); // Store the end time for the pattern
            }
            count = 1;                // Reset the count for the new pattern
            last_sign = current_sign; // Update the last sign to the current one
        }
        else
        {
            count++; // Increment the count if the sign is the same as the last one
        }
    }

    // After the loop, check if the last pattern meets the threshold and add it
    if (count >= threshold)
    {
        patterns += last_sign;         // Add the last detected sign to the pattern string
        end_times.push_back(size - 1); // Store the end time as the last index
    }

    // Remove duplicate patterns while preserving their order
    std::string unique_patterns;       // String to store unique patterns
    std::vector<int> unique_end_times; // Vector to store corresponding unique end times
    last_sign = '\0';                  // Reset last_sign to remove duplicate entries

    // Loop through the detected patterns and add unique patterns to the result
    for (size_t i = 0; i < patterns.length(); ++i)
    {
        char c = patterns[i];           // Get the current character (pattern)
        if (c != last_sign && c != '0') // Ignore duplicates and '0' (no significant angular speed)
        {
            unique_patterns += c;                     // Add the unique pattern to the result
            unique_end_times.push_back(end_times[i]); // Add corresponding end time
            last_sign = c;                            // Update last_sign to the current pattern
        }
    }

    // Return the unique patterns and their corresponding end times as a pair
    return std::make_pair(unique_patterns, unique_end_times);
}

// Function to combine patterns from three different inputs (X, Y, Z) into a final pattern string
// The function compares the timing of the patterns from three axes (X, Y, Z), ensuring that patterns are ordered and
// combined based on the given threshold. It returns a string representing the combined pattern sequence.
std::string final_pattern(const std::pair<std::string, std::vector<int>> x,
                          const std::pair<std::string, std::vector<int>> y,
                          const std::pair<std::string, std::vector<int>> z, int threshold = 40)
{
    std::string result;                           // String to store the final combined pattern result
    size_t x_index = 0, y_index = 0, z_index = 0; // Indices for traversing the X, Y, Z pattern vectors
    int prev_val = -1000;    // Variable to store the previous value for comparison (initialized to a low value)
    int x_val, y_val, z_val; // Variables to hold the current values being compared for X, Y, and Z

    // Loop to iterate until all pattern vectors (X, Y, Z) are fully traversed
    while (true)
    {
        // Get the current values from the X, Y, Z pattern vectors, or set them to a large value if the index exceeds
        // the size
        x_val = x_index < x.second.size() ? x.second[x_index] : 1000;
        y_val = y_index < y.second.size() ? y.second[y_index] : 1000;
        z_val = z_index < z.second.size() ? z.second[z_index] : 1000;

        // Break the loop when all pattern vectors have been fully traversed
        if ((x_index == x.second.size()) && (y_index == y.second.size()) && (z_index == z.second.size()))
            break;

        // Compare the current values of X, Y, and Z and select the smallest value to determine which axis to process
        // next
        if (x_val <= y_val && x_val <= z_val)
        {
            // If the current X value exceeds the threshold compared to the previous value, add a separator (comma)
            if (x_val > prev_val + threshold)
            {
                result += ","; // Add comma separator for significant change
            }
            result += "X";              // Add the axis identifier for X
            result += x.first[x_index]; // Add the pattern character from the X axis
            prev_val = x_val;           // Update the previous value to the current X value
            x_index++;                  // Move to the next pattern in the X axis
        }
        else if (y_val <= x_val && y_val <= z_val)
        {
            // If the current Y value exceeds the threshold compared to the previous value, add a separator (comma)
            if (y_val > prev_val + threshold)
            {
                result += ","; // Add comma separator for significant change
            }
            result += "Y";              // Add the axis identifier for Y
            result += y.first[y_index]; // Add the pattern character from the Y axis
            prev_val = y_val;           // Update the previous value to the current Y value
            y_index++;                  // Move to the next pattern in the Y axis
        }
        else
        {
            // If the current Z value exceeds the threshold compared to the previous value, add a separator (comma)
            if (z_val > prev_val + threshold)
            {
                result += ","; // Add comma separator for significant change
            }
            result += "Z";              // Add the axis identifier for Z
            result += z.first[z_index]; // Add the pattern character from the Z axis
            prev_val = z_val;           // Update the previous value to the current Z value
            z_index++;                  // Move to the next pattern in the Z axis
        }
    }

    return result; // Return the combined pattern result as a string
}

// Function to sort and reorder the characters 'X', 'Y', and 'Z' in each interval of the input string.
// The input string is assumed to contain multiple intervals separated by commas, and each interval contains characters
// 'X', 'Y', and 'Z' along with optional values. The function reorders each interval such that 'X' comes first, followed
// by 'Y', then 'Z', and returns a new string with the ordered intervals.

std::string sortSubstring(const std::string input)
{
    std::vector<std::string> intervals; // Vector to store the individual intervals (substrings separated by commas)
    std::string current;                // Temporary string to accumulate characters for the current interval

    // Split the input string into intervals based on commas
    for (char c : input)
    {
        if (c == ',') // If the character is a comma, it indicates the end of an interval
        {
            if (!current.empty()) // If the current interval is not empty, push it to the intervals vector
            {
                intervals.push_back(current);
                current.clear(); // Clear the current string to start accumulating the next interval
            }
        }
        else // If the character is not a comma, add it to the current interval
        {
            current += c;
        }
    }

    // If there's any remaining string after the loop, add it as the last interval
    if (!current.empty())
    {
        intervals.push_back(current);
    }

    std::string result; // String to accumulate the final sorted result

    // Process each interval to extract and reorder the characters 'X', 'Y', and 'Z'
    for (const auto &interval : intervals)
    {
        std::string x, y, z; // Strings to hold the extracted values for 'X', 'Y', and 'Z'

        // Extract the characters 'X', 'Y', and 'Z' along with any values following them
        for (size_t i = 0; i < interval.length(); ++i)
        {
            if (interval[i] == 'X') // If 'X' is found, store it with its corresponding value (if any)
            {
                x = "X" + (i + 1 < interval.length() ? std::string(1, interval[i + 1]) : "");
            }
            else if (interval[i] == 'Y') // If 'Y' is found, store it with its corresponding value (if any)
            {
                y = "Y" + (i + 1 < interval.length() ? std::string(1, interval[i + 1]) : "");
            }
            else if (interval[i] == 'Z') // If 'Z' is found, store it with its corresponding value (if any)
            {
                z = "Z" + (i + 1 < interval.length() ? std::string(1, interval[i + 1]) : "");
            }
        }

        // Reorder the values of X, Y, and Z within the interval
        std::string reordered;
        if (!x.empty()) // If 'X' was found, add it to the reordered string
            reordered += x;
        if (!y.empty()) // If 'Y' was found, add it to the reordered string
            reordered += y;
        if (!z.empty()) // If 'Z' was found, add it to the reordered string
            reordered += z;

        // Add the reordered interval to the result string, prefixing it with a comma
        result += "," + reordered;
    }

    return result; // Return the final sorted result string
}

// Main Function
int main()
{
    // Interrupt handling: Configure an interrupt on pin PA_2 with a pull-down resistor.
    // The interrupt triggers on a rising edge (button press) and calls the data_cb function.
    InterruptIn int2(PA_2, PullDown);
    int2.rise(&data_cb);

    // Attach the toggle_led function to the button's rising edge (button press)
    debounce_timer.start();   // Start the debounce timer to prevent multiple toggles
    button.rise(&toggle_led); // Trigger toggle_led on button press

    // Initialize the SPI communication and display a greeting on the LCD
    init_spi();
    display_greeting();

    // Wait for the record flag to be set (indicating the recording has started)
    while (!record)
    {
        lcd.DisplayChar(0 + 2 * spacing, LINE(5), '|'); // Display the '|' symbol
        thread_sleep_for(500);                          // Wait for 500ms
        lcd.DisplayChar(0 + 2 * spacing, LINE(5), ' '); // Clear the symbol
        thread_sleep_for(500);                          // Wait for 500ms again
    };

    // Once the recording starts, show messages on the LCD indicating the status
    lcd.DisplayStringAt(0 + 2 * spacing, LINE(5), (uint8_t *)"Recording Key...", LEFT_MODE);
    lcd.DisplayStringAt(0, LINE(6), (uint8_t *)"> Press button when", LEFT_MODE);
    lcd.DisplayStringAt(0, LINE(7), (uint8_t *)"  done.", LEFT_MODE);

    // Start the function to record data from the gyroscope
    record_fn(gx_arr, gy_arr, gz_arr);

    // After recording, display "Recording complete!" message on the LCD and pause
    lcd.DisplayStringAt(0, LINE(8), (uint8_t *)"> Recording complete!", LEFT_MODE);
    thread_sleep_for(1000);

    // Display a loading screen while processing the recorded data
    display_loading_screen();

    // Find the patterns in the recorded data for X, Y, and Z axes
    std::pair<std::string, std::vector<int>> resultX = find_patterns(gx_arr);
    std::pair<std::string, std::vector<int>> resultY = find_patterns(gy_arr);
    std::pair<std::string, std::vector<int>> resultZ = find_patterns(gz_arr);

    // Print the found patterns and their corresponding times for X, Y, and Z axes
    // printf("X : %s\t", resultX.first.c_str());
    // for (int i : resultX.second)
    //     printf("%d ", i);
    // printf("\n");

    // printf("Y : %s\t", resultY.first.c_str());
    // for (int i : resultY.second)
    //     printf("%d ", i);
    // printf("\n");

    // printf("Z : %s\t", resultZ.first.c_str());
    // for (int i : resultZ.second)
    //     printf("%d ", i);
    // printf("\n");

    // Generate the final pattern by combining the results from X, Y, and Z axes
    std::string actual = final_pattern(resultX, resultY, resultZ);
    // printf("OP : %s\n", actual.c_str());

    // Sort the substring
    actual = sortSubstring(actual);
    // printf("OP : %s\n", actual.c_str());

    // Main loop for testing gesture input
    while (true)
    {
        // Display instructions to the user
        // printf("Press button to test\n");
        display_vault_locked(); // Show the "locked" screen on the LCD

        // Wait for the record flag to be set (indicating recording has started)
        while (!record)
        {
            lcd.DisplayChar(0 + 2 * spacing, LINE(5), '|'); // Display the '|' symbol
            thread_sleep_for(500);                          // Wait for 500ms
            lcd.DisplayChar(0 + 2 * spacing, LINE(5), ' '); // Clear the symbol
            thread_sleep_for(500);                          // Wait for another 500ms
        };

        // printf("Started Test\n");

        // Display messages indicating the test is starting
        lcd.DisplayStringAt(0 + 2 * spacing, LINE(5), (uint8_t *)"Recording Key...", LEFT_MODE);
        lcd.DisplayStringAt(0, LINE(6), (uint8_t *)"> Press button when", LEFT_MODE);
        lcd.DisplayStringAt(0, LINE(7), (uint8_t *)"  done.", LEFT_MODE);

        // Start recording the test data
        record_fn(gx_arr, gy_arr, gz_arr);

        // After recording, display "Recording complete!" and wait for a moment
        // printf("Test Done\n");
        lcd.DisplayStringAt(0, LINE(8), (uint8_t *)"> Recording complete!", LEFT_MODE);
        thread_sleep_for(1000);

        // Find patterns for the test data (X, Y, and Z axes)
        resultX = find_patterns(gx_arr);
        resultY = find_patterns(gy_arr);
        resultZ = find_patterns(gz_arr);

        // Print the test data patterns and times for X, Y, and Z axes
        // printf("X : %s\t", resultX.first.c_str());
        // for (int i : resultX.second)
        //     printf("%d ", i);
        // printf("\n");

        // printf("Y : %s\t", resultY.first.c_str());
        // for (int i : resultY.second)
        //     printf("%d ", i);
        // printf("\n");

        // printf("Z : %s\t", resultZ.first.c_str());
        // for (int i : resultZ.second)
        //     printf("%d ", i);
        // printf("\n");

        // Generate and sort the test pattern
        std::string test = final_pattern(resultX, resultY, resultZ);
        test = sortSubstring(test);
        // printf("OP : %s\n", test.c_str());

        // Compare the recorded pattern with the test pattern to unlock the vault
        if (actual == test)
        {
            display_vault_unlock(); // Show the "unlocked" screen on the LCD
            // printf("Gesture Matched. Unlocked!\n");

            thread_sleep_for(5000); // Wait for 5 second before retrying
        }
        else
        {
            // printf("Gesture Match Failed. Try Again!\n");
            // Set text color to red and display an error message
            lcd.SetTextColor(LCD_COLOR_RED);
            lcd.DisplayStringAt(0, LINE(9), (uint8_t *)"> Wrong Key!", LEFT_MODE);

            thread_sleep_for(1000); // Wait for 1 second before retrying
        }
    }
}
