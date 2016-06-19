#define F_CPU 8000000UL

#include "neatavr.hpp"

#include "neatavr/serial.hpp"
#include "neatavr/twi.hpp"

#include <stdio.h>
#include <string.h>

#include <util/delay.h>

#include <util/twi.h>

#define I2C_ADDRESS 0x42

using namespace NeatAVR;

typedef Pin14 LED;

template<typename Pin> class IRDetector {
private:
    volatile uint16 counter;

    volatile uint8 state;
    volatile uint8 pulses;
public:
    volatile uint8 active;

    void init() {
        Pin::input();
    }

    void detect() {
        uint8 next_state = Pin::read();
        if (state == next_state) {
            counter++;
            if (counter > 3500) {
                counter = 0;
                active = 0;
            }
        } else {
            if (counter > 6 || counter < 2) {
                pulses = 0;
            } else {
                pulses++;
            }
            if (pulses > 10) {
                active = 1;
            }
            counter = 0;
        }
        state = next_state;
    }
};

IRDetector<Pin15> ir0;
IRDetector<Pin13> ir1;
IRDetector<Pin12> ir2;
IRDetector<Pin11> ir3;
IRDetector<Pin10> ir4;
IRDetector<Pin9>  ir5;


int main() {
    Serial::init();

    Serial::printline("\n\nTin Bot Extension Board v0.1");
    Serial::printline("Initializing");

    LED::output();

    Timer::init(Timer::Prescaler::DIV_8);
    Timer::Interrupt::enable();

    ir0.init();
    ir1.init();
    ir2.init();
    ir3.init();
    ir4.init();
    ir5.init();

    System::enable_interrupts();

    Serial::printline("Running");

    char message[64];

    while (1) {
        _delay_ms(200);

        memset(message, 0, 64);
        sprintf(message, "Sensors: %i %i %i %i %i %i", ir0.active, ir1.active, ir2.active, ir3.active, ir4.active, ir5.active);
        Serial::printline(message);
    }
}

INTERRUPT_ROUTINE(TIMER_OVERLOW_INTERRUPT) {
    // executed every 255us
    ir0.detect();
    ir1.detect();
    ir2.detect();
    ir3.detect();
    ir4.detect();
    ir5.detect();
}



// ===========================================
//TWI::init(I2C_ADDRESS);
//TWI::Interrupt::enable();



/*





unsigned int previous = 0;
unsigned int previous_active = 0;

while (1) {
    if (status != previous) {
        Serial::printline("Status:");
        char data[10];
        memset(data, 0, 10);
        sprintf(data, "%i", status);
        previous = status;
        Serial::printline(data);
    }
    if (available) {
        System::disable_interrupts();
        Serial::printline("Message:");
        Serial::write((char*) buffer, strlen((char*) buffer));
        Serial::write("\n", 1);
        available = 0;
        System::enable_interrupts();
    }

    _delay_ms(200);
    char data[64];
    memset(data, 0, 64);
    sprintf(data, "Sensors: %i %i %i %i %i %i", ir0.active, ir1.active, ir2.active, ir3.active, ir4.active, ir5.active);
    Serial::printline(data);
}*/





volatile byte available = 0;


volatile char buffer[64];




volatile unsigned int status;
volatile unsigned int counter = 0;

//volatile char buffer[64];
volatile unsigned int position;

//typedef TWI2<> twi;

INTERRUPT_ROUTINE(TWI_INTERRUPT) {
    status = TWI::status();
    switch (TWI::status()) {
        case TW_SR_SLA_ACK:
            TWI::acknowledge();
            position = 0;
            available = 0;
            break;
        case TW_SR_DATA_ACK:
            buffer[position] = (char) TWI::get();
            position++;
            TWI::acknowledge();
            break;
        case TW_ST_SLA_ACK:
        case TW_ST_DATA_ACK:
            TWI::put(0x05);
            TWI::acknowledge();
            //LED::on();
            break;
        case TW_ST_DATA_NACK:
            TWI::acknowledge();
            break;
        default:
            buffer[position] = 0;
            available = 1;
            TWI::acknowledge();
    }
}
