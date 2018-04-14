#ifndef MOTORDRIVER_H_
#define MOTORDRIVER_H_



#include <Arduino.h>


#define MOTOR_DRIVER_DDR DDRC
#define MOTOR_DRIVER_PORT PORTC

#define STEP_PIN PC3
#define DIR_PIN PC2

// in milliseconds
#define PERIOD 1500
#define PULSE 20

// for internal usage
#define MOTOR_STOP MOTOR_DRIVER_PORT&=(~((1<<STEP_PIN)|(1<<DIR_PIN)))


void motor_move(int16_t steps);
void motor_up(void);
void motor_down(void);
void motor_stop(void);
void motor_init(void);



#endif /* MOTORDRIVER_H_ */
