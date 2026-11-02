#include <avr/io.h>
#include <avr/interrupt.h>

/*
IR receiver: TSOP34838 (https://www.vishay.com/docs/82489/tsop322.pdf)

1 period (burst + gap) = 15 + 15 = 30 cycles
The alarm is triggered when no complete burst is detected for 2 entire periods (60 cycles).

t_period = 789.44 us => 1578.88 us of minimum occlusion time to trigger alarm

f_ctc_int = 1 / (2 * t_period) = 633.36 Hz
prescaler_min = f_clk / (256 * f_ctc_int) = 99
prescaler_real = 256
OCR0A = f_clk / (prescaler_real * f_ctc_int) - 1 = 97
*/

volatile uint8_t detectedBurstsCount = 0;
volatile uint8_t alarmTriggered = 0;

ISR(TIMER0_COMPA_vect)
{
if (detectedBurstsCount < 1)
    {
        alarmTriggered = 1;
    }
    else
    {
        alarmTriggered = 0; // Reset alarm when beam is clear
    }
    detectedBurstsCount = 0;
}

ISR(INT0_vect)
{
    detectedBurstsCount++;
}

int main()
{
    // Configure PD2 (IR receiver output) as input
    DDRD &= ~(1 << DDD2);
    PORTD |= (1 << PORTD2); // Internal pull-up

    // Configure PB5 (red LED) and PB4 (yellow LED)
    // red on + yellow off = alarm primed
    // red off + yellow on = alarm triggered
    DDRB |= (1 << DDB5) | (1 << DDB4);
    PORTB = 1 << PORTB4;

    // Timer0 CTC Mode
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS02);
    OCR0A = 98; // T_ctc_int ~ 1.584ms (2 periods)

    // Enable TIMER0_COMPA_vect
    TIMSK0 = 1 << OCIE0A;

    // Configure External Interrupt INT0 on falling edge (ISC01 = 1, ISC00 = 0)
    EICRA |= (1 << ISC01);
    EIMSK |= (1 << INT0);

    sei();

    while (true)
    {
        if (alarmTriggered)
        {
            // Triggered state: Red OFF, Yellow ON
            PORTB = (1 << PORTB5);
        }
        else
        {
            // Primed state: Red ON, Yellow OFF
            PORTB = (1 << PORTB4);
        }
    }
}