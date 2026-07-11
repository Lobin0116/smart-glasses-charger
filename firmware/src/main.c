#include "gd32e23x.h"
#include "hal_gpio.h"

void SystemInit(void) {
    hal_gpio_init();
}

int main(void) {
    while (1) {
    }
}
