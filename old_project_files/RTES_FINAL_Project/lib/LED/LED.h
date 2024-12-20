#ifndef LED_H
#define LED_H

#include "mbed.h"

class LEDController {
public:
    LEDController(PinName red_led_pin, PinName green_led_pin);
    void turn_on_red();
    void turn_off_red();
    void turn_on_green();
    void turn_off_green();
    void toggle_red();
    void toggle_green();
    void toggle_both();

private:
    DigitalOut red_led;
    DigitalOut green_led;
};

#endif // LED_H