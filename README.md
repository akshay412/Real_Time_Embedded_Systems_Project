# Embedded Sentry

Embedded Sentry is an embedded system that provides a mechanism to lock/unlock the device using one's gestures. This project is designed for the 32F429IDISCOVERY board using PlatformIO.

## Authors
- Akshay Paralikar (ap8235)
- Rugved Mhatre (rrm9598)
- Bibek Poudel (bp2376)

## Requirements

- **Board**: 32F429IDISCOVERY
- **Software**: PlatformIO IDE

## Setup

### Step 1: Install PlatformIO IDE

Ensure you have PlatformIO installed in your VS Code editor.

### Step 2: Clone the Repository

Clone the repository and open the `Embedded_Challenge` folder in PlatformIO IDE.

```bash
git clone https://github.com/rugvedmhatre/Embedded-Sentry.git
```

### Step 3: Configure PlatformIO

Make sure the following is included in your `platformio.ini` configuration file:

```ini
platform = ststm32
board = disco_f429zi
framework = mbed
build_type = debug
```

### Step 4: Build and Upload

1. Open the **Embedded_Challenge** directory in PlatformIO IDE.
2. Connect your **32F429IDISCOVERY** board via USB.
3. Build and upload the code using PlatformIO.

### Main Code

The main functionality of the program is implemented in:

- `Embedded_Challenge/src/main.cpp`

### Drivers

Necessary drivers are located in the `drivers` folder:

- `Embedded_Challenge/src/drivers/`

These drivers provide the necessary support for interacting with the gyroscope and display.

## Features

- **Gesture-based Security**: The system records and processes gyroscope data to detect specific gesture patterns. If the recorded pattern matches the expected one, it will unlock the system.

- **Graphical User Interface (GUI)**: The LCD screen provides a user interface to guide the user through recording a specific gesture pattern, which can later be used to unlock the system.

- **Button Interaction**: The system utilizes a button to switch between recording mode and idle mode, allowing the user to control the gesture recording process.

## Demo

Here is a video demonstrating the project in action:

[![alt text](https://github.com/rugvedmhatre/Embedded-Sentry/blob/main/thumbnail.png?raw=true)](https://youtu.be/Wg8uC9wbMRM)