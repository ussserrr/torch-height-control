#include <MotorDriver.h>


// Motor timer initialization
void motor_init(void) {
    MOTOR_DRIVER_DDR |= (1<<STEP_PIN)|(1<<DIR_PIN);
    // period
    OCR2A = 186;
    // CTC mode
    TCCR2A |= (1<<WGM21);
    // set prescaler to 128 (remove CS20 for 64) and start the timer
    TCCR2B |= (1<<CS22)|(1<<CS20);
}


// ISR for timer for motor
ISR (TIMER2_COMPA_vect) {
    MOTOR_DRIVER_PORT |= (1<<STEP_PIN);
    _delay_us(PULSE);
    MOTOR_DRIVER_PORT &= ~(1<<STEP_PIN);
}


void motor_up(void) {
    MOTOR_DRIVER_PORT |= (1<<DIR_PIN);
    // enable interrupt for motor timer
    TIMSK2 |= (1<<OCIE2A);
}


void motor_down(void) {
    MOTOR_DRIVER_PORT &= ~(1<<DIR_PIN);
    // enable interrupt for motor timer
    TIMSK2 |= (1<<OCIE2A);
}


// Make ±steps steps in one or another direction
void motor_move(int16_t steps) {
    if (steps > 0)
        MOTOR_DRIVER_PORT |= (1<<DIR_PIN);
    else if (steps < 0)
        MOTOR_DRIVER_PORT &= ~(1<<DIR_PIN);

    int16_t steps_cnt;
    for (steps_cnt=0; steps_cnt<abs(steps); steps_cnt++) {
        MOTOR_DRIVER_PORT |= (1<<STEP_PIN);
        _delay_us(PULSE);
        MOTOR_DRIVER_PORT &= ~(1<<STEP_PIN);
        _delay_us(PERIOD);
    }

    MOTOR_STOP;
}


void motor_stop(void) {
    // disable interrupt for motor timer
    TIMSK2 &= ~(1<<OCIE2A);
    MOTOR_STOP;
}
