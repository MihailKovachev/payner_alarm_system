#include <avr/io.h>
#include <avr/interrupt.h>

/*
From IR receiver datasheet (https://www.vishay.com/docs/82489/tsop322.pdf):
    - f = 38 KHz
    - t_burst >= 10 cycles (263.16 us)
    - t_gap >= 12 cycles (315.79 us)

Safety margin:

    t_burst = 15 cycles (394.73 us)
    t_gap = 15 cycles (394.73 us)
    T = t_burst + t_gap = 789.44 us
    F = 1.27 KHz

*/

enum class TimerState : uint8_t
{
    Pulse,
    Gap
};

volatile TimerState timerState = TimerState::Pulse;
volatile uint8_t timerCyclesCount = 0;

/*
Timer0 is responsible for both the burst phase and the gap phase.
The output signal is always on OC0B (Pin PD5).

During burst phase: Use OCR0A and OCR0B to generate exactly 15 pulses (duty cycle 0.5).

f_pulse = 38 kHz
Prescaler_min = f_clk / (256 * f_pulse) = 1.64 =>
=> Prescaler_real = 8

OCR0A = round( f_clk / (Prescaler_real * f_pulse) - 1) = 52
OCR0B = OCR0A / 2 = 26

During gap phase: Hold OC0B low for 394.73 us (15 cycles)
*/

ISR(TIMER0_COMPA_vect)
{
    timerCyclesCount++;

    if (timerCyclesCount >= 15)
    {
        timerCyclesCount = 0;

        switch(timerState)
        {
            case TimerState::Pulse:
            {
                timerState = TimerState::Gap;
                TCCR0A &= ~(1 << COM0B1); // Disconnect output OC0B
                break;
            }
            case TimerState::Gap:
            {
                timerState = TimerState::Pulse;
                TCCR0A |= (1 << COM0B1); // Enable output OC0B
                break;
            }
        }
    }
}

int main()
{
    // Configure Pin D5 as output (PWM output OC0B)
    DDRD |= 1 << DDD5;
    PORTD &= ~(1 << PORTD5);

    // Timer0 Fast PWM Mode 7 with TOP = OCR0A, prescaler = 8
    TCCR0A = (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << WGM02) | (1 << CS01);

    // Prescaler = 8 -> f_timer = 2 MHz
    OCR0A = 52; 
    OCR0B = 26; // 50% duty cycle

    // Enable TIMER0_COMPA_vect
    TIMSK0 = 1 << OCIE0A;

    sei();

    while (true);
}