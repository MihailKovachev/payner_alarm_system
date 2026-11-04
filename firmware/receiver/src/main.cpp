#include <avr/io.h>
#include <avr/interrupt.h>

#include "keypad.h"

/*

Board Pin Layout:

D2 -> PD2 (IR receiver)
D3 -> PD3 (buzzer)
D13 -> PB5 (red LED)
D11 -> PB3 (green LED)

A5 -> PC5 (keypad 1)
A4 -> PC4 (keypad 2)
A3 -> PC3 (keypad 3)
A2 -> PC2 (keypad 4)

A1 -> PC1 (keypad 5)
A0 -> PC0 (keypad 6)
D8 -> PB0 (keypad 7)
D9 -> PB1 (keypad 8)
*/

constexpr char disarmingCode[] = {'A', 'A', 'A', 'A'}; // Terrible cybersecurity, I know

enum class AlarmState : uint8_t
{
    Armed,
    Disarmed,
    Triggered
};

volatile uint8_t detectedBurstsCount = 0;
volatile AlarmState alarmState = AlarmState::Disarmed;
volatile bool isBuzzerBuzzing = false;
volatile bool isRedLEDBlinking = false;

void startBuzzer()
{
    DDRD |= 1 << DDD3;
    PORTD &= ~(1 << PORTD3);

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
    // Configure PB5 (red LED) as output
    DDRB |= 1 << DDB5;
    PORTB |= (1 << PORTB5); // red LED off by default since default state is disarmed
    /*
    PB5 does not support hardware PWM => emulation using CTC.

    f_blink = 2 Hz
    t_on = 1 / f_blink * 0.5 = 250 ms
    t_off = 250 ms

    f_ctc = 1 / t_on = 4 Hz

    f_clk / (prescaler * f_ctc) = OCR1A + 1 <= 2^16
    prescaler_min = f_clk / (f_ctc * 65536) = 61
    prescaler = 64
    OCR1A = round (f_clk / (prescaler * f_ctc) - 1) = 62499
    */

    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    TIMSK1 = 1 << OCIE1A;
    OCR1A = 62499;
    TCNT1 = 0;

    PORTB &= ~(1 << PORTB5);
    isRedLEDBlinking = true;
}

void armAlarm()
{
    alarmState = AlarmState::Armed;

    // Configure PD2 (IR receiver output) as input
    DDRD &= ~(1 << DDD2);
    PORTD |= (1 << PORTD2); // Internal pull-up


    PORTB &= ~(1 << PORTB5); // Turn on red LED
    PORTB |= (1 << PORTB3);  // Turn off green LED

    /*
    IR receiver: TSOP34838 (https://www.vishay.com/docs/82489/tsop322.pdf)

    1 period (burst + gap) = 15 + 15 = 30 cycles
    The alarm is triggered when no complete burst is detected for 2 entire periods (60 cycles).

    t_period = 789.44 us => 1578.88 us of minimum occlusion time to trigger alarm

    f_ctc = 1 / (2 * t_period) = 633.36 Hz
    prescaler_min = f_clk / (65536 * f_ctc) = 0.39
    prescaler = 1
    OCR0A = f_clk / (prescaler * f_ctc) - 1 = 25262
    */

    // Timer1 CTC Mode
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS10);
    
    TIMSK1 = 1 << OCIE1A; // TIMER1_COMPA_vect

    OCR1A = 25262;
    TCNT1 = 0;

    // Configure External Interrupt INT0 on falling edge (ISC01 = 1, ISC00 = 0)
    EICRA |= (1 << ISC01);
    EIMSK |= (1 << INT0);
}

void disarmAlarm()
{
    alarmState = AlarmState::Disarmed;

    // Stop Timer1 (Red LED Blink) and reset flag
    TIMSK1 = 0;
    TCCR1B = 0;
    isRedLEDBlinking = false;

    // Stop Timer2 (Buzzer) and reset flag
    TCCR2A = 0;
    TCCR2B = 0;
    PORTD &= ~(1 << PORTD3); // Ensure buzzer pin is LOW
    isBuzzerBuzzing = false;

    // Stop Timer0 & INT0 (IR detection)
    TIMSK0 = 0;
    EIMSK &= ~(1 << INT0);

    PORTB |= (1 << PORTB5);  // Turn off red LED
    PORTB &= ~(1 << PORTB3); // Turn on green LED
}

void triggerAlarm()
{
    alarmState = AlarmState::Triggered;
    if (!isBuzzerBuzzing)
        startBuzzer();

    if (!isRedLEDBlinking)
        startRedLEDBlink();
}

ISR(TIMER1_COMPA_vect)
{
    if (alarmState == AlarmState::Armed)
    {
        if (detectedBurstsCount < 1)
        {
            triggerAlarm();
        }
        detectedBurstsCount = 0;
    }
    else if (alarmState == AlarmState::Triggered)
    {
        PORTB ^= (1 << PORTB5); // Toggles PORTB5 (red LED)
    }
}

// A falling edge on PD2 (IR receiver output) => 1 complete burst
ISR(INT0_vect)
{
    if (alarmState == AlarmState::Armed)
        detectedBurstsCount++;
}

int main()
{
    // Configure PB5 (red LED) as output
    DDRB |= 1 << DDB5;

    // Configure PB3 (green LED) as output
    DDRB |= 1 << DDB3;

    Keypad::initializeKeypad();

    disarmAlarm();
    sei();

    while (true)
    {
        if (alarmState == AlarmState::Disarmed)
        {
            // Poll for the A key ("arming" key)
            while (Keypad::getKeyReleased() != 'A');

            armAlarm();
        }
        else
        {
            uint8_t successfullyDetectedCodeSymbols = 0;

            while (successfullyDetectedCodeSymbols < 5)
            {
                char key = Keypad::getKeyReleased();
                if (key != '\0')
                {
                    if (key == disarmingCode[successfullyDetectedCodeSymbols])
                    {
                        successfullyDetectedCodeSymbols++;
                    }
                    else
                    {
                        successfullyDetectedCodeSymbols = (key == disarmingCode[0]) ? 1 : 0;
                    }
                }
            }
            disarmAlarm();
        }
    }
}