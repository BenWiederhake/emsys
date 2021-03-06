#define F_CPU 8000000UL

#include "neatavr.hpp"

#include <util/delay.h>

/**
 * Emits IR Signal using 3% or the IR-Band
 * - 33kHz carrier wave
 * - 4.3ms signal every 150ms
 */

using namespace NeatAVR;

typedef Pin4 LED;
typedef Pin9 IREmitter;

volatile uint8 carrier = 1;

void squawk() {
    // total signal length 4.3ms (delay is not precise)
    // smaller pulse width will not work
    for (uint8 counter = 0; counter < 4; counter++) {
        carrier = 1;
        _delay_us(400);
        carrier = 0;
        _delay_us(400);
    }
}

int main() {
    IREmitter::output();

    LED::output();
    LED::on();

    Timer::init(Timer::Prescaler::DIV_8);
    Timer::WaveGeneration::set(Timer::WaveGeneration::CTC);

    Timer::ChannelA::Interrupt::enable();
    Timer::ChannelA::Compare::set(14);

    System::enable_interrupts();

    while (1) {
        _delay_ms(120);
        squawk();
    }
}

INTERRUPT_ROUTINE(TIMER_COMPARE_A_INTRRUPT) {
    // generates 33kHz carrier signal
    if (carrier) {
        IREmitter::toggle();
    } else {
        IREmitter::off();
    }
}