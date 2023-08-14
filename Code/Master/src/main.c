#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>

unsigned char keypad_conf[4][3] = {
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9},
    {10, 0, 11}};

int temp_latest = 0;
int light_latest = 0;

char keypad()
{
    unsigned char col, row;

    // Check if any key is pressed
    do
    {
        PORTC &= 0x0F;
        row = PINC & 0x0F;
    } while (row != 0x0F);

    do
    {
        row = PINC & 0x0F; // Wait for the key to be released
    } while (row == 0x0F); // Continue looping while no key is released

    // Determine the column of the pressed key
    while (1)
    {
        PORTC = 0xEF; // 0b11101111
        row = PINC & 0x0F;
        if (row != 0x0F)
        {
            col = 0;
            break;
        }
        PORTC = 0xDF; // 0b11011111
        row = PINC & 0x0F;
        if (row != 0x0F)
        {
            col = 1;
            break;
        }
        PORTC = 0xBF; // 0b10111111
        row = PINC & 0x0F;
        if (row != 0x0F)
        {
            col = 2;
            break;
        }
    }

    int res;
    // Determine the row and return the corresponding button value
    if (row == 0x0E)
    {                              // 0x0e = 0b00001110
        res = keypad_conf[0][col]; // button 1, 2, 3
        return res;
    }
    else if (row == 0x0D)
    {                              // 0x0d = 0b00001101
        res = keypad_conf[1][col]; // button 4, 5, 6
        return res;
    }
    else if (row == 0x0B)
    {                              // 0x0b = 0b00001011
        res = keypad_conf[2][col]; // button 7, 8, 9
        return res;
    }
    else
    {                              // 0x07 = 0b00000111
        res = keypad_conf[3][col]; // button *, 0, #
        return res;
    }
}

int main()
{
    DDRB = (1 << DDB7) | (0 << DDB6) | (1 << DDB5) | (1 << DDB4);
    PORTB = (1 << PORTB4);
    DDRC = (1 << DDC6) | (1 << DDC5) | (1 << DDC4);
    PORTC = (1 << PORTC7) | (1 << PORTC6) | (1 << PORTC5) | (1 << PORTC4) | (1 << PORTC3) | (1 << PORTC2) | (1 << PORTC1) | (1 << PORTC0);
    PORTD = (1 << PORTD2); // pull-up resistor for INT0

    SPCR = (0 << SPIE) | (1 << SPE) | (0 << DORD) | (1 << MSTR) | (0 << CPOL) | (0 << CPHA) | (1 << SPR1) | (1 << SPR0); // clock/128
    PORTB |= (1 << PORTB4);                                                                                              // Deselect Slave
    SPSR = (0 << SPI2X);
    GICR = (0 << INT1) | (1 << INT0) | (0 << INT2);                    // enable intrupt INT0 and INT1
    MCUCR = (0 << ISC11) | (0 << ISC10) | (1 << ISC01) | (0 << ISC00); // intrupt INT0 is falling edge

    ADMUX = (0 << REFS1) | (1 << REFS0) | (0 << ADLAR) | (0 << MUX3) | (0 << MUX2) | (0 << MUX1) | (0 << MUX0); // A0 is input & Vref = 5V (Vref --> AVcc)
    ADCSRA = (1 << ADIE) | (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);                            // enable ADC and set prescaler to 128, enable intrupt

    _delay_ms(500);
    ADCSRA |= (1 << ADSC); // start conversion

    sei();

    while (1)
    {
        unsigned char data = 200 + keypad();
        PORTB &= ~(1 << PORTB4); // Select Slave
        SPDR = data;
        _delay_ms(100);
        while (!(SPSR & (1 << SPIF)))
            ;
        PORTB |= (1 << PORTB4); // Deselect Slave #1
    }
}

ISR(INT0_vect)
{
    PORTD ^= (1 << 0);
    PORTB &= ~(1 << PORTB4); // Select Slave 1

    SPDR = 212;
    while (((SPSR >> SPIF) & 1) == 0)
        ;
    PORTB |= (1 << PORTB4); // Deselect Slave 1
}

ISR(ADC_vect)
{
    _delay_ms(3);
    int adc = ADCW;

    // make multiplexer that make once A0 use and once A1 use

    // LDR --> ADC1
    if (ADMUX & (1 << MUX0))
    {
        int light = adc / 7.65;

        if (light_latest != light)
        {
            light_latest = light;
            // send temperature to slave
            _delay_ms(100);
            unsigned char data = 100 + light;
            PORTB &= ~(1 << PORTB4); // Select Slave 1
            SPDR = data;
            _delay_ms(100);
            while (!(SPSR & (1 << SPIF)))
                ;
            PORTB |= (1 << PORTB4); // Deselect Slave 1
        }
    }
    else
    {
        int temperature = adc * (0.48828125);

        if (temp_latest != temperature)
        {
            temp_latest = temperature;
            // send temperature to slave
            _delay_ms(100);
            unsigned char data = temperature;
            PORTB &= ~(1 << PORTB4); // Select Slave
            SPDR = data;
            _delay_ms(100);
            while (!(SPSR & (1 << SPIF)))
                ;
            PORTB |= (1 << PORTB4); // Deselect Slave #1
        }
    }
    ADMUX ^= (1 << MUX0);
    _delay_ms(3);
    ADCSRA |= (1 << ADSC); // start conversion
}