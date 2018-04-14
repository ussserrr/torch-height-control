#include <ADC.h>


void adc_init(void) {
	// enable ADC, prescaler=128
	ADCSRA |= (1<<ADEN) | (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
	// AVCC with external capacitor at AREF pin
	ADMUX |= (1<<REFS0);
}


uint16_t adc_read(uint8_t adc_channel) {
	// reset current channel
	ADMUX &= 0xF0;
	// set new channel
	ADMUX |= adc_channel;

	// start conversion
	ADCSRA |= (1<<ADSC);
	// wait for completing
	while ( !(ADCSRA & (1<<ADIF)) ) {}
	// clear interrupt flag
	ADCSRA |= (1<<ADIF);

	// register containing result
	return ADC;
}
