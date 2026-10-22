// Measures how fast mbed::Ticker (us_ticker based) can fire on LPC1768 and how
// much CPU is left for the main loop, relevant for the stepper ISR.
#include "mbed.h"

static volatile uint32_t isr_count;
static void isr() { isr_count++; }

static uint32_t spin_for_100ms()
{
    Timer t;
    t.start();
    uint32_t n = 0;
    while (t.elapsed_time() < 100ms) n++;
    return n;
}

int main()
{
    Ticker tk;
    uint32_t base = spin_for_100ms();
    printf("baseline loops/100ms: %lu\r\n", (unsigned long)base);
    for (int period : {1000, 100, 50, 20, 10, 8, 6, 4}) {
        isr_count = 0;
        tk.attach(&isr, std::chrono::microseconds(period));
        uint32_t n = spin_for_100ms();
        uint32_t hits = isr_count;
        tk.detach();
        printf("period %4d us: isr/100ms %6lu (expected %6d), cpu left %3lu%%\r\n", period,
               (unsigned long)hits, 100000 / period, (unsigned long)(100ull * n / base));
    }
    printf("done\r\n");
    while (true) ThisThread::sleep_for(1s);
}
