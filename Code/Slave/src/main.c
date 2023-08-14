#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <LCD.h>

char user_try[100] = "";
char user_try_hide[100] = "";
int user_try_index = 0;
int can_see = 1; // 0: hide, 1: show
char ADC_val[20] = "";
int ADC_val_index = 0;
int checker = 0;
int cooler_dc = 0;
int heater_dc = 0;
int blink = 0;
int LDR_dc = 0;

char *stringToHex(const char *str)
{
    size_t len = strlen(str);
    char *hexStr = (char *)malloc(len * 2 + 1); // Each character requires 2 hex digits + 1 for the null terminator

    if (hexStr == NULL)
    {
        // Error in memory allocation
        return NULL;
    }

    for (size_t i = 0; i < len; i++)
    {
        sprintf(hexStr + i * 2, "%02X", str[i]);
    }

    hexStr[len * 2] = '\0'; // Add null terminator

    return hexStr;
}

char answer[6] = {0x31, 0x39, 0x31, 0x39, 0x00}; // Hexadecimal representation of "1919"
const char key = 0x0A;                           // XOR encryption key

void encryptPassword()
{
    for (int i = 0; i < strlen(answer); i++)
    {
        answer[i] = answer[i] ^ key;
    }
}

void checking_password(int user_try_length)
{
    char correct_password[20] = "Access is granted";
    char wrong_password[20] = "Wrong password";

    LCD_cmd(0x01); // clear the screen
    encryptPassword();
    for (int i = 0 < user_try_length; i++)
    {
        user_try[i] = user_try[i] ^ key;
    }
    // if password wrong
    if (user_try_length != strlen(answer))
    {
        for (int j = 0; wrong_password[j]; j++)
        {
            LCD_write(wrong_password[j]);
        }
        LCD_cmd(0x01); // clear the screen
        return;
    }
    else if (user_try_length == strlen(answer))
    {
        for (int i = 0; i < user_try_length; i++)
        {
            if (user_try[i] != answer[i])
            {
                for (int j = 0; wrong_password[j]; j++)
                {
                    LCD_write(wrong_password[j]);
                }
                LCD_cmd(0x01); // clear the screen
                return;
            }
        }
    }

    // if password correct
    for (int i = 0; correct_password[i]; i++)
    {
        LCD_write(correct_password[i]);
    }
    checker = 1;
    // write ADC_val to LCD
    _delay_ms(100);
    LCD_cmd(0x01); // clear the screen
    for (int i = 0; i < strlen(ADC_val); i++)
    {
        LCD_write(ADC_val[i]);
    }
    return;
}

void LDR_motor(char LDR_value)
{
    if (LDR_value <= 25)
    {
        LDR_dc = 100.0;
    }
    else if (LDR_value <= 50)
    {
        LDR_dc = 75.0;
    }
    else if (LDR_value <= 75)
    {
        LDR_dc = 50.0;
    }
    else if (LDR_value <= 100)
    {
        LDR_dc = 25.0;
    }
}

int main()
{
    DDRC = (1 << DDC0) | (1 << DDC1) | (1 << DDC2) | (1 << DDC3) | (1 << DDC4) | (1 << DDC5) | (1 << DDC6) | (1 << DDC7);
    DDRB = (0 << DDB7) | (1 << DDB6) | (0 << DDB5) | (0 << DDB4) | (1 << DDB1) | (1 << DDB0);
    DDRD = (1 << DDD0) | (1 << DDD1) | (1 << DDD2) | (1 << DDD4) | (1 << DDD5) | (1 << DDD7);
    DDRA = (0 << DDA0);

    SPCR = (1 << SPIE) | (1 << SPE) | (0 << DORD) | (0 << MSTR) | (0 << CPOL) | (0 << CPHA) | (1 << SPR1) | (1 << SPR0);
    SPSR = (0 << SPI2X);

    // set timer counter 1 to be in fast PWM checker, non-inverting checker
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10);
    TCCR1B = (1 << WGM12) | (1 << CS11);

    TCCR2 = (1 << WGM20) | (1 << WGM21) | (1 << COM21) | (1 << CS21); // 128

    // set PWM pin as non-inverting
    TCCR1A &= ~(1 << COM1A0);
    TCCR1A &= ~(1 << COM1B0);
    TCCR2 &= ~(1 << COM20);

    init_LCD();
    LCD_cmd(0x0f); // make blinking cursor

    sei();
    while (1)
    {
        if (blink == 1)
        {
            PORTB |= (1 << PB0);
            _delay_ms(200);
            PORTB &= ~(1 << PB0);
            _delay_ms(200);
        }
        else if (blink == 2)
        {
            PORTB |= (1 << PB1);
            _delay_ms(200);
            PORTB &= ~(1 << PB1);
            _delay_ms(200);
        }
        else if (blink == 0)
        {
            PORTB &= ~(1 << PB0);
            PORTB &= ~(1 << PB1);
        }
        _delay_ms(1);
    }
}

ISR(SPI_STC_vect)
{
    uint8_t received_data = SPDR;
    if (received_data > 100 && received_data < 200)
    {
        LCD_write('X');
        if (checker == 1)
        {
            LCD_write('T');
            LDR_motor(received_data - 100);
            OCR2 = (LDR_dc / 100.0) * 255.0;
        }
    }

    // if password is correct, checker = 1 and if password is wrong, checker = 0
    if (received_data >= 200 && checker == 0)
    {
        received_data = received_data - 200;
        if (received_data == 12)
        {
            can_see ^= 1;
            if (can_see == 1)
            {
                LCD_cmd(0x01); // clear the screen
                _delay_ms(100);
                // write entered password to LCD
                for (int i = 0; i < strlen(user_try); i++)
                {
                    LCD_write(user_try[i]);
                }
            }
            else if (can_see == 0)
            {
                LCD_cmd(0x01); // clear the screen
                _delay_ms(100);
                // write entered password to LCD
                for (int i = 0; i < strlen(user_try_hide); i++)
                {
                    LCD_write(user_try_hide[i]);
                }
            }
        }

        if (received_data != 10 && received_data != 11 && received_data != 12)
        {
            user_try[user_try_index] = received_data + '0';
            // user_try[user_try_index] = (char)received_data;
            user_try_hide[user_try_index] = '*';

            if (can_see == 0)
            {
                LCD_write('*');
            }
            else if (can_see == 1)
            {
                LCD_write(received_data + '0');
                // LCD_write(received_data);
                // LCD_write((char)received_data);
            }
            user_try_index++;
        }

        // check password
        if (received_data == 10)
        {

            LCD_cmd(0x01); // clear the screen
            _delay_ms(100);
            // write entered password to LCD
            for (int i = 0; i < strlen(user_try); i++)
            {
                LCD_write(user_try[i]);
            }
            _delay_ms(100);

            int user_try_length = strlen(user_try);
            char correct_password[20] = "Access is granted";
            char wrong_password[20] = "Wrong password";

            LCD_cmd(0x01); // clear the screen

            // if password wrong
            if (user_try_length != strlen(answer))
            {
                for (int j = 0; wrong_password[j]; j++)
                {
                    LCD_write(wrong_password[j]);
                }
                LCD_cmd(0x01); // clear the screen
                return;
            }
            else if (user_try_length == strlen(answer))
            {
                for (int i = 0; i < user_try_length; i++)
                {
                    if (user_try[i] != answer[i])
                    {
                        for (int j = 0; wrong_password[j]; j++)
                        {
                            LCD_write(wrong_password[j]);
                        }
                        LCD_cmd(0x01); // clear the screen
                        return;
                    }
                }
            }

            // if password correct
            for (int i = 0; correct_password[i]; i++)
            {
                LCD_write(correct_password[i]);
            }
            checker = 1;
            // write ADC_val to LCD
            _delay_ms(100);
            LCD_cmd(0x01); // clear the screen
            for (int i = 0; i < strlen(ADC_val); i++)
            {
                LCD_write(ADC_val[i]);
            }
            _delay_ms(100);

            for (int i = 0; i < user_try_length; i++)
            {
                user_try[i] = '\0';
                user_try_hide[i] = '\0';
            }
            user_try_index = 0;
        }
        else if (received_data == 11)
        {
            // remove last character from user_try
            if (user_try_index > 0)
            {
                user_try_index--;
                user_try[user_try_index] = '\0';
                user_try_hide[user_try_index] = '\0';
                LCD_cmd(0x10);  // move cursor left
                LCD_write(' '); // remove last character from LCD
                LCD_cmd(0x10);  // move cursor left
            }
        }
    }
    else if (received_data < 100)
    {
        // convert received_data to string and add it to ADC_val
        // sprintf(ADC_val, "%d", received_data);
        itoa(received_data, ADC_val, 10);
        int temperature = received_data;
        if (checker == 1)
        {
            if (temperature >= 25 && temperature <= 55)
            {
                cooler_dc = 50.0 + ((temperature - 25) / 5) * 10;
                heater_dc = 0.0;
            }
            else if (temperature >= 3 && temperature <= 20)
            {
                cooler_dc = 0.0;
                heater_dc = 100.0 - ((temperature - 0) / 5) * 25;
            }
            else if (temperature > 55)
            {
                cooler_dc = 0.0;
                heater_dc = 0.0;
            }
            else if (temperature < 3)
            {
                cooler_dc = 0.0;
                heater_dc = 0.0;
            }
            if (temperature >= 25 && temperature <= 55)
            {
                cooler_dc = 50.0 + ((temperature - 25) / 5) * 10;
                heater_dc = 0.0;
            }
            else if (temperature >= 3 && temperature <= 20)
            {
                cooler_dc = 0.0;
                heater_dc = 100.0 - ((temperature - 0) / 5) * 25;
            }
            else if (temperature > 55)
            {
                cooler_dc = 0.0;
                heater_dc = 0.0;
            }
            else if (temperature < 3)
            {
                cooler_dc = 0.0;
                heater_dc = 0.0;
            }
            OCR1A = (cooler_dc / 100.0) * 255.0;
            OCR1B = (heater_dc / 100.0) * 255.0;

            if (received_data > 55)
            {
                blink = 1;
            }
            else if (received_data < 3)
            {
                blink = 2;
            }
            else
            {
                blink = 0;
            }

            // write ADC_val to LCD
            _delay_ms(100);
            LCD_cmd(0x01); // clear the screen
            for (int i = 0; i < strlen(ADC_val); i++)
            {
                LCD_write(ADC_val[i]);
            }
        }
    }
}