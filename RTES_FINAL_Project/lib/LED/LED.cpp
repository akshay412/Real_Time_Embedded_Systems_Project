#include "led.h"

LEDController::LEDController(PinName red_led_pin, PinName green_led_pin)
    : red_led(red_led_pin), green_led(green_led_pin) {}

void LEDController::turn_on_red() {
    red_led = 1;
}

void LEDController::turn_off_red() {
    red_led = 0;
}

void LEDController::turn_on_green() {
    green_led = 1;
}

void LEDController::turn_off_green() {
    green_led = 0;
}

void LEDController::toggle_red() {
    red_led = !red_led;
}

void LEDController::toggle_green() {
    green_led = !green_led;
}
void LEDController::toggle_both() {
    red_led = !red_led;
    green_led = !green_led;
}