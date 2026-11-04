#pragma once

#include <avr/io.h>
#include <util/delay.h>

namespace Keypad
{
    constexpr uint8_t keypadRowsCount = 4;
    constexpr uint8_t keypadColumnsCount = 4;

    constexpr char keypadLayout[keypadRowsCount][keypadColumnsCount] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}};

    // left to right
    constexpr uint8_t colPins[keypadColumnsCount] = {PC2, PC3, PC4, PC5};
    // top to bottom
    constexpr uint8_t rowPins[keypadRowsCount] = {PB1, PB0, PC0, PC1};

    void initializeKeypad()
    {
        /*
        A2 -> PC2 (keypad 4, left column)
        A3 -> PC3 (keypad 3)
        A4 -> PC4 (keypad 2)
        A5 -> PC5 (keypad 1, right column)

        D9 -> PB1 (keypad 8, top row)
        D8 -> PB0 (keypad 7)
        A0 -> PC0 (keypad 6)
        A1 -> PC1 (keypad 5, bottom row)
        */

        // Columns as inputs by default to prevent short circuits
        DDRC &= ~((1 << DDC5) | (1 << DDC4) | (1 << DDC3) | (1 << DDC2));
        PORTC &= ~((1 << PORTC5) | (1 << PORTC4) | (1 << PORTC3) | (1 << PORTC2));

        // Rows as inputs with internal pull-ups enabled
        DDRB &= ~((1 << DDB1) | (1 << DDB0));
        PORTB |= (1 << PORTB1) | (1 << PORTB0);

        DDRC &= ~((1 << DDC0) | (1 << DDC1));
        PORTC |= (1 << PORTC0) | (1 << PORTC1);
    }

    char getKeyPressed()
    {
        char currentKey = '\0';
        for (uint8_t col = 0; col < keypadColumnsCount; col++)
        {
            // Active column as output and pulled LOW
            DDRC |= (1 << colPins[col]);
            PORTC &= ~(1 << colPins[col]);
            _delay_us(5); // For voltage settling

            // Construct rows state
            uint8_t r0 = (PINB >> PINB1) & 1; // Row 0: PB1 (Keypad 8, Top)
            uint8_t r1 = (PINB >> PINB0) & 1; // Row 1: PB0 (Keypad 7)
            uint8_t r2 = (PINC >> PINC0) & 1; // Row 2: PC0 (Keypad 6)
            uint8_t r3 = (PINC >> PINC1) & 1; // Row 3: PC1 (Keypad 5, Bottom)

            uint8_t rowsState = r0 | (r1 << 1) | (r2 << 2) | (r3 << 3);

            // There is a button pressed
            if (rowsState != 0b00001111)
            {
                for (uint8_t row = 0; row < keypadRowsCount; row++)
                {
                    if (!(rowsState & (1 << row)))
                    {
                        currentKey = keypadLayout[row][col];
                    }
                }
            }

            // Return current column to HIGH Z input
            DDRC &= ~(1 << colPins[col]);
            PORTC &= ~(1 << colPins[col]);
        }

        return currentKey;
    }

    char getKeyReleased()
    {
        static char last_key = '\0';
        char current_key = getKeyPressed();

        if (current_key != last_key)
        {
            _delay_ms(20);
            current_key = getKeyPressed();
        }

        char released_key = '\0';

        if (last_key != '\0' && current_key != last_key)
            released_key = last_key;
        
        last_key = current_key;

        return released_key;
    }
}