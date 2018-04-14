#include <MotorControl.h>


static bool motor_up_flag;
static int8_t last_step = 0;


// Motor timer initialization. We use Timer2 for motor movement algorithm
void motor_init(void) {
	MOTOR_DDR |= (1<<MOTOR_PHASE_A)|(1<<MOTOR_PHASE_B)|(1<<MOTOR_PHASE_C)|(1<<MOTOR_PHASE_D);
	/*
	 *  3000us pulse duration: NOT CONFIRMED (~167Hz on one motor channel,
	 *  1.5ms pulse duration, 6ms for 4 steps) (though datasheet confirmed)
	 */
	OCR2A = 186;
	// CTC mode
	TCCR2A |= (1<<WGM21);
	// set prescaler to 128 (remove CS20 for 64) and start the timer
	TCCR2B |= (1<<CS22)|(1<<CS20);
}


// Make ±steps steps in one or another direction
void motor_move(int16_t steps) {
	int16_t steps_cnt;

	if (steps > 0) {
		for (steps_cnt=0; steps_cnt<steps; steps_cnt++) {
			MOTOR_PORT = 1 << (last_step+MOTOR_PINS_OFFSET);
			_delay_us(MOTOR_PULSE_TIME);

			if (++last_step == 4) last_step = 0;
		}
	}

	else if (steps < 0) {
		for (steps_cnt=steps; steps_cnt<0; steps_cnt++) {
			MOTOR_PORT = 1 << (last_step+MOTOR_PINS_OFFSET);
			_delay_us(MOTOR_PULSE_TIME);

			if (--last_step == -1) last_step = 3;
		}
	}

	MOTOR_STOP;
}


// ISR for timer for motor
ISR (TIMER2_COMPA_vect) {
	MOTOR_PORT = 1 << (last_step+MOTOR_PINS_OFFSET);

	if (motor_up_flag) {
		if (++last_step == 4) last_step = 0;
	}
	else {
		if (--last_step == -1) last_step = 3;
	}
}


void motor_up(void) {
	motor_up_flag = true;
	// Enable interrupt for motor timer
	TIMSK2 |= (1<<OCIE2A);
}


void motor_down(void) {
	motor_up_flag = false;
	// Enable interrupt for motor timer
	TIMSK2 |= (1<<OCIE2A);
}


void motor_stop(void) {
	// Disable interrupt for motor timer
	TIMSK2 &= ~(1<<OCIE2A);
	MOTOR_STOP;
}
