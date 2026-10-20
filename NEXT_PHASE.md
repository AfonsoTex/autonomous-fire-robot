# ESP32-CAM Firefighter — Next phase

*From a line follower that puts out candles to a robot that maps a room, patrols it and refills itself.*

## Contents

- [1. Where the robot is today](#1-where-the-robot-is-today)
- [2. What has to change, and why](#2-what-has-to-change-and-why)
- [3. Navigation: from following a line to SLAM](#3-navigation-from-following-a-line-to-slam)
- [4. Seeing fire: reflections and the dark](#4-seeing-fire-reflections-and-the-dark)
- [5. Turning](#5-turning)
- [6. Knowing the wheels really turn](#6-knowing-the-wheels-really-turn)
- [7. A PCB instead of wires](#7-a-pcb-instead-of-wires)
- [8. A chassis that does not bend](#8-a-chassis-that-does-not-bend)
- [9. Water level](#9-water-level)
- [10. The refill station](#10-the-refill-station)
- [11. The mission](#11-the-mission)
- [12. Materials](#12-materials)
- [13. Suggested order](#13-suggested-order)

## 1. Where the robot is today

The current robot is a four-motor car built around an ESP32-CAM, a PC and a Raspberry Pi 5. It already does the following:

- **Drives** from an Xbox controller over WiFi, or **follows a line** of tape on its own using the ESP32 camera and OpenCV on the PC.
- **Detects flames** with FireNet, a small neural network trained on images recorded by the robot, running on the Pi.
- **Stops the car** through a GO/STOP wire from the Pi to the ESP32 when a flame is seen.
- **Aims** a pan/tilt nozzle at the flame using a calibration table, and **sprays** short water pulses with a limited number of retries.
- **Resumes** the line once the scene is clear.

Building it showed clearly what the next version has to change. Each section below starts from a problem seen on the real robot.

## 2. What has to change, and why

| Problem seen | Consequence | Direction for the next phase |
| --- | --- | --- |
| The robot can only follow a line of tape | It cannot patrol a real room, go to a flame or find its way back | SLAM: build a map and navigate in it (section 3) |
| Hundreds of loose wires | Hard to debug, connectors come loose, difficult to reproduce | A custom PCB (section 7) |
| The chassis bends in the middle | Heavy parts (battery, water, Pi) sit between the axles | New chassis with the weight over the wheels (section 8) |
| To turn left it stops the left motors and drives the right ones | Wide turns, no turning on the spot, struggles in sharp corners | Independent direction per side, or real steering (section 5) |
| The car often got stuck with the motors on | As the battery drains, the same PWM no longer moves the car | Wheel encoders and closed-loop speed control (section 6) |
| FireNet sees reflections of the flame on wet floors | After spraying, the robot stops again for a reflection | A thermal camera to confirm real heat (section 4) |
| The robot does not know how much water it has | It can spray an empty tank | A water level sensor and a refill station (sections 9 and 10) |
| It needs light to work | Line following and FireNet fail in the dark | LiDAR for SLAM, infrared or thermal vision for fire (sections 3 and 4) |

## 3. Navigation: from following a line to SLAM

SLAM (Simultaneous Localisation and Mapping) lets the robot build a map of the room while working out where it is on that map. With a map, the robot can patrol, go to a point, avoid obstacles and come back to a station, which a line can never give it.

### LiDAR or camera

| Option | How it works | Works at night? | Verdict |
| --- | --- | --- | --- |
| 2D LiDAR (e.g. LDROBOT LD19 / LD06, Slamtec RPLIDAR C1) | A spinning laser measures the distance to the walls all around the robot, about 10 times per second | Yes: it brings its own light | Recommended: the most reliable and best supported option |
| Camera, visual SLAM (e.g. ORB-SLAM3, RTAB-Map) | Tracks visual features between frames | Only with infrared light and a NoIR camera | Possible, but needs texture and light, and uses much more CPU |
| Depth camera (e.g. Intel RealSense, OAK-D) | Measures depth per pixel | Partly (active infrared) | Powerful, but expensive and heavy for this size of robot |

**Recommendation:** a 2D LiDAR for the map and navigation, combined with wheel encoders (section 6) and an IMU (e.g. BNO085) so the robot knows how it moved between scans. LiDAR solves the night requirement on its own, because it does not depend on room light.

### Software

- **ROS 2** on the Raspberry Pi 5 as the framework that connects sensors, map and motors.
- **slam_toolbox** to build the map from the LiDAR, **Nav2** to plan paths and drive along them.
- The ESP32 (or a new motor microcontroller) receives speed commands and returns wheel speeds, instead of receiving MOV commands from a PC.
- The PC becomes optional: a laptop is only needed to watch the map and give missions.

## 4. Seeing fire: reflections and the dark

### What happened with reflections

After the first water pulse the floor around the candle gets wet, and the wet floor reflects the flame. FireNet only looks at the colour image, and a reflection of a flame looks exactly like a flame, so the robot detected a second "flame" next to the one it had just sprayed and stopped again.

The current fix is a filter, not a solution: after spraying, new detections within a radius of the sprayed position are ignored for a few seconds after the car is allowed to move again, unless the same detection already existed before spraying. It works most of the time, but a real second flame close to the first can be ignored, and a reflection farther away is not filtered.

### A thermal camera

A reflection is cold; a flame is hot. A thermal camera measures temperature in every pixel, so it can confirm that a detection is real heat. It also works in complete darkness.

| Sensor | Resolution | Notes |
| --- | --- | --- |
| Melexis MLX90640 | 32 × 24 pixels | I2C, connects directly to the Pi. Low resolution, but enough to confirm "there is something hot in that direction". |
| FLIR Lepton 3.5 (on a breakout board) | 160 × 120 pixels | Real thermal image with temperature per pixel. Much more expensive. |
| Small USB thermal cameras (InfiRay / Topdon modules, 256 × 192) | 256 × 192 pixels | Good resolution; check that Linux drivers exist before buying. |

**Proposed use:** keep FireNet on the colour camera to find and aim at the flame, and require the thermal camera to see a hot spot in the same direction before the car stops or sprays. That removes the reflection problem at its source and makes the time-based filter unnecessary.

> **Note:** Thermal cameras do not see through glass, and a hot object that is not a flame (a radiator, a lamp) is also hot. Colour and heat together are much stronger than either alone.

### Fire detection at night

At night a candle is the brightest thing in the room, so the colour camera still sees it, but everything else disappears. The thermal camera covers detection; for the navigation the LiDAR covers the map. If a camera is still wanted for the map, a NoIR camera (no infrared filter) with an infrared LED illuminator sees in the dark without lighting the room for people.

## 5. Turning

Today both L293D drivers share the same direction pins, so the two sides of the car always turn in the same direction. To turn left, the left motors are slowed or stopped and the right ones keep going. The car turns in a wide arc and cannot turn on the spot.

| Option | What changes | For | Against |
| --- | --- | --- | --- |
| A. Independent direction per side (skid steer / tank) | One direction signal per side: one more GPIO, or a separate motor controller | Turns on the spot; standard for ROS 2 (diff_drive); no new mechanics | Wheels skid when turning, which makes encoder odometry less precise |
| B. Car-like steering (Ackermann) | A servo turns the front wheels; the rear wheels drive | Smooth turns, less tyre wear | Cannot turn on the spot, larger turning radius, new mechanics and harder navigation |

**Recommendation:** option A. It needs almost no mechanical change and suits SLAM navigation in tight rooms. The ESP32-CAM has no free pins left, which is one more reason to move the motors to a separate controller on the new PCB (section 7).

## 6. Knowing the wheels really turn

The motors are controlled in open loop: the robot sends a PWM value and assumes the wheels turn. As the battery drains, the same value produces less force, and the car can sit still with the motors buzzing indefinitely.

**Encoders** fix this. Each motor gets a sensor that counts wheel rotations. With them the robot can:

- Control **speed instead of PWM**: a PID loop raises the PWM until the wheel reaches the requested speed, so a weaker battery is compensated automatically.
- **Detect a stall:** the command is high but the wheel does not turn. The robot then tries more power, then backs up, and if it is still stuck it stops and reports it instead of burning the motors.
- Measure **how far it moved**, which SLAM needs (odometry).

| Option | Notes |
| --- | --- |
| Gear motors with built-in Hall encoders (e.g. JGA25-370 or N20 with encoder) | Recommended: encoder already aligned with the motor, two channels give direction too |
| Optical slot sensor and slotted disc on the existing wheels | Cheapest way to keep the current motors; one channel only, lower resolution |
| Current sensor (e.g. INA219) per side | Extra: a stalled motor draws much more current, a second way to detect being stuck |

The L293D also wastes about 2 V of the battery as heat. A modern MOSFET driver such as the TB6612FNG or DRV8833 gives the motors more of the battery voltage and runs cooler.

## 7. A PCB instead of wires

The current robot is built on protoboards with hundreds of wires. A custom PCB, designed in KiCad and ordered from a PCB manufacturer, would hold:

- Power: battery input with a fuse and reverse-polarity protection, the 5 V supply for the Pi and servos, and a voltage divider so the robot can measure its own battery.
- Motor drivers (TB6612FNG or DRV8833) and a microcontroller for the motors and encoders (an ESP32-S3 or RP2040).
- The pump MOSFET with its flyback diode, and the gate resistors already in place.
- Locking connectors (JST) for motors, servos, pump, sensors and LiDAR, and a header for the Raspberry Pi.

Fewer wires means fewer intermittent faults, and the design can be documented, shared and rebuilt exactly.

## 8. A chassis that does not bend

The printed chassis bends in the middle because the heaviest parts — battery, water and Raspberry Pi — sit between the axles, where the plate has the least support. For the next version:

- Put the battery and the water tank over or between the wheels, as low as possible, so the weight goes straight into the axles.
- Add ribs or a box section along the length of the chassis instead of a flat plate.
- Print in PETG or nylon instead of PLA, or use an aluminium or carbon plate as the base with printed parts on top.
- Check the design with a simple load simulation in Fusion 360 before printing.

## 9. Water level

| Sensor | What it measures | Notes |
| --- | --- | --- |
| Non-contact capacitive sensor (e.g. XKC-Y25) | Water present or not at one height | Glued outside the tank, nothing touches the water. Two of them give "low" and "full". |
| Float switch | Water above or below one level | Simple and cheap, but moving parts inside the tank. |
| Waterproof ultrasonic sensor (e.g. JSN-SR04T) | Continuous level | Gives a percentage, but needs some height above the water. |
| Flow sensor (e.g. YF-S401) on the pump outlet | Water used | Counts how much was sprayed; combined with a level sensor it detects a blocked hose. |

**Recommendation:** two capacitive sensors (low and full). They are reliable, have no moving parts and are enough for the mission logic: "full", "enough", "go and refill".

## 10. The refill station

A fixed station where the robot can refill its tank on its own:

- **Position:** the station has a fixed place on the SLAM map. For the last centimetres, an ArUco/AprilTag marker seen by the camera aligns the robot precisely.
- **Docking detection:** a limit switch or a Hall sensor and magnet confirms the robot is in place before any water flows.
- **Filling:** the station has its own tank and a solenoid valve (or a small pump) above a funnel on the robot. A small ESP32 on the station opens the valve when the robot asks for it over WiFi, and closes it when the robot’s "full" sensor triggers.
- **Optional later:** charging contacts (pogo pins) on the same dock so the robot can also charge.

## 11. The mission

With a map, a water level and a station, the robot’s behaviour becomes a mission instead of a single reaction:

| Situation | What the robot does |
| --- | --- |
| Patrolling | Follows a route through the map, looking for flames with the colour and thermal cameras. |
| Flame confirmed, water available | Stops, aims, sprays, checks again, and continues the patrol. |
| Water low | Stops patrolling and plans a path to the refill station. |
| Flame seen on the way to the station | Marks the flame’s position on the map (robot position plus the direction and distance from the camera and LiDAR) and keeps going to refill. |
| After refilling | Goes to the marked flames first, nearest first, before resuming the patrol. |
| Stuck or lost | Reports it and waits, instead of driving the motors indefinitely. |

## 12. Materials

| Item | Purpose | Example |
| --- | --- | --- |
| 2D LiDAR | Map and navigation, day and night | LDROBOT LD19 or Slamtec RPLIDAR C1 |
| IMU | Rotation and acceleration for odometry | BNO085 or MPU-6050 |
| Thermal camera | Confirm real heat, reject reflections, see at night | MLX90640 (cheap) or FLIR Lepton 3.5 |
| NoIR camera + infrared LEDs (optional) | Camera image in the dark | Raspberry Pi Camera Module 3 NoIR |
| Gear motors with encoders (×4) | Speed control and stall detection | JGA25-370 with Hall encoder |
| Motor drivers | Replace the L293D | TB6612FNG or DRV8833 |
| Motor microcontroller | Motors, encoders, sensors; frees the camera board | ESP32-S3 or RP2040 |
| Current sensors (optional) | Second way to detect a stall | INA219 |
| Water level sensors (×2) | Know when to refill | XKC-Y25 capacitive |
| Flow sensor (optional) | Water sprayed, blocked hose | YF-S401 |
| Custom PCB | Replace the wiring | KiCad design, JST connectors |
| Chassis material | Stiffer base | PETG / nylon, or aluminium plate |
| Station: tank, solenoid valve, ESP32, funnel | Refilling | 12 V solenoid valve, small ESP32 board |
| Station: docking sensor and marker | Know the robot is in place | Limit switch or Hall sensor and magnet; printed AprilTag |
| Battery | More capacity for LiDAR and Pi | Larger 2S pack with a BMS |

Choose the exact parts after checking current availability, voltages and Linux/ROS 2 support.

## 13. Suggested order

| Step | Work | Why at this point |
| --- | --- | --- |
| 1 | Encoders, new motor drivers and closed-loop speed | Everything else depends on the robot moving reliably |
| 2 | Independent direction per side | Small change, needed for navigation |
| 3 | LiDAR, IMU and SLAM with ROS 2 | Build and save a map, then drive to points on it |
| 4 | Thermal camera and fusion with FireNet | Fixes reflections, adds night detection |
| 5 | Water level sensors | Simple, and the mission needs them |
| 6 | Refill station and the mission logic | Needs map, navigation and water level working |
| 7 | PCB and new chassis | Once the parts are chosen and tested on protoboard |
