#include "mbed.h"

static DigitalOut leds[] = {DigitalOut(LED1), DigitalOut(LED2), DigitalOut(LED3), DigitalOut(LED4)};

int main()
{
    printf("LaosLaser Mbed CE bring-up, target %s\r\n", MBED_STRINGIFY(TARGET_NAME));
    unsigned n = 0;
    while (true) {
        for (unsigned i = 0; i < 4; i++) {
            leds[i] = (i == n % 4);
        }
        printf("tick %u\r\n", n++);
        ThisThread::sleep_for(250ms);
    }
}
