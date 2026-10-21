# ESPHome Matrix-buttons custom component

**Custom ESPHome component for binary buttons arranged in a matrix configuration using multiplexers. Derived from matrix_keypad. Can be used for push-buttons or toggle-buttons if every button is protected with diode (see example circuit).**

To install, download and copy the matrix_buttons directory into the "my_components" subdirectory of your esphome project directory.

### Features

  - Works with io-extenders like PCF8574 or MCP23017.
  - Allows to specify a scan_delay in microseconds to wait between activating a row and reading back the column states (needed for extenders or optocouplers, amongst others, to propagate the activation).
  - Allows to specify a script-id for asynchronuous handling of state changes.
  - Allows detection of multiple activated buttons or switches (every button and switch needs a diode or similar to avoid ghosting).
  - Handling of fast consecutive state changes: triple or more state changes are silently collapsed into one or two state changes.

## Component/Hub

```yaml
# Example configuration entry for an 8x8 matrix with two pcf8574 expanders (exp_one and exp_two) communicating over an i2c bus (i2c_exp).
# Example circuit:
#   pcf-row  -> optoled -> 220R -> gnd
#   12V      -> opto-transistor -> ( 4k7 -> gnd ) -----> push-button (momentary or latching)
#   GND      <- optoled <- 1kR <-----------------------------|
#   pcf-col  <- opto-transistor <- gnd

i2c:
  - id: i2c_exp
    sda: GPIO09
    scl: GPIO10
    frequency: 400kHz

pcf8574:
  - id: exp_one
    i2c_id: i2c_exp
    address: 0x20
  - id: exp_two
    i2c_id: i2c_exp
    address: 0x21

matrix_buttons:
  id: my_matrix_buttons
  active_high: false     # Rows and columns are active low
  passive_rows: false    # Set to true if outputs aren't protected: non-protected outputs will be switched to input mode when not active to prevent damage from back-feeding current through the diodes
  multi_keys: 1000000000 # Set to the number of simultaneous key presses to allow (if hardware supports it), otherwise only one key press will be allowed at a time
  scan_delay: 60         # Delay in µs for rows to stabilize before reading columns, can be adjusted based on the hardware (opto-couplers, diodes, etc.) used in the matrix
  rows:
    - pin: { pcf8574: exp_one, number: 0, mode: OUTPUT }   # outputs are pulled down on activation
    - pin: { pcf8574: exp_one, number: 1, mode: OUTPUT }
    - pin: { pcf8574: exp_one, number: 2, mode: OUTPUT }
    - pin: { pcf8574: exp_one, number: 3, mode: OUTPUT }
    - pin: { pcf8574: exp_one, number: 4, mode: OUTPUT }
    - pin: { pcf8574: exp_one, number: 5, mode: OUTPUT }
    - pin: { pcf8574: exp_one, number: 6, mode: OUTPUT }
    - pin: { pcf8574: exp_one, number: 7, mode: OUTPUT }
  columns:
    - pin: { pcf8574: exp_two, number: 0, mode: INPUT } # columns are pulled down on activation
    - pin: { pcf8574: exp_two, number: 1, mode: INPUT }
    - pin: { pcf8574: exp_two, number: 2, mode: INPUT }
    - pin: { pcf8574: exp_two, number: 3, mode: INPUT }
    - pin: { pcf8574: exp_two, number: 4, mode: INPUT }
    - pin: { pcf8574: exp_two, number: 5, mode: INPUT }
    - pin: { pcf8574: exp_two, number: 6, mode: INPUT }
    - pin: { pcf8574: exp_two, number: 7, mode: INPUT }
  debounce_time: 20
  on_state:
     - lambda: |-  
         ESP_LOGD("my_matrix_buttons:on_state", "state %d pos %d row %d col %d rowcnt %d colcnt %d (key %d [%c])", 
                  (int) state, pos, row, col, rowcnt, colcnt, (int) key, (char)(key==0 ? '@' : key));
  script_id: process_matrix_button_state

script:
  - id: process_matrix_button_state
    mode: single
    parameters:
      state: bool
      pos: int
    then:
      - lambda: |-
        ....
```

### Configuration variables

  - id (Optional, ID): Set the ID of this device for use in lambdas.
  - rows (Required, list): A list of pins where the horizontal matrix lines are connected, in order from top to bottom. These pins need to be output capable.
  - columns (Required, list): A list of pins where the vertical matrix lines are connected, in order from left to right. These pins need to be input capable with pullups enabled. If there is no internal pullup, then an external one is required.
  - keys (Optional, string): The keys present on the matrix, from top left to bottom right, row by row. Required for key_collector and binary_sensor (if using key selection).
  - debounce_time (Optional, int): Debounce time for button activations in milliseconds. Defaults to 1.
  - scan_delay (Optional, int): Time in microseconds to wait for a row activation to propagate before checking column activations. Defaults to 0, maximum value of 500. 
  - passive_rows (Optional, boolean): Set to true if inactive rows should be switched to passive inputs. This is mandatory when the rows are not protected with diodes, optocouplers or anything else, against short-circuiting with adjacant rows, which happens when multiple buttons are active at once. When set to false, inactive rows are switched to low/high, depending on the value of active_high, which is faster than switching them to inputs. Defaults to true.
  - active_high (Optional, boolean): Set to true if rows and columns are active high. Can still be inversed with the pin properties if needed. Defaults to false.
  - multi_keys (Optional, int): Set to the number of simultaneous key presses to allow (if hardware supports it), otherwise only one key press will be allowed at a time. Defaults to 1.
  - script_id (Optional, ID): ID of a script that will be called on every state change of a button.

## Example circuit

Following example circuit has been used with the yaml above. The diode in series with every button allows the matrix to detect multiple button activations, which in turn allows the use of both toggle and momentary switches. The row pins are protected by optocouplers, allowing the "passive_rows" property to be safely set to false. As a PC817 optocoupler may induce a delay up to 18µs, a scan delay of 60µs is needed after activation of a row for the signal to propagate throug the i2c bus, the optocouplers and the rather long transmission lines of up to 100m to the push-buttons or switches (typical signal propagation in a copper conductor is around 5ns/m).

<img src="example/esp32-pcf8574 circuit-v2c.jpg" alt="Example Circuit">

## Rationale / Use-Case

I created this component as part of introducing Home Assistant (HA) into my home.

The original electrical installation uses impulse relays (teleruptors) controlled by push-buttons throughout the house. That implies a lot of wires carrying AC24V between the teleruptors and their corresponding push-buttons. Its a lot of wiring!

I could replace the push-buttons with smart switches communicating with HA over Wi-Fi. The above component, however, allows me to reuse the existing wires and - by using I/O expanders - only use two GPIO pins on the ESP32.

Along the way, I thought it would also be nice to have the option of converting certain push-buttons into toggle switches. That idea eventually led me to the solution presented above.

I hope someone else may find this useful too ;-)

## Disclaimer

This code is experimental and has not been thoroughly tested. It is provided as is, without any warranty or liability, as stated in the <a href="https://www.gnu.org/licenses/gpl-3.0.html">GPLv3 LICENCE.txt</a>.

This warning is particularly important when using microcontrollers, such as the ESP32, to control electrical installations. Experimental hardware and software can behave unpredictably and may pose serious safety risks.

You are responsible for taking all necessary safety precautions when developing, testing, installing, or using experimental electrical projects. These include following established electrical safety practices, applying all legally required safety measures, and ensuring that the work is carried out by a suitably qualified person where required.

**Failure to take appropriate precautions may result in personal injury, fire, or property damage.**

Use this code and any associated hardware entirely at your own risk.
