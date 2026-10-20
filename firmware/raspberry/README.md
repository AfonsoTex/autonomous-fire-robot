# Raspberry Pi — FireNet

This folder contains the Linux C++ program that captures the Raspberry Pi camera,
runs the FireNet model, draws detections, and streams the result to a web browser.
With `--aim` it also stops the car, aims the pan/tilt nozzle at a flame and, with
`--pump`, sprays short water pulses. It runs from an SSH terminal; no monitor is needed.

The program uses the camera connected to the Raspberry Pi through libcamera.
It does not receive the ESP32 camera's UDP stream.

## Settings

Almost every tunable value lives in [`settings.h`](settings.h): confidence and NMS
thresholds, aiming speed, confirmation counts, waiting times, spray duration, attempt
limits, car stop/resume timing and the reflection filter. Each value has a comment
explaining what raising or lowering it does. The Python training tools read the same
file, so there is only one place to edit.

`FAST_TEST_MODE` replaces several timing and confirmation values with a faster preset
(see `getResponseSettings()` at the end of `settings.h`). Check this switch before
comparing behaviour with the individual values.

After editing `settings.h`, copy it to the Raspberry Pi and rebuild. Values that are
**not** in `settings.h`:

- The aim calibration table: `AIM_CALIBRATION` in `aim_controller.cpp`.
- Pins: pump in `pump.h`, servos in `servos.h`, car permission in `car_control.h`.
- Servo pulse limits in `servos.h`, HTTP port and model path in `main.cpp`.
- Home pulses and directions: command-line arguments.

## How to read the files

A `.h` file is a header: it declares the functions, classes, or constants that
other files can use. A `.cpp` file contains their implementation. `main.cpp`
connects these parts into one program.

## File guide

| File | Purpose |
| --- | --- |
| `main.cpp` | Entry point. Reads the command-line options, opens the camera, loads the model, starts the HTTP server and runs the capture → inference → control loop. Draws detections and status labels, publishes JPEG frames, and turns everything off on Ctrl+C or an error. |
| `settings.h` | Shared settings for the Raspberry program and the Python training tools (see above). |
| `camera.h` / `camera.cpp` | Captures 640×480 NV12 at 30 fps through libcamera/GStreamer, converts it for OpenCV, rotates it 180° and returns full-resolution RGB. `main.cpp` resizes a separate copy to 180×180 for the model. |
| `inference.h` / `inference.cpp` | Loads the model with ONNX Runtime, scales RGB to 0–1 in a `[1, 3, 180, 180]` tensor, and returns the raw 45×45 grid. Each cell (`Prediction`) holds presence, x/y offsets, width and height. |
| `detection_filter.h` / `detection_filter.cpp` | Removes overlapping duplicate boxes (NMS) using `NMS_IOU_THRESHOLD`, matching the Python evaluation. |
| `aim_controller.h` / `aim_controller.cpp` | Selects and confirms a target, moves the servos towards the calibrated aim point, decides when to request water, returns home, and filters reflections after spraying. Contains the calibration table. |
| `car_control.h` / `car_control.cpp` | Drives the movement-permission wire (BCM GPIO23 → ESP32 GPIO2): stops the car when flames are seen and releases it when the scene is clear and aiming is finished. |
| `servos.h` / `servos.cpp` | Hardware PWM for pan (GPIO18) and tilt (GPIO19) through `/sys/class/pwm/pwmchip1`, 20 ms period, 1000–2000 µs pulses. |
| `pump.h` / `pump.cpp` | Timed pump pulses on BCM GPIO17 with `lgpio`. The pulse is timed by the library, so it does not block video. |
| `video_server.h` / `video_server.cpp` | Small HTTP server: HTML page at `/`, MJPEG stream at `/stream.mjpg`, up to four viewers. Only the newest frame is kept. |
| `firenet.onnx` | The exported network. Must match the input/output layout in `inference.cpp`. |

## Image flow

```text
Camera → 640×480 RGB (orientation corrected)
           ├── resize copy to 180×180 → FireNet → NMS → detections → car stop / aiming / water
           └── convert to BGR → draw detections and status → JPEG → browser
```

Closing the browser does not stop inference.

## Build on the Raspberry Pi

Requirements: a C++17 compiler, OpenCV with GStreamer support, libcamera's
GStreamer source, ONNX Runtime, and the lgpio development files. Set `ORT` to the
ONNX Runtime folder (containing `include/` and `lib/`) and run from this folder:

```bash
g++ -std=c++17 -O2 -pthread main.cpp camera.cpp inference.cpp video_server.cpp servos.cpp aim_controller.cpp pump.cpp detection_filter.cpp car_control.cpp \
  -I"$ORT/include" \
  -L"$ORT/lib" -Wl,-rpath,"$ORT/lib" \
  $(pkg-config --cflags --libs opencv4) \
  -lonnxruntime -llgpio -o firenet
```

## Run

```text
./firenet                                                         video only
./firenet --aim PAN_HOME_US TILT_HOME_US PAN_DIRECTION TILT_DIRECTION          aiming
./firenet --aim PAN_HOME_US TILT_HOME_US PAN_DIRECTION TILT_DIRECTION --pump   aiming + water
```

Example: `./firenet --aim 1450 1400 -1 -1 --pump`. Keep `firenet.onnx` in the
current folder. Then open `http://BaraoForrester.local:8080/` (or the Pi's IP) on a
PC in the same network. Press Ctrl+C to stop.

- Home pulses (1000–2000 µs) must point the camera at the ground/search area. They depend on the assembly; 1500 is not automatically the right position.
- Directions are `1` or `-1`. Choose pan so a flame on the right moves towards the centre, and tilt so a flame below the centre moves towards the centre (after the 180° image rotation).
- Video-only mode never grants movement permission, so the ESP32 keeps the car stopped.

The servo backend needs the `pwm-2chan` overlay (`sudo dtoverlay pwm-2chan`, which
lasts only until reboot) with GPIO18/19 as `pwmchip1` channels 2/3. On the current
setup `pwmchip0` is the fan controller. Recheck the path after reboots or OS updates.

## What happens when a flame appears

1. **Stop the car.** After `CAR_FLAME_CONFIRM_FRAMES` consecutive frames with a
   flame, GPIO23 goes LOW and the ESP32 stops the motors.
2. **Settle.** Wait `CAR_SETTLE_SECONDS`; only frames captured after that count.
3. **Confirm the target.** Pick the flame nearest the image centre and confirm it in
   `REQUIRED_DETECTIONS` of the last `CONFIRMATION_WINDOW` frames. Its initial
   vertical position selects the aim point from the calibration table.
4. **Aim.** Move pan/tilt in steps (size set by `AIM_SPEED_LEVEL`), waiting
   `MOVE_SETTLE_SECONDS` after each one, until the flame is within
   `CENTER_DEADBAND` of the aim point.
5. **Spray** (only with `--pump`). After `REQUIRED_CENTERED_FRAMES` centred frames
   lasting `CENTERED_BEFORE_PUMP_SECONDS`, send one pulse of `PUMP_SPRAY_SECONDS`.
   Servos stay still during the pulse. Retry after `PUMP_RETRY_COOLDOWN_SECONDS`, up
   to `MAX_PUMP_ATTEMPTS` per target.
6. **Return home** when the target has been missing for `MAX_MISSING_SECONDS`, then
   wait `HOME_SETTLE_SECONDS`.
7. **Release the car** when there are `CAR_NO_FLAME_FRAMES` clear frames spanning
   `CAR_NO_FLAME_SECONDS`, the camera is home and no pulse is running.

Missing detections do not prove that a flame is out; they only mean the model no
longer sees it.

### Safety behaviour

- Normal exit, Ctrl+C and reported errors turn the pump off, stop the servos and set the permission wire LOW.
- A processing gap longer than `CAR_NO_FLAME_SECONDS` stops the car and restarts the clear-frame count. A gap longer than `MAX_MISSING_SECONDS` sends the camera home.
- If a correction is needed but a servo is already at its pulse limit, the controller reports `Aim unreachable`, does not spray and returns home. That target is then ignored at home (it is followed as the car moves) until it has been missing for `IGNORED_FLAME_FORGET_SECONDS`.
- The permission wire is a static level, not a heartbeat: a frozen process could leave it HIGH. The ESP32's own PC watchdog still applies.

### Reflection filter after spraying

Water on the floor can reflect the flame. After a pulse, when the camera is home again,
new detections within `REFLECTION_IGNORE_RADIUS` of the sprayed target are ignored
unless they match a flame already seen there before spraying
(`POST_SPRAY_MATCH_DISTANCE`). The filter ends `REFLECTION_IGNORE_SECONDS` after the
car is allowed to move again. This is position matching only; it cannot truly tell a
reflection from a real flame.

## Movement-permission wiring

Connect Raspberry physical pin 16 (BCM GPIO23) to ESP32 GPIO2 and connect the grounds
(e.g. Raspberry pin 6 to ESP GND). It is a 3.3 V logic signal. HIGH lets the ESP32
obey the PC; LOW or a disconnected wire stops the motors (the ESP input has a
pull-down). Disconnect the wire when programming the ESP32, because GPIO2 must not be
HIGH in download mode. The permission pin must not share the pump (17), pan (18) or
tilt (19) pin; `car_control.cpp` refuses to start if it does.

## Aim calibration

The nozzle is not at the camera's position, so centering the flame in the image would
miss. Instead, `AIM_CALIBRATION` in `aim_controller.cpp` maps the flame's **initial
vertical position** (measured at home, before the servos move) to the image point
where the flame should end up. A flame higher in the image is usually farther away.
This is an empirical image-space table, not a distance measurement.

- Each row is `{initialFlameY, desiredX, desiredY}` in image fractions (0 = left/top, 1 = right/bottom). Keep rows sorted by Y.
- Between rows the values are interpolated; outside the table the nearest row is used.
- `HORIZONTAL_AIM_ADJUSTMENT` and `VERTICAL_AIM_ADJUSTMENT` in `settings.h` shift every target. Final targets are limited to 0.05–0.95.
- `USE_VERTICAL_AIM_CALIBRATION = false` uses the fixed `AIM_HORIZONTAL_POSITION` / `AIM_VERTICAL_POSITION` instead.

Calibrating:

1. Keep the same home pulses, mounting and candle height on level ground.
2. Run with `--aim ... --pump`. After confirmation the terminal prints
   `Aim calibration: initial Y=...` with the chosen target, and before each pulse
   `Aim before water request: ...` with the flame position and servo pulses.
3. With the current mounting: if the jet passes **right** of the flame, increase that
   row's X; if it passes **left**, decrease it. If the jet passes **above**, decrease
   Y; if it passes **below**, increase Y.
4. Add or adjust the row for that initial Y, rebuild and test again at several distances.

Remounting the camera or nozzle invalidates the table. The servos have no position
feedback, so a jam or a mechanical stop before the pulse limit cannot be detected.

### Aiming speed levels

`AIM_SPEED_LEVEL` in `settings.h` (fast mode always uses level 4):

| Level | Near gain / max step | Far gain / max step | Notes |
| --- | --- | --- | --- |
| 1 | 60 / 12 µs | 60 / 12 µs | Slowest |
| 2 | 60 / 12 µs | 120 / 24 µs | Faster far away |
| 3 | 90 / 18 µs | 120 / 24 µs | Also faster small corrections |
| 4 | 120 / 24 µs | 180 / 36 µs | Fastest; more overshoot risk |

An axis is "far" when its error is more than 15% of the image. Inside the deadband no
correction is made.

### Reading the aiming code

Start with `update()` in `aim_controller.cpp`. It follows the cycle in order: finish
returning home, wait for settling, find the target, confirm it, and calculate the next
correction.

- `availableFlames()` removes ignored (unreachable) targets and post-spray reflections.
- `findMatchingFlame()` chooses near the image centre first, then near the last target position. Two nearby flames can still be confused.
- `rememberDetection()` keeps the recent yes/no observations used for confirmation.
- `aimAtSelectedFlame()` and `calculatePulseChange()` turn the position error into servo steps.
- `handleMissingFlame()` tolerates brief detection gaps before returning home.
- `moveHome()` returns in small steps.

## Status

The program runs on the Raspberry Pi 5 with the real camera, servos and pump, and has
been tested with candles. Detection quality and aiming accuracy depend on the scene,
lighting and calibration; this is an experimental prototype, not a safety device.
