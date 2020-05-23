#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sfr_defs.h>

#define F_CPU 1000000
#define BAUD 9600

#include <util/setbaud.h>

#include "chars.h"
#include "chars_human.h"

enum modes
{
	MODE_TIME = 'D',
	MODE_SET_HOURS = 'H',
	MODE_SET_MINUTES = 'M',
	MODE_TUNE = 'T',
	MODE_ERROR = 'E'
};

unsigned const char *digits = digits_paw;
unsigned char char_sep = CHAR_P_SEP;
unsigned char char_bar = CHAR_P_BAR;
unsigned char char_error = CHAR_P_ERROR;

const unsigned int base10[] = {1, 10, 100, 1000, 10000};

enum modes mode = MODE_ERROR;
bool running = false;

unsigned int pulses_per_minute = 440 * 60;
unsigned int pulses_per_second = 440;

char seconds = 0;
char minutes = 0;
char hours = 0;

char command;
char rec_byte;
char rec_buf[9];

bool echo = false;

bool send_time = false;
bool send_time_auto = false;

bool input_button = false;
bool input_rotary = false;

unsigned char input_timeout = 0;
unsigned int tune_timeout = 0;

unsigned char second_pulse_time = 0;

void shift_out(unsigned char value)
{
	PORTB &= ~_BV(5);

	USIDR = value;
	USISR = _BV(USIOIF);
	do
	{
		USICR = _BV(USIWM0) | _BV(USICS1) | _BV(USICLK) | _BV(USITC);
	} while (!bit_is_set(USISR, USIOIF));

	PORTB |= _BV(5);
}

void serial_tx(char *str)
{
	for (unsigned char i = 0; i < strlen(str); i++)
	{
		while (!bit_is_set(UCSRA, UDRE))
			;
		UDR = str[i];
		while (!bit_is_set(UCSRA, TXC))
			;
	}
}

ISR(TIMER0_COMPA_vect)
{
	static unsigned char display = 0;

	static bool clock_last = false;
	static unsigned char clock_timeout = 0;

	static unsigned char blink = 0;

	if (input_timeout > 0)
	{
		PORTA |= _BV(1);

		if (!input_button && !input_rotary)
			input_timeout--;
	}
	else
	{
		PORTA &= ~_BV(1);
	}

	if (second_pulse_time > 0)
		second_pulse_time--;
	else
		PORTD |= _BV(6);

	if (input_button && (mode != MODE_TUNE))
	{
		if (tune_timeout > 0)
			tune_timeout--;
		else
			mode = MODE_TUNE;
	}

	bool clock = bit_is_set(PIND, 5);

	if (clock != clock_last)
	{
		if (clock_timeout < 10)
			clock_timeout++;
		else
			PORTA |= _BV(0);

		clock_last = clock;
	}
	else
	{
		if (clock_timeout > 0)
		{
			clock_timeout--;
		}
		else
		{
			PORTA &= ~_BV(0);
			running = false;

			if (mode == MODE_TIME)
				mode = MODE_ERROR;
		}
	}

	blink++;
	display++;

	display %= 5;

	unsigned char segments = 0x00;

	if ((mode == MODE_TIME) || (mode == MODE_SET_HOURS) || (mode == MODE_SET_MINUTES))
	{
		switch (mode)
		{
		case MODE_TIME:
			if (seconds >= 10 * (display + 1))
				segments |= char_bar;
			break;

		case MODE_SET_HOURS:
			if ((display < 2) && !input_button)
				segments |= char_bar;
			break;

		case MODE_SET_MINUTES:
			if ((display > 2) && !input_button)
				segments |= char_bar;
			break;

		default:
			break;
		}

		switch (display)
		{
		case 0:
			segments |= digits[hours / 10];
			break;

		case 1:
			segments |= digits[hours % 10];
			break;

		case 2:
			if (((mode == MODE_TIME) && ((seconds & 0x01) == 0)) || ((mode != MODE_TIME) && (blink & 0b10000000)))
				segments |= char_sep;

			break;

		case 3:
			segments |= digits[minutes / 10];
			break;

		case 4:
			segments |= digits[minutes % 10];
			break;

		default:
			break;
		}
	}
	else if (mode == MODE_TUNE)
	{
		segments = digits[(pulses_per_minute / base10[4 - display]) % 10];
	}
	else
	{
		if (blink & 0b10000000)
			segments |= char_error;
	}

	PORTB &= 0b11100000;

	shift_out(segments);

	PORTB |= _BV(display);
}

ISR(TIMER1_COMPA_vect)
{
	// Clear comparator B interrupt
	TIFR |= _BV(OCF1B);

	OCR1B = pulses_per_second;

	if (!running)
		return;

	seconds = 0;
	second_pulse_time = 250;
	PORTD &= ~_BV(6);
	send_time |= send_time_auto;

	minutes++;

	if (minutes >= 60)
	{
		minutes = 0;
		hours++;
	}

	if (hours >= 24)
		hours = 0;
}

ISR(TIMER1_COMPB_vect)
{
	OCR1B += pulses_per_second;

	if (!running)
		return;

	seconds++;

	if (seconds < 60)
	{
		second_pulse_time = 100;
		PORTD &= ~_BV(6);
		send_time |= send_time_auto;
	}
}

ISR(INT0_vect)
{
	input_button = bit_is_clear(PIND, 2);

	if ((!input_button) || (input_timeout > 0))
		return;

	input_timeout = 100;
	tune_timeout = 2500;

	switch (mode)
	{
	case MODE_TIME:
	case MODE_ERROR:
		mode = MODE_SET_HOURS;
		break;

	case MODE_SET_HOURS:
		mode = MODE_SET_MINUTES;
		break;

	case MODE_SET_MINUTES:
		if (!running)
		{
			seconds = 0;
			TCNT1 = 0;
			OCR1B = pulses_per_second;
			running = true;
		}
		mode = MODE_TIME;
		break;

	case MODE_TUNE:
		OCR1A = pulses_per_minute;
		eeprom_update_word(0x00, pulses_per_minute);
		mode = MODE_TIME;
		break;

	default:
		break;
	}
}

ISR(INT1_vect)
{
	input_rotary = bit_is_clear(PIND, 3);

	if ((!input_rotary) || (input_timeout > 0))
		return;

	input_timeout = 25;

	char direction = bit_is_clear(PIND, 4) ? 1 : -1;

	switch (mode)
	{
	case MODE_SET_HOURS:
		hours += direction;
		if (hours < 0)
			hours = 23;
		if (hours >= 24)
			hours = 0;
		break;

	case MODE_SET_MINUTES:
		running = false;
		minutes += direction;
		if (minutes < 0)
			minutes = 59;
		if (minutes >= 60)
			minutes = 0;
		break;

	case MODE_TUNE:
		pulses_per_minute += direction;
		pulses_per_second = pulses_per_minute / 60;
		break;

	default:
		break;
	}
}

ISR(USART_RX_vect)
{
	rec_byte = UDR;

	if (!command)
	{
		if (isupper(rec_byte))
			command = rec_byte;
	}
	else
	{
		if ((isprint(rec_byte)) && (strlen(rec_buf) < sizeof(rec_buf) - 1))
			rec_buf[strlen(rec_buf)] = rec_byte;
	}

	if (echo)
	{
		while (!bit_is_set(UCSRA, UDRE))
			;
		UDR = rec_byte;
		while (!bit_is_set(UCSRA, TXC))
			;
	}
}

int main(void)
{
	// Set up ports
	DDRA = 0b00000011;
	PORTA = 0b00000000;

	DDRB = 0b11111111;
	PORTB = 0b00000000;

	DDRD = 0b01000010;
	PORTD = 0b01011100;

	// Read clock calibration from EEPROM
	unsigned int eeval = eeprom_read_word(0x00);

	if (eeval != 0xffff)
	{
		pulses_per_minute = eeval;
		pulses_per_second = pulses_per_minute / 60;
	}
	else
	{
		eeprom_update_word(0x00, pulses_per_minute);
	}

	// Set up timer 0 for display refresh
	TCCR0A = _BV(WGM01);
	TCCR0B = _BV(CS00) | _BV(CS01);
	OCR0A = 12;

	// Set up timer 1 for timekeeping
	TCCR1B = _BV(CS10) | _BV(CS11) | _BV(CS12) | _BV(WGM12);
	OCR1A = pulses_per_minute;
	OCR1B = pulses_per_second;

	TIMSK = _BV(OCIE0A) | _BV(OCIE1A) | _BV(OCIE1B);

	// Enable interrupts on Int0 and Int1 pins for user input
	MCUCR |= _BV(ISC00) | _BV(ISC10);
	GIMSK |= _BV(INT0) | _BV(INT1);

	// Set up serial port
	UBRRH = UBRRH_VALUE;
	UBRRL = UBRRL_VALUE;

#if USE_2X
	UCSRA |= (1 << U2X);
#else
	UCSRA &= ~(1 << U2X);
#endif

	UCSRB = _BV(TXEN) | _BV(RXEN) | _BV(RXCIE);
	UCSRC = _BV(USBS) | (3 << UCSZ0);

	// Disable analague comparator
	ACSR |= _BV(ACD);

	sei();

	set_sleep_mode(SLEEP_MODE_IDLE);
	while (1)
	{
		if (command && ((rec_byte == '\n') || (rec_byte == '\r')))
		{
			switch (command)
			{
			case 'E':
				if ((rec_buf[0] == '0') || (rec_buf[0] == '1'))
				{
					echo = rec_buf[0] == '1';
					serial_tx("=OK\n\r");
				}
				else
				{
					serial_tx("=ERR:INV\n\r");
				}

				break;

			case 'A':
				if ((rec_buf[0] == '0') || (rec_buf[0] == '1'))
				{
					send_time_auto = rec_buf[0] == '1';
					serial_tx("=OK\n\r");
				}
				else
				{
					serial_tx("=ERR:INV\n\r");
				}

				break;

			case 'C':
				if ((rec_buf[0] == '0') || (rec_buf[0] == '1'))
				{
					if (rec_buf[0] == '0')
					{
						digits = digits_paw;
						char_sep = CHAR_P_SEP;
						char_bar = CHAR_P_BAR;
						char_error = CHAR_P_ERROR;
					}
					else
					{
						digits = digits_human;
						char_sep = CHAR_H_SEP;
						char_bar = CHAR_H_BAR;
						char_error = CHAR_H_ERROR;
					}

					serial_tx("=OK\n\r");
				}
				else
				{
					serial_tx("=ERR:INV\n\r");
				}

				break;

			case 'T':
				if (rec_buf[0] == '?')
				{
					send_time = true;
					serial_tx("=OK\n\r");
				}
				else
				{
					char time[2];
					char *tok = rec_buf;
					unsigned char ti = 0;

					for (unsigned char i = 0; i <= strlen(rec_buf); i++)
					{
						if ((rec_buf[i] == ':') || (rec_buf[i] == '\0'))
						{
							time[ti++] = atoi(tok);

							if (ti >= 2)
								break;

							tok = rec_buf + i + 1;
						}
					}

					if ((ti == 2) && (time[0] >= 0) && (time[0] < 24) && (time[1] >= 0) && (time[1] < 60))
					{
						hours = time[0];
						minutes = time[1];
						seconds = 0;

						TCNT1 = 0;
						OCR1B = pulses_per_second;
						running = true;

						mode = MODE_TIME;
						serial_tx("=OK\n\r");
					}
					else
					{
						char str[6];
						itoa(ti, str, 10);

						serial_tx(str);
						serial_tx("\r\n");
						serial_tx("=ERR:INV\n\r");
					}
				}

				break;

			case 'P':
				if (rec_buf[0] == '?')
				{
					serial_tx("=OK\n\r");

					char str[6];
					itoa(pulses_per_minute, str, 10);

					serial_tx(str);
					serial_tx("\r\n");
				}
				else
				{
					unsigned int val = atol(rec_buf);

					if (val > 0)
					{
						pulses_per_minute = val;
						pulses_per_second = pulses_per_minute / 60;
						OCR1A = pulses_per_minute;
						eeprom_update_word(0x00, pulses_per_minute);

						serial_tx("=OK\n\r");
					}
					else
					{
						serial_tx("=ERR:INV\n\r");
					}
				}

				break;

			case 'M':
				if (rec_buf[0] == '?')
				{
					serial_tx("=OK\n\r");

					char str[2];
					str[0] = mode;
					str[1] = '\0';

					serial_tx(str);

					serial_tx("\r\n");
				}
				else
				{
					serial_tx("=ERR:INV\n\r");
				}

				break;

			case 'R':
				if (rec_buf[0] == '?')
				{
					serial_tx("=OK\n\r");

					char str[2];
					str[0] = running + '0';
					str[1] = '\0';

					serial_tx(str);

					serial_tx("\r\n");
				}
				else if (rec_buf[0] == '1')
				{
					seconds = 0;

					TCNT1 = 0;
					OCR1B = pulses_per_second;
					running = true;

					mode = MODE_TIME;

					serial_tx("=OK\n\r");
				}
				else
				{
					serial_tx("=ERR:INV\n\r");
				}

				break;

			default:
				serial_tx("=ERR:UC\n\r");
			}

			command = '\0';
			rec_byte = '\0';
			memset(rec_buf, '\0', sizeof(rec_buf));
		}

		if (send_time)
		{
			send_time = false;

			char str[4];

			itoa(hours, str, 10);
			serial_tx(str);

			serial_tx(":");

			itoa(minutes, str, 10);
			serial_tx(str);

			serial_tx(":");

			itoa(seconds, str, 10);
			serial_tx(str);

			serial_tx("\r\n");
		}

		sleep_mode();
	}
}
