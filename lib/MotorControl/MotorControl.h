#ifndef MOTORCONTROL_H_
#define MOTORCONTROL_H_



#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>


/*
 *  Motor definitions. We've used 42BYGHW208 stepper motor with such pinout:
 *    First coil:
 *      - A black
 *      - C green
 *    Second coil:
 *      - B red
 *      - D blue
 */
#define MOTOR_DDR DDRC
#define MOTOR_PORT PORTC
#define MOTOR_PHASE_A PC2
#define MOTOR_PHASE_C PC3
#define MOTOR_PHASE_B PC4
#define MOTOR_PHASE_D PC5
#define MOTOR_PINS_OFFSET 2  // 2 for PC2. Needed to iterate through the pins
#define MOTOR_STOP MOTOR_PORT&=(~((1<<MOTOR_PHASE_A)|(1<<MOTOR_PHASE_C)|(1<<MOTOR_PHASE_B)|(1<<MOTOR_PHASE_D)))
#define MOTOR_PULSE_TIME 1500  // in microseconds


void motor_move(int16_t steps);
void motor_up(void);
void motor_down(void);
void motor_stop(void);
void motor_init(void);



#endif /* MOTORCONTROL_H_ */
