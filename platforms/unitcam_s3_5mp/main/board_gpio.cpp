#include "board_gpio.hpp"
#include "board_pins.hpp"
#include <driver/gpio.h>

void board_gpio_init()
{
    gpio_reset_pin(static_cast<gpio_num_t>(BOARD_PIN_LED));
    gpio_set_direction(static_cast<gpio_num_t>(BOARD_PIN_LED), GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(static_cast<gpio_num_t>(BOARD_PIN_LED), GPIO_PULLUP_ONLY);

    gpio_reset_pin(static_cast<gpio_num_t>(BOARD_PIN_BUTTON_A));
    gpio_set_direction(static_cast<gpio_num_t>(BOARD_PIN_BUTTON_A), GPIO_MODE_INPUT);
    gpio_set_pull_mode(static_cast<gpio_num_t>(BOARD_PIN_BUTTON_A), GPIO_FLOATING);
}

void board_led_set(bool on)
{
    gpio_set_level(static_cast<gpio_num_t>(BOARD_PIN_LED), on ? 0 : 1);
}
