#ifndef ADC_H_
#define ADC_H_



#include <avr/io.h>

// ADC pinout
#define ADC_SETTINGS_PIN 0  // PC0
#define ADC_FEEDBACK_PIN 1  // PC1

// edit this value to match your voltage
#define ADC_REFERENCE_VOLTAGE 5.00


void adc_init(void);
uint16_t adc_read(uint8_t adc_channel);



#endif /* ADC_H_ */
