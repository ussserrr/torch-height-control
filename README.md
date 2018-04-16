# torch-height-control
CNC plugin for the plasma torch height controlling on the base of Arduino (ATmega328P MCU). Can be used to improve cutting quality of bent and distorted metal plates. Regulator intercepts Z-axis (vertical) movement of a torch. Bypass mode is provided for cases when control is unneeded. Main idea is to determine the current height by measuring a voltage of the plasma arc: the higher the voltage, the greater the distance.


## Features
  - Receiving signals from CNC for initial and manual positioning;
  - API for easy interfacing with any Z-axis actuator (2 drivers for stepper motor are already available out-of-the-box: 4-wire bipolar and 2-wire "smart" driver (STEP+DIR));
  - Auto defining of the height to keep (setpoint) using only initial presetted distance value. It allows to abstract from such parameters as metal type, its thickness etc.;
  - Adjustable insensitivity hysteresis of setpoint;
  - Software filtering of an arc voltage reduces noise and spikes (recommended for usage alongside with hardware filter);
  - Displaying of a current arc voltage and measured setpoint value allows to evaluate control quality (required LCD (sort of HD44780));
  - Detecting a touch of torch and metal at startup (no need to manually set the initial height at every cut, should do it only once in the settings);
  - Bypass mode (system only responds to the Up/Down signals);
  - Settings menu for all necessary parameters;
  - Preserving parameters after power off.


## Logic description
  1. CNC or user performs initial positioning (e.g. motion to the entry point);
  2. After Plasm signal presence torch going down until the Touch signal is appeared;
  3. Torch immediately lifting to the pierce height (specified parameter);
  4. Torch is holding on at this position for the entire pierce time (specified parameter);
  5. After setpoint calculation, then main control algorithm starts;
  6. System goes back to the Idle mode when the cutting is complete (Plasm signal is turning off).


## Connection
To minimize effect of cross-talks and spikes there should be solid assembly and the plugin must not be located near any high-voltage or power circuits. Some installed shield (full case or just to wires) is also desirable. It's advised to galvanic isolate device as possible to protect low-current and logic circuits.

### Digital inputs
All digital inputs are pulled-up to the high-level voltage by default (so they are triggered by 0). ATmega328P and Arduino pins notations are specified in the brackets.
  - **UP** (PB2, 10) and **DOWN** (PB3, 11) - Up and Down signals for Z-axis. Movement is performing for all time the signal is applying. These signals are ignored in the Working mode;
  - **PLASM** (PB4, 12) - signal of the plasma ignition. It must be directed both to the regulator and the torch (e.g. DPST relay);
  - **TOUCH** (PB1, 9) - signal propagating when a metal plate and a torch touch each other. There are variety of available implementations depends on the specific construction of Z-axis. There can be, for example, a capacitive sensor or some sort of connection directly to the torch nozzle (so when the torch touches the metal they both should have got the same electrical potential).
  - **SETTINGS BUTTON** (PB0, 8) - just button for the cyclic navigation through the menu.

### Analog inputs
  - **FEEDBACK** (PC1, A1) - arc voltage value that limited to the diapason of ADC input voltages (0-5 V, desirable with some gap). It can be done, in the simplest case, via basic voltage divisor. Additionally recommended to install any low-pass filter to reduce noise and spikes;
  - **SETTINGS ADC** (PC0, A0) - used to set parameters. Usually represented by a potentiometer (3-100K) connected between GND and 5V.

### Motor
To actuating of Z-axis your driver should provide 5 functions:
  - `motor_init()`;
  - `motor_up()`;
  - `motor_down()`;
  - `motor_stop()`;
  - `motor_move(int16_t steps)` where the sign of `steps` indicates a direction of movement.

Also, note that your custom pinout should not conflict with other signals. It's recommended to use PC2-5 (Arduino's A2-5) pins.

There already 2 libraries in the `/lib` folder representing 2 different drivers for stepper motors' driven Z-axis.

#### MotorControl
Straightforward driver that uses Timer2 to manually switches corresponding phases of 4-wire bipolar stepper motors (wave (one-phase-on) mode). Plug in your motor to any switches (relays, discrete transistors, array of transistors, driver IC and so on). Correct order of phases' connection depends on your motor but generally is A-C-B-D, considering AB and CD as 2 coils. Other parameters that you should adjust are period and pulsewidth (we consider them as equal).

#### MotorDriver
Library for usage with "smart" drivers that controlled by 2 signals: STEP and DIRECTION. It also uses Timer2 to form STEP pulses sequence. Other parameters that you should adjust to match your driver are period and pulsewidth.

### Display
THC uses Arduino's LiquidCrystal library to manage LCD (HD44780, its derivatives and other compatible ones). Timer1 is used for refreshing information on the screen when it's needed (e.g. in Working mode or in the settings menu). By default LCD is connected to PD1-7 pins with following pinout (Arduino notation in the brackets):
  - PD4 (4) - DB4;
  - PD5 (5) - DB5;
  - PD6 (6) - DB6;
  - PD7 (7) - PD7;
  - PD1 (1) - RS;
  - PD2 (2) - RW
  - PD3 (3) - E.


## User parameters
Before flashing the firmware you can check and set some related parameters. Most of them are located in `TorchHeightControl.cpp/h` files:
  - I/Os and default signals' port state (in case of reassigning);
  - Cutting height is measured in steps of stepper motor (0-1023). EEMEM is the default value at the flash time. Actually, not so important because we can always change it at runtime;
  - Pierce time default EEMEM;
  - Bypass mode flag;
  - Number of values for setpoint definition;
  - Setpoint hysteresis offset EEMEM default value (setpoint ± setpoint_offset);
  - Interval of voltages for specifing setpoint offset in settings menus;
  - Prescaler for dividing frequency of algorithm' timer in its ISR;
  - Hysteresis for averaging only close results of ADC measurements;


## Build and flash
It's recommended to use [PlatformIO](https://platformio.org) for building and flashing as all-in-one solution.

### CLI
Change settings in `platformio.ini` file if needed (for example, specify whether use programming unit or not). Then in the same directory run:
```sh
$ pio run  # build

$ pio run -t program  # flash using usbasp
or
$ pio run -t upload  # flash using on-board programmer

$ pio run -t uploadeep  # write EEPROM
```


### IDE (Atom or VSCode)
  1. Import project: `File` -> `Add Project Folder`;
  2. Change settings in `platformio.ini` file if needed (for example, specify whether use programming unit or not);
  3. Build: `PlatformIO` -> `Build`;
  4. Open built-in terminal and run:
     ```sh
     $ pio run -t program; pio run -t uploadeep
     ```
     or for Arduino board:
     ```sh
     $ pio run -t upload; pio run -t uploadeep
     ```


## Usage
After reset, LCD displays Idle mode. It contains current settings of setpoint hysteresis, the cutting height `lft` (in steps) and the pierce time `dlay`. Cycle through the settings menu by pressing settings button till you get back to the Idle mode. Current value of each parameter is indicated in the brackets. Use your potentiometer to adjust values.

After Plasm ON signal, `start...` string is appears. After touching the metal, `pierce...` lasts entire pierce time. Then `define sp...` is appeared on short time during which setpoint is defining. Finally, working mode follows and the first LCD line displays measured setpoint and the second displays current averaged arc voltage. If the `define sp...` string lasts too long it means that whether the number of values for setpoint definition is too high or the offset for averaging is too small or an arc signal is just too noisy.

After cutting completes, Idle mode will also display last measured setpoint.

To enter (and to exit) Bypass mode press and hold Settings button for 8 seconds. LCD will display `regulation off` string. THC then will respond only to Up/Down signals. Bypass mode state is saved after resets and power offs.


## Notes
See states diagram (UML) in `torch-height-control-uml.*` files (created with [draw.io](https://draw.io)).

Behavior of THC when multiple signals are applied is undefined in general and depends on the order of corresponding handlers in the `PCINT0_vect` ISR. Because of the `if - else if` construction execution of multiple code blocks is impossible but such conditions are still not recommended. However, at normal operating, those cases are not met.
