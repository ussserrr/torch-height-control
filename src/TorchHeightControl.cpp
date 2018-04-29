#include <TorchHeightControl.h>



/*
 *  Other settings
 */
// Lift before cut is measured in steps of stepper motor (0-1023).
// EEMEM is the default value at the flash time. Actually, not so important
// because we always can change it at runtime
uint16_t cutting_height;  // cutting_height
uint16_t EEMEM cutting_height_EEPROM = 150;
// Pierce time is measured in ELEMENTARY_DELAYs which in turn is measured in ms
uint8_t pierce_time;
#define PIERCE_TIME_ELEMENTARY_DELAY 100  // 100 ms
#define PIERCE_TIME_MAX_TIME 50  // 50*ELEMENTARY_DELAY = 5s max
uint8_t EEMEM pierce_time_EEPROM = 20;  // 20*ELEMENTARY_DELAY = 2000ms
// Flag of the "bypass mode" without regulation. We explicitly use uint8_t type
// (instead of bool) because AVR's EEPROM driver has function for that
uint8_t EEMEM bypass_ON_flag_EEPROM = 0;


/*
 *  Setpoint settings
 */
uint16_t setpoint = 0;
// setpoint will be automatically defined at regulation start
#define NUM_OF_VALUES_FOR_SETPOINT_DEFINITION 200
bool setpoint_defined = false;
// hysteresis for control algorithm (setpoint ± setpoint_offset)
uint16_t setpoint_offset;
uint16_t EEMEM setpoint_offset_EEPROM = 20;  // 97mV
// interval of voltages for specifing setpoint offset in settings menus, Volts
#define SETPOINT_OFFSET_MIN_SET_VOLTAGE 0.010
#define SETPOINT_OFFSET_MAX_SET_VOLTAGE 0.200


/*
 *  Signal from arc and control algorithm definitions. Note that we use shared
 *  feedback variables first for setpoint defining and then for main regulation
 *  algorithm itself
 */
uint16_t feedback = 0;  // 10-bit ADC value
uint16_t feedback_prev = 0;  // previous ADC value
uint32_t feedback_accum = 0;  // accumulator for averaging
uint16_t feedback_accum_cnt = 0;  // counter of num of values for averaging
uint16_t feedback_avrg = 0;
// counter for manual dividing frequency of algo' timer in its ISR
uint8_t prescaler_cnt = 0;
#define PRESCALER_MAIN_ALGO 50
// hysteresis for averaging only close results of ADC measurements
#define OFFSET_FOR_AVRG 10  // ±48mV



int main(void) {

    // disable all interrupts while setup
    cli();

    /*
     *  Debug LED
     */
    #ifdef TEST_LED
        TEST_LED_DDR |= (1<<TEST_LED_PIN);
        TEST_LED_PORT &= ~(1<<TEST_LED_PIN);
    #endif

    /*
     * LCD startup
     */
    lcd.begin(16, 2);
    lcd.clear();
    lcd.print("loading...");

    /*
     *  Retrieve values from EEPROM
     */
    setpoint_offset = eeprom_read_word(&setpoint_offset_EEPROM);
    cutting_height = eeprom_read_word(&cutting_height_EEPROM);
    pierce_time = eeprom_read_byte(&pierce_time_EEPROM);

    /*
     *  Set directions of IOs
     */
    // outputs:
    // MOTOR_DDR |= (1<<MOTOR_PHASE_A)|(1<<MOTOR_PHASE_B)|(1<<MOTOR_PHASE_C)|(1<<MOTOR_PHASE_D);
    // inputs:
    SIGNALS_DDR &= ~( (1<<SETTINGS_BUTTON_PIN) | (1<<UP_SIGNAL_PIN) | (1<<DOWN_SIGNAL_PIN) |
                      (1<<PLASM_SIGNAL_PIN) | (1<<TOUCH_SIGNAL_PIN) );

    /*
     *  Interrupts setup (so far only for up, down and settings button pins)
     */
    PCICR |= (1<<PCIE0);
    PCMSK0 |= (1<<SETTINGS_BUTTON_INT) | (1<<UP_SIGNAL_INT) | (1<<DOWN_SIGNAL_INT);

    /*
     *  Timer0 for the torch height control algorithm
     */
    // CTC mode
    TCCR0A |= (1<<WGM01);
    // 1000 Hz frequency (formula from datasheet) (CONFIRMED). For getting actual frequency
    // of controlling divide this frequency on PRESCALER_MAIN_ALGO
    OCR0A = 124;
    // Set prescaler to 64 and start the timer
    TCCR0B |= (1<<CS01)|(1<<CS00);
    // We don't have dedicated controlling procedure as we use interrupt of Timer0 for this task.
    // So we need to activate it every time we want to start control algorithm
    #define REGULATION_START TIMSK0|=(1<<OCIE0A)
    #define REGULATION_STOP TIMSK0&=~(1<<OCIE0A)

    /*
     *  Timer1 for LCD redrawing (2.5 Hz) (CONFIRMED)
     */
    OCR1A = 12499;
    // Mode CTC on OCR1A, set prescaler to 256 and start the timer
    TCCR1B |= (1<<WGM12) | (1<<CS12);
    // We don't have dedicated routine for LCD processing as we use interrupt of Timer1 for this task.
    // So we need to enable interrupt every time when we want to constantly refreshing LCD. See
    // corresponding ISR for more details
    #define LCD_ROUTINE_ON TIMSK1|=(1<<OCIE1A)
    #define LCD_ROUTINE_OFF TIMSK1&=~(1<<OCIE1A)

    motor_init();

    adc_init();

    /*
     *  Handle bypass mode
     */
    if (eeprom_read_byte(&bypass_ON_flag_EEPROM)) {
        lcd.clear();
        lcd.print("regulation off");
    }
    else {
        // LCD timer ON
        LCD_ROUTINE_ON;
        // Plasm interrupt ON
        PCMSK0 |= (1<<PLASM_SIGNAL_INT);
    }

    // finally globally enable interrupts
    sei();

    while (1) {}
}



/*
 *  Interrupt handler for control algorithm timer
 */
ISR (TIMER0_COMPA_vect) {

    // measure voltage of the arc
    feedback = adc_read(ADC_FEEDBACK_PIN);

    // Define setpoint by measured values that staying in ±OFFSET_FOR_AVRG interval
    // (NUM_OF_VALUES_FOR_SETPOINT_DEFINITION values)
    if (setpoint_defined == false) {
        if (feedback_accum_cnt < NUM_OF_VALUES_FOR_SETPOINT_DEFINITION) {
            if (abs(feedback-feedback_prev) <= OFFSET_FOR_AVRG) {
                feedback_accum += feedback;
                feedback_accum_cnt++;
            }
        }
        else {
            setpoint_defined = true;
            // calculate setpoint
            setpoint = feedback_accum/NUM_OF_VALUES_FOR_SETPOINT_DEFINITION;

            feedback_accum = 0;
            feedback_accum_cnt = 0;

            sprintf(bufferA, "measured: %0.2fV", ADC_REFERENCE_VOLTAGE*setpoint/1023);
            lcd.clear();
            lcd.print(bufferA);

            menu = WORK_MENU;

            // Turn ON timer interrupt for LCD when setpoint is defined and main
            // regulation mode is started
            LCD_ROUTINE_ON;
        }
    }
    // setpoint defined
    else {
        // Same as above, we accumulate ADC values that close to each other
        // (±OFFSET_FOR_AVRG hysteresis interval)
        if (abs(feedback-feedback_prev) <= OFFSET_FOR_AVRG) {
            feedback_accum += feedback;
            feedback_accum_cnt++;
        }

        // We gather data at each ISR flash but process it only each
        // PRESCALER_MAIN_ALGO times
        if (++prescaler_cnt == PRESCALER_MAIN_ALGO) {
            prescaler_cnt = 0;

            feedback_avrg = feedback_accum/feedback_accum_cnt;
            feedback_accum = 0;
            feedback_accum_cnt = 0;

            // lift up if torch is too low (taking into account the hysteresis interval)
            if (feedback_avrg < (setpoint-setpoint_offset))
                motor_up();
            // get down if torch is too high (taking into account the hysteresis interval)
            else if (feedback_avrg > (setpoint+setpoint_offset))
                motor_down();
            // otherwise stop
            else
                motor_stop();
        }
    }

    // store previous value
    feedback_prev = feedback;
}



/*
 *  Interrupt handler for LCD menu timer
 */
ISR (TIMER1_COMPA_vect) {

    // if bypass mode ON
    if ( eeprom_read_byte(&bypass_ON_flag_EEPROM) ) {
        // turn off LCD timer interrupt (i.e. this interrupt)
        LCD_ROUTINE_OFF;
        return;
    }

    switch (menu) {

        // print only second row - current voltage (averaged)
        case WORK_MENU:
            sprintf(bufferB, "%0.2f", ADC_REFERENCE_VOLTAGE*feedback_avrg/1023);
            break;

        // print only once, then turn off LCD timer interrupt
        case IDLE_MENU:
            sprintf(bufferA, "sp: %0.2fV+-%3umV", ADC_REFERENCE_VOLTAGE*setpoint/1023,
                                                  (uint16_t)(1000*ADC_REFERENCE_VOLTAGE*setpoint_offset/1023));
            sprintf(bufferB, "lft%u dlay%u", cutting_height,
                                             pierce_time*PIERCE_TIME_ELEMENTARY_DELAY);
            lcd.clear();
            lcd.print(bufferA);
            // turn off LCD timer interrupt
            LCD_ROUTINE_OFF;
            break;

        // For the next 3 menu entries first string (bufferA) was printed outside this ISR
        // (at settings button ISR) so we only need to handle second row
        case cutting_height_MENU:
            // We don't map this, the default interval 0-1023 is OK for us
            // since ~200 steps is a one full revolution
            cutting_height = adc_read(ADC_SETTINGS_PIN);
            sprintf(bufferB, "%4u steps", cutting_height);
            break;

        case SETPOINT_OFFSET_MENU:
            setpoint_offset = map( adc_read(ADC_SETTINGS_PIN), 0, 1023,
                                   1023*SETPOINT_OFFSET_MIN_SET_VOLTAGE/ADC_REFERENCE_VOLTAGE,
                                   1023*SETPOINT_OFFSET_MAX_SET_VOLTAGE/ADC_REFERENCE_VOLTAGE  );
            sprintf(bufferB, "%3umV", (uint16_t)(1000*ADC_REFERENCE_VOLTAGE*setpoint_offset/1023));
            break;

        case PIERCE_TIME_MENU:
            pierce_time = map(adc_read(ADC_SETTINGS_PIN), 0, 1023, 0, PIERCE_TIME_MAX_TIME);
            sprintf(bufferB, "%5ums", pierce_time*PIERCE_TIME_ELEMENTARY_DELAY);
            break;
    }

    // we always print second row of LCD here
    lcd.setCursor(0, 1);
    lcd.print(bufferB);
}



/*
 *  Input signals interrupt
 */
ISR (PCINT0_vect) {

    // determine which bits have changed
    uint8_t changed_bits = SIGNALS_PIN ^ signals_port_history;
    // store current state as old one
    signals_port_history = SIGNALS_PIN;


    if ( changed_bits & (1<<SETTINGS_BUTTON_PIN) ) {
        // Anti-jitter delay. Button is pressed by a human so generally we need some
        // significant delay (also, the button quality is another one factor)
        _delay_ms(100);

        // HIGH to LOW pin change
        if ( !(SIGNALS_PIN & (1<<SETTINGS_BUTTON_PIN)) ) {

            // hold the button for about 7.5s to turn ON/OFF bypass mode (valid only from idle mode)
            uint8_t toggle_bypass_cnt = 0;
            while ( !(SIGNALS_PIN & (1<<SETTINGS_BUTTON_PIN)) && (menu==IDLE_MENU) ) {
                _delay_ms(100);
                if (++toggle_bypass_cnt > 75) {  // 75*100ms = 7.5s
                    toggle_bypass_cnt = 0;

                    // write status in EEPROM
                    eeprom_update_byte(&bypass_ON_flag_EEPROM, eeprom_read_byte(&bypass_ON_flag_EEPROM)^1);

                    // toggle plasm interrupt
                    PCMSK0 ^= (1<<PLASM_SIGNAL_INT);

                    lcd.clear();
                    lcd.print("regulation off");

                    // LCD timer interrupt ON
                    LCD_ROUTINE_ON;

                    return;
                }
            }
            // ignore short clicks in bypass mode
            if ( eeprom_read_byte(&bypass_ON_flag_EEPROM) ) return;


            /*
             *  Menu routine. We cycle through the menu entries and store
             *  the entered value from the previous menu (at each button press)
             */
            if (++menu == NUM_OF_MENUS)
                menu = IDLE_MENU;

            if (menu == IDLE_MENU) {
                // save previous parameter in EEPROM on button press
                eeprom_update_byte(&pierce_time_EEPROM, pierce_time);
                // turn on plasm interrupt only in Idle mode
                PCMSK0 |= (1<<PLASM_SIGNAL_INT);
            }
            else {
                // turn off plasm interrupt in settings mode
                PCMSK0 &= ~(1<<PLASM_SIGNAL_INT);

                if (menu == cutting_height_MENU) {
                    sprintf(bufferA, "lift (%u):", cutting_height);
                }
                else if (menu == SETPOINT_OFFSET_MENU) {
                    eeprom_update_word(&cutting_height_EEPROM, cutting_height);
                    sprintf(bufferA, "offset (%u):", (uint16_t)(1000*ADC_REFERENCE_VOLTAGE*setpoint_offset/1023));
                }
                else if (menu == PIERCE_TIME_MENU) {
                    eeprom_update_word(&setpoint_offset_EEPROM, setpoint_offset);
                    sprintf(bufferA, "delay (%u):", pierce_time*PIERCE_TIME_ELEMENTARY_DELAY);
                }

                lcd.clear();
                lcd.print(bufferA);
                // timer interrupt for LCD ON
                LCD_ROUTINE_ON;
            }
        }
    }


    // Note that it's important to use if-elseif construction because otherwise
    // we can fall to the strange and unpredicted behavior
    else if ( changed_bits & (1<<UP_SIGNAL_PIN) ) {
        if ( SIGNALS_PIN & (1<<UP_SIGNAL_PIN) )
            motor_stop();
        else
            motor_up();
    }


    else if ( changed_bits & (1<<DOWN_SIGNAL_PIN) ) {
        if ( SIGNALS_PIN & (1<<DOWN_SIGNAL_PIN) )
            motor_stop();
        else
            motor_down();
    }


    else if ( changed_bits & (1<<PLASM_SIGNAL_PIN) ) {
        // plasm relay cause jitter
        _delay_ms(20);

        // LOW to HIGH pin change (plasm is OFF)
        if ( SIGNALS_PIN & (1<<PLASM_SIGNAL_PIN) ) {
            // regulation OFF (imer interrupt for algorithm OFF)
            REGULATION_STOP;
            motor_stop();

            // reset variables
            prescaler_cnt = 0;
            feedback_accum = 0;
            feedback_accum_cnt = 0;
            feedback_avrg = 0;
            setpoint_defined = false;

            // interrupt for signals ON
            PCMSK0 |= (1<<SETTINGS_BUTTON_INT) | (1<<UP_SIGNAL_INT) | (1<<DOWN_SIGNAL_INT);
            // turn off touch tracking
            PCMSK0 &= ~(1<<TOUCH_SIGNAL_INT);

            // go to Idle mode
            menu = IDLE_MENU;

            // in case of interrupted regulation session Timer interrupt for LCD ON
            LCD_ROUTINE_ON;
        }

        // HIGH to LOW pin change (plasm is ON)
        else {
            // turn off touch tracking (do it only here to prevent any random triggering)
            PCMSK0 |= (1<<TOUCH_SIGNAL_INT);
            // interrupt OFF for all signals except PLASM_SIGNAL_PIN and TOUCH_SIGNAL_PIN
            PCMSK0 &= ~( (1<<SETTINGS_BUTTON_INT) | (1<<UP_SIGNAL_INT) | (1<<DOWN_SIGNAL_INT) );
            // move down till the torch touch the metal
            motor_down();

            lcd.clear();
            lcd.print("start...");
        }
    }


    else if ( changed_bits & (1<<TOUCH_SIGNAL_PIN) ) {

        // HIGH to LOW pin change
        if ( !(SIGNALS_PIN & (1<<TOUCH_SIGNAL_PIN)) ) {

            motor_stop();
            // turn off touch tracking
            PCMSK0 &= ~(1<<TOUCH_SIGNAL_INT);
            // lift to desired distance of cutting the metal
            motor_move(cutting_height);

            // Wait a bit for pierce (torch is fully burning and all metal droplets
            // can't affect the measurements)
            lcd.clear();
            lcd.print("pierce...");
            uint8_t pierce_time_cnt = pierce_time;
            do {
                _delay_ms(PIERCE_TIME_ELEMENTARY_DELAY);
            } while (--pierce_time_cnt);

            // If you see this, it means very noisy signal and/or too many points for setpoint definition
            // (NUM_OF_VALUES_FOR_SETPOINT_DEFINITION), and/or very small interval for averaging (OFFSET_FOR_AVRG)
            lcd.clear();
            lcd.print("define sp...");

            // timer interrupt for algorithm ON
            REGULATION_START;
        }
    }

}
