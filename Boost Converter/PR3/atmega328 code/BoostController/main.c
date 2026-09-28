/*
 * BoostController.c
 *
 * Created: 28/09/2026 5:57:41 pm
 * Author : tmar904
 */ 

#include <avr/io.h>
#include <util/delay.h>
#define F_CPU 16000000UL
#include <stdint.h>
#include <stdlib.h>
#include <avr/interrupt.h>

#define  VOLTAGE_KP 2.0f
#define  VOLTAGE_KI 10.0f
#define T_SAMPLE (1.0f / 1000.0f)

static float int_out = 0.0f;


void adc_init(void){
	ADMUX = (1 << REFS0); //reference AVCC
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); //ADC on, 128
}

uint16_t adc_read(void){
	ADCSRA |= (1 << ADSC); //convert start
	while (ADCSRA & (1 << ADSC)){} //wait till convert finish
	return ADC;
}

volatile uint8_t control_due = 0;

ISR(TIMER1_COMPA_vect)
{
	control_due = 1;
}

void timer1_init(void){
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS10);
	OCR1A = 15999;
	TIMSK1 = (1 << OCIE1A);
}

int main(void)
{
	DDRD |= (1<<DDD5);  //set PD5 ooutput
	TCCR0A = (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);
	TCCR0B = (1 << WGM02) | (1 << CS00); //Timer 0 fast PWM
	OCR0A = 159; //100kHZ
	OCR0B = 80; //50 dutycycle
	
	adc_init();
	timer1_init();
	sei();
	
	while(1){
		
		uint16_t value = adc_read();
		uint16_t adc_mV = ((uint32_t)value * 5000UL) / 1024UL;
		uint16_t vout_mV = adc_mV *2;
		int32_t error_mV = (int32_t)vout_mV - 5000;
		float prop_out = (float)error_mV * VOLTAGE_KP;
		
		int_out += (float)error_mV * VOLTAGE_KI * T_SAMPLE;
		float pi_out = prop_out + int_out;
		
		_delay_ms(500);
	}
	
}

