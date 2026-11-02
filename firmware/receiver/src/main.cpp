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

enum class AlarmState : uint8_t
{
    Armed,
    Disarmed,
    Triggered
};

volatile uint8_t detectedBurstsCount = 0;
volatile AlarmState alarmState = AlarmState::Armed;
volatile bool isBuzzerBuzzing = false;
volatile bool isRedLEDBlinking = false;

ISR(TIMER0_COMPA_vect)
{
    if (detectedBurstsCount < 1)
    {
        alarmState = AlarmState::Triggered;
    }

    detectedBurstsCount = 0;

    // TODO: Arming and disarming logic 
}

// A falling edge on PD2 (IR receiver output) => 1 complete burst
ISR(INT0_vect)
{
    detectedBurstsCount++;
}

ISR(TIMER1_COMPA_vect)
{
    PINB = (1 << PINB5); // Toggles PORTB5
}

void startBuzzer()
{
    /* 
    CTC frequency (Sound frequency): f_snd = 3kHz = f_ctc
    prescaler_min = f_clk / (256 * f_ctc) = 21
    prescaler_real = 32

    OCR2A = round(f_clk / (prescaler_real * f_ctc) - 1) = 166
    OCR2B = OCR2A / 2 = 83 (50% duty cycle)
    */

    OCR2A = 166;
    OCR2B = OCR2A / 2;

    // Set fast PWM mode, enable OC2B (PD3) as output, set prescaler = 32
    TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);
    TCCR2B |= (1 << WGM22) | (1 << CS21) | (1 << CS20);

    TCNT2 = 0;
    isBuzzerBuzzing = true;
}

void startRedLEDBlink()
{
    /*
    PB5 does not support hardware PWM => emulation using CTC.

    f_blink = 2 Hz
    t_on = 1 / f_blink * 0.5 = 250 ms
    t_off = 250 ms

    f_ctc = 1 / t_on = 4 Hz

    f_clk / (prescaler * f_ctc) = OCR1A + 1 <= 2^16
    prescaler_min = f_clk / f_ctc * 65536 = 61
    prescaler = 64
    OCR1A = round (f_clk / (prescaler * f_ctc) - 1) = 62499
    */

    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    TIMSK1 = 1 << OCIE0A;
    OCR1A = 62499;
    TCNT1 = 0;

    PORTB |= (1 << PORTB5);
    isRedLEDBlinking = true;
}

int main()
{
    // Configure PD2 (IR receiver output) as input
    DDRD &= ~(1 << DDD2);
    PORTD |= (1 << PORTD2); // Internal pull-up

    // Configure PB5 (red LED) as output
    DDRB |= 1 << DDB5;
    PORTB &= ~(1 << PORTB5);

    // Configure PB3 (green LED) as output
    DDRB |= 1 << DDB3;
    PORTB |= 1 << PORTB3;

    // Configure PD3 (buzzer) as output
    DDRD |= 1 << DDD3;
    PORTD &= ~(1 << PORTD3);

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
        switch (alarmState)
        {
            case AlarmState::Armed:
            {
                // Detection as usual
                break;
            }
            case AlarmState::Disarmed:
            {
                // No detection
                // Green LED
                break;
            }
            case AlarmState::Triggered:
            {
                if (!isBuzzerBuzzing)
                    startBuzzer();
                
                if (!isRedLEDBlinking)
                    startRedLEDBlink();

                break;
            }
        }
    }
}