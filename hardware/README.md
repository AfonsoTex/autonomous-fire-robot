# Hardware

Wiring diagram and the 3D-printed chassis (Fusion 360 source; export an STL from it to print).

| File | What it is |
| --- | --- |
| `wiring_diagram.png` | Raspberry Pi 5, power, L293D motor drivers, encoder motors, LiDAR, camera, two IMUs, water pump, water level sensors and pan/tilt servos |
| `Chassis.f3d` | Chassis source (Fusion 360) |

This is the Phase III wiring, with the Raspberry Pi 5 controlling everything. It has not been built or tested yet, and the Raspberry program does not drive the motors or read the encoders yet.

<div align="center"><img src="wiring_diagram.png" width="1000" alt="Complete Raspberry Pi 5 wiring: power, L293D motor drivers, encoder motors, LiDAR, camera, two IMUs, water pump, water level sensors and servos"></div>

## Power

- **Batteries:** two 2S packs of 18650 cells in parallel (7.4 V nominal, 8.4 V full, 2000 mAh each). Charge each pack separately and join them only when their voltages are within 0.1 V.
- **T-plug:** the only power connection between the batteries and the robot. With it unplugged, nothing after it is powered, including the voltmeter.
- **Fuse:** a 15 A blade fuse on the positive wire, between the T-plug and the + Point. It cuts the batteries off if there is a short circuit.
- **Voltmeter:** connected after the T-plug, so it does not drain the batteries while the robot is off. The minimum PWM that turns the motors rises as the battery drains.
- **Pi UBEC (7.4 V → 5 V, 5 A):** feeds only the Raspberry Pi, through a USB-C cable. The LiDAR and the camera get their power from the Pi.
- **Servo UBEC (7.4 V → 5 V, 7 A):** feeds only the two MZ996 servos, on its own red wire from the + Point, so servo current peaks do not pull down the Pi's supply. Its output can be 5.25 V ± 0.5 V, so never connect it to the Pi.
- **5 V step-down:** feeds the logic supply of both L293D chips (pin 16) and the two water level sensors. Never connect its output to the Pi's 5 V.
- **Ground:** batteries, both UBECs, step-down, L293D chips, MOSFET, servos, encoders, IMUs, water level sensors and Pi share one ground.

## Pump and servos

- **Pump:** runs from the 7.4 V line. Its negative side goes through an IRLZ44N logic-level MOSFET. The Pi drives the gate from GPIO17 (pin 11) through a 220 Ω resistor; a 10 kΩ resistor from gate to ground keeps the pump off while the Pi boots or if the wire comes loose.
- **Flyback diode:** the pump is an inductive load. A 1N5819 across it (cathode to +) protects the MOSFET from the voltage spike when it switches off.
- **Servos:** two Miuzei MZ996, each drawing up to about 1.8 A when it stalls. They are never connected to the battery line, because a full 2S pack (8.4 V) is above their 7.4 V rating.
- **Servo signals:** pan on GPIO18 (pin 12), tilt on GPIO19 (pin 35), hardware PWM.

## Motors

- **L293D board:** the diagram shows the parts side (top) and the solder side (bottom), where the matching pins of the two chips are joined so both receive the same signals. Pin 16 takes the 5 V logic supply and pin 8 takes the 7.4 V motor supply.
- **Speed:** left side on GPIO12 (pin 32) to L293D pin 1, right side on GPIO13 (pin 33) to L293D pin 9, hardware PWM. A 10 kΩ resistor from each of these lines to ground keeps the motors off while the Pi boots.
- **Direction, left side:** GPIO16 (pin 36) to L293D pin 2 and GPIO20 (pin 38) to L293D pin 7.
- **Direction, right side:** GPIO21 (pin 40) to L293D pin 15 and GPIO26 (pin 37) to L293D pin 10.
- **Motors:** motors 1 and 3 are the left pair, motors 2 and 4 the right pair. Each has a 6-pin PH2.0 plug: encoder GND, phase A, phase B, encoder VCC, Motor+ and Motor−. Motor+ and Motor− go to the L293D outputs.

## Sensors

- **Encoder power:** 3.3 V from Pi pin 1 and ground from Pi pin 9. Never use 5 V, because the GPIO pins only tolerate 3.3 V.
- **Encoder signals (A, B), with internal pull-ups:** motor 1 on GPIO22 and GPIO23, motor 2 on GPIO27 and GPIO4, motor 3 on GPIO5 and GPIO6, motor 4 on GPIO24 and GPIO25.
- **IMUs (MPU6050, GY-521 modules):** one on the chassis and one on the tower, sharing the same I2C bus: SDA on GPIO2 (pin 3), SCL on GPIO3 (pin 5), 3.3 V from pin 17 and ground from pin 25. The chassis IMU leaves AD0 unconnected (address 0x68). The tower IMU has AD0 tied to 3.3 V (address 0x69). XDA, XCL and INT stay unconnected on both.
- **LiDAR (RPLIDAR C1):** connects through its serial-to-USB adapter board to a USB port on the Pi. It uses no GPIO.
- **Camera (OV5647, 5 MP):** connects to a CAM/DISP connector on the Pi with a 30 cm ribbon, 22-pin on the Pi side and 15-pin on the camera side. It uses no GPIO.
- **Water level sensors (two XKC-Y25-NPN):** glued outside the bottle, one low to know when to refill and one high to know when it is full. The plug is cut off and the wires are soldered: brown to 5 V, blue to ground (Pi pin 39), black (mode) to 5 V, yellow to the signal.
- **Water sensor signal:** the yellow wire can reach 5 V and the GPIO pins only tolerate 3.3 V. Each sensor uses a divider: 10 kΩ from yellow to the GPIO and 18 kΩ from the GPIO to ground, which gives about 3.2 V with water and 0 V without. The bottom sensor goes to GPIO7 (pin 26) and the top one to GPIO8 (pin 24), with the internal pull-ups off.

GPIO 9, 10, 11, 14 and 15 remain free.

## Things to check before trusting the wiring

- **Motor plug orientation:** confirm which side of the plug is pin 1 on the real motor before connecting it. A reversed plug could put the motor voltage on the encoder.
- **Servo UBEC output:** measure it with a multimeter before connecting the servos.
- **Water sensor signal:** before connecting a sensor to the Pi, measure the point between its two resistors. It must read about 3.2 V with water and 0 V without. The wire functions come from the XKC-Y25-T12V datasheet, a close variant, so confirm them on the real sensor.
- **Stopping the motors:** hardware PWM keeps running if the Raspberry program freezes, so the motor code needs its own way to stop the car.
