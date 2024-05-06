/*************************************************************************
Title:    XBee based AC switch
Authors:  Michael Petersen <railfan@drgw.net>
File:     CKT-XBEEIO-ACSW
License:  GNU General Public License v3

LICENSE:
    Copyright (C) 2024 Michael Petersen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*************************************************************************/

#include <stdlib.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>

typedef enum
{
	STATE_IDLE          = 0x00,
} AcswState;

AcswState state = STATE_IDLE;


// ******** Start 100 Hz Timer, 0.16% error version (Timer 0)
// If you can live with a slightly less accurate timer, this one only uses Timer 0, leaving Timer 1 open
// for more advanced things that actually need a 16 bit timer/counter

// Initialize a 100Hz timer for use in triggering events.
// If you need the timer resources back, you can remove this, but I find it
// rather handy in triggering things like periodic status transmissions.
// If you do remove it, be sure to yank the interrupt handler and ticks/secs as well
// and the call to this function in the main function

volatile uint8_t ticks;
volatile uint16_t decisecs;

void initialize100HzTimer(void)
{
	// Set up timer 1 for 100Hz interrupts
	TCNT0 = 0;
	//OCR0A = 0xC2;
	OCR0A = 0x6C; // Appropriate for 11.0592 MHz
	ticks = 0;
	decisecs = 0;
	TCCR0A = _BV(WGM01);
	TCCR0B = _BV(CS02) | _BV(CS00);
	TIMSK0 |= _BV(OCIE0A);
}

ISR(TIMER0_COMPA_vect)
{
	if (++ticks >= 10)
	{
		ticks = 0;
		decisecs++;
	}
}

// End of 100Hz timer

uint8_t readInput(uint8_t input)
{
	uint8_t inputPin;
	switch(input)
	{
		case 0:
			inputPin = PB0;
			break;
		case 1:
			inputPin = PB1;
			break;
		case 2:
			inputPin = PB2;
			break;
		default:
			return 0;
	}
	return(PINB & _BV(inputPin));
}

void setLed(uint8_t led, uint8_t val)
{
	if(led > 2)
		return;
	if(val)
		PORTD |= _BV(7-led);
	else
		PORTD &= ~(_BV(7-led));
}

void setRelay(uint8_t relay, uint8_t val)
{
	uint8_t setPin, resetPin;
	switch(relay)
	{
		case 0:
			setPin = PC1;
			resetPin = PC0;
			break;
		case 1:
			setPin = PC2;
			resetPin = PC3;
			break;
		case 2:
			setPin = PC4;
			resetPin = PC5;
			break;
		default:
			return;
	}
	if(val)
	{
		setLed(relay,1);
		PORTC |= _BV(setPin);
		wdt_reset();
		_delay_ms(150);
		PORTC &= ~(_BV(setPin));
	}
	else
	{
		setLed(relay,0);
		PORTC |= _BV(resetPin);
		wdt_reset();
		_delay_ms(150);
		PORTC &= ~(_BV(resetPin));
	}
}

void init(void)
{
	// Kill watchdog
	MCUSR = 0;

	wdt_reset();
	WDTCSR |= _BV(WDE) | _BV(WDCE);
	WDTCSR = _BV(WDE) | _BV(WDP2) | _BV(WDP1); // Set the WDT to system reset and 1s timeout
	wdt_reset();

	PORTC = 0;
	DDRC = _BV(PD0) | _BV(PD1) | _BV(PD2) | _BV(PD3) | _BV(PD4) | _BV(PD5);
	PORTD = 0;
	DDRD = _BV(PD5) | _BV(PD6) | _BV(PD7);
}

int main(void)
{
	uint8_t oldVal[3] = {0, 0, 0};
	uint8_t i;
	uint8_t val;
	// Application initialization
	init();

	// Initialize a 100 Hz timer.  See the definition for this function - you can
	// remove it if you don't use it.
	initialize100HzTimer();

	// Do something at startup and also give the XBee time to receive data from the switch
	wdt_reset();
	_delay_ms(750);

	setLed(0,1);
	wdt_reset();
	_delay_ms(500);
	setLed(0,0);

	setLed(1,1);
	wdt_reset();
	_delay_ms(500);
	setLed(1,0);

	setLed(2,1);
	wdt_reset();
	_delay_ms(500);
	setLed(2,0);

	setLed(0,1);
	setLed(1,1);
	setLed(2,1);
	wdt_reset();
	_delay_ms(750);
	setLed(0,0);
	setLed(1,0);
	setLed(2,0);

	wdt_reset();
	_delay_ms(500);
	wdt_reset();

	// Read input pins and pre-configure relays to match
	for(i=0; i<3; i++)
	{
		wdt_reset();
		oldVal[i] = readInput(i);
		setRelay(i, oldVal[i]);
	}

	while (1)
	{
		for(i=0; i<3; i++)
		{
			wdt_reset();
			val = readInput(i);
			if(val != oldVal[i])
			{
				setRelay(i, val);
				oldVal[i] = val;
			}
		}
	}
}
