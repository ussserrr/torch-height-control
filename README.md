# torch-height-control
CNC plugin for the plasma torch height controlling on the base of Arduino (ATmega328P MCU). Can be used to improve cutting quality of bent and distorted metal plates. Regulator intercepts Z-axis (vertical) movement of a torch. Bypass mode is provided for cases when control is unneeded. Main idea is to determine current height by measuring a voltage of the plasma arc: the higher the voltage, the greater the distance.


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


## Logic' description
  1. CNC or user performs initial positioning (e.g. motion to the entry point);
  2. After Plasm signal presence torch going down until the Touch signal is appeared;
  3. Torch immediately lifting to the pierce height (specified parameter);
  4. Torch is holded at this position for the entire pierce time (specified parameter);
  5. After setpoint calculation, then main control algorithm starts;
  6. System goes back to the Idle mode when the cutting is complete (Plasm signal is turning off).


## Connection
To minimize effect of cross-talks and spikes there should be solid assembly and the plugin must not be located near any high-voltage or power circuits. Some installed shield (full case or just to wires) is also desirable. It's advised to galvanic isolate device as possible to protect low-current and logic circuits.

### Digital inputs
All digital inputs are pulled-up to the high-level voltage by default. ATmega328P and Arduino pins notations are specified in the brackets.
  - **UP** (PB2, 10) and **DOWN** (PB3, 11) - Up and Down signals for Z-axis. Movement is performing for all time the signal is applying. These signals are ignored in the Working mode;
  - **PLASM** (PB4, 12) - signal of the plasma ignition. It must be directed both to the regulator and the torch (e.g. DPST relay);
  - **TOUCH** (PB1, 9) - signal propagating when a metal plate and a torch touch each other. There are variety of available implementations depends on the specific construction of Z-axis. There can be, for example, a capacitive sensor or some sort of connection directly to the torch nozzle (so when the torch touches the metal they both should have got the same electrical potential).
  - **SETTINGS BUTTON** (PB0, 8) - just button for the cyclic navigation through the menu.

### Analog inputs
  - **FEEDBACK** (PC1, A1) - arc voltage value that limited to the diapason of ADC input voltages (0-5 V, desirable with some gap). It can be done, in the simplest case, via basic voltage divisor. Additionally recommended to install any low-pass filter to reduce noise and spikes;
  - **SETTINGS ADC** (PC0, A0) - used to set parameters. Usually represented by a potentiometer (3-100K) connected between GND and 5V.

### Motor

