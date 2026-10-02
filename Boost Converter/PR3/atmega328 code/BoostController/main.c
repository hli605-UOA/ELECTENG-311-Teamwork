#include <avr/io.h>              // Gives access to ATmega328P registers and pins
#include <stdint.h>              // Gives fixed-size data types like uint16_t and int32_t
#include <avr/interrupt.h>       // Lets us use interrupts such as Timer1 interrupt

#define F_CPU 16000000UL         // MCU clock is 16 MHz

#define VOLTAGE_KP 0.2f          // KP
#define VOLTAGE_KI 1.0f         // KI
#define T_SAMPLE (1.0f / 1000.0f) // Controller sample time = 1 ms

#define VREF_MV 5000             // Target output voltage = 5000 mV = 5 V
#define PWM_TOP 159              // Timer0 counts from 0 to 159 for 100 kHz PWM

#define PI_MIN_MV 1000.0f        // Minimum PI output = 1000 mV = 1 V
#define PI_MAX_MV 4000.0f        // Maximum PI output = 4000 mV = 4 V

static float int_out = 2500.0f;  // Stores the integral value, starts at 2.5 V

volatile uint8_t control_due = 0; // Flag used to tell main loop when PI control should run



void adc_init(void)
{
	ADMUX = (1 << REFS0);        // Use AVCC as the ADC reference voltage 

	ADCSRA =
	(1 << ADEN)              // Turn on the ADC
	| (1 << ADPS2)           // 
	| (1 << ADPS1)           // 
	| (1 << ADPS0);          // prescaler = 128
}



uint16_t adc_read(void)
{
	ADCSRA |= (1 << ADSC);       // Start one ADC conversion

	while (ADCSRA & (1 << ADSC)) // Keep waiting while ADC is still converting
	{
	}

	return ADC;                  // Return the ADC result, from 0 to 1023
}



void timer1_init(void)
{
	TCCR1A = 0;                  // Clear Timer1 control register A

	TCCR1B =
	(1 << WGM12)             // CTC mode
	| (1 << CS10);           // Run Timer1 with no prescaler

	OCR1A = 15999;               // Make Timer1 interrupt every 1 ms at 16 MHz

	TIMSK1 = (1 << OCIE1A);      // Enable Timer1 Compare Match A interrupt
}



void pwm_init(void)
{
	DDRD |= (1 << DDD5);         // Set PD5 as an output pin

	TCCR0A =
	(1 << COM0B1)            // Output PWM signal on OC0B, which is PD5
	| (1 << WGM01)           // Fast PWM mode setting
	| (1 << WGM00);          // Fast PWM mode setting

	TCCR0B =
	(1 << WGM02)             // Use OCR0A as the TOP value
	| (1 << CS00);           // Run Timer0 with no prescaler

	OCR0A = PWM_TOP;             // Set PWM period, TOP = 159

	OCR0B = 80;                  // Start with about 50% duty cycle
}



void set_pi_pwm(float pi_mV)
{
	if (pi_mV < PI_MIN_MV)      
	pi_mV = PI_MIN_MV;       // Limit it to 1 V

	if (pi_mV > PI_MAX_MV)       
	pi_mV = PI_MAX_MV;       // Limit it to 4 V

	float duty =
	pi_mV / 5000.0f;         // Convert PI voltage into PWM duty ratio

	OCR0B =
	(uint8_t)(duty * 160.0f);    // Convert duty ratio into Timer0 compare value
}



void pi_controller(void)
{
	uint16_t adc_value =
	adc_read();              // Read the voltage from ADC0

	uint16_t adc_mV =
	((uint32_t)adc_value * 5000UL)
	/ 1024UL;                // Convert ADC number into millivolts

	uint16_t vout_mV =
	adc_mV * 2;              // Undo the 1:2 voltage divider to estimate real Vout

	int32_t error_mV = (int32_t)vout_mV - VREF_MV;               
	// Calculate error = measured Vout - target Vref
	// inverting calculation so it fit UC3843

	float prop_out = (float)error_mV * VOLTAGE_KP;       // Calculate proportional part: P = error × Kp

	float new_int = int_out + (float)error_mV * VOLTAGE_KI * T_SAMPLE;  // Calculate the new integral value

	float new_pi = prop_out + new_int;      // Calculate the new PI output

	if (new_pi >= PI_MIN_MV && new_pi <= PI_MAX_MV)     
	{
		int_out = new_int;       // Only update the integral if output is not saturated
	}

	float pi_out = prop_out + int_out;      // Final PI output = proportional + integral

	if (pi_out > PI_MAX_MV)      
	pi_out = PI_MAX_MV;      // Limit it to 4 V

	if (pi_out < PI_MIN_MV)      
	pi_out = PI_MIN_MV;      // Limit it to 1 V

	set_pi_pwm(pi_out);          // Convert PI output into PWM duty cycle
}


ISR(TIMER1_COMPA_vect)
{
	control_due = 1;             // Tell the main loop it is time to run PI control
}


int main(void)
{
	adc_init();                  
	pwm_init();                  
	timer1_init();               
	sei();                       

	while (1)                    
	{
		if (control_due)         // Check if 1 ms has passed
		{
			control_due = 0;     // Clear the flag

			pi_controller();     // Run one PI controller update
		}
	}
}


//ADC?Vout
//??error
//?P
//?I
//P + I
//???1~4V
//??PWM duty
//???PD5