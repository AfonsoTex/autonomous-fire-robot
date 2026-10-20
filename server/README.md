# PC server — driving and line following

`unified_python_server - UDP.py` is the program that runs on the PC. It receives the
ESP32-CAM video, reads the Xbox controller, follows the line on its own when asked to,
and sends the drive commands back to the car.

## Run

```bash
pip install opencv-python numpy pygame
python "server/unified_python_server - UDP.py"
```

Power the car; it finds the PC automatically. Press **A** on the controller to switch
between manual driving and line following. Two windows show the camera and the
processed strip the line follower looks at. Run only one copy at a time and allow UDP
ports 1883 and 1884 through the PC firewall.

## How it is organised

Three threads share the work:

| Thread | Job |
| --- | --- |
| Video receive | Reads UDP packets on port 1884 and rebuilds each JPEG using its start/end markers (`FF D8` / `FF D9`). Keeps only the newest frame. |
| Processing | Runs the line-following pipeline on the newest frame and writes the wanted speed and steering. |
| Main | Reads the controller, chooses manual or automatic commands, and talks to the ESP32 on UDP port 1883. |

## Messages to and from the ESP32

| Message | Direction | Meaning |
| --- | --- | --- |
| `HELLO` | ESP32 → PC | "I am here" (broadcast). The PC learns the car's address from it. |
| `MOV:x,DIR:y` | PC → ESP32 | Speed `x` and steering `y`, both from -1 to 1. Sent when the command changes. |
| `HB` | PC → ESP32 | Heartbeat every 0.2 s when nothing changed. Without it the car stops after 1 s. |
| `SEND_MOV` | ESP32 → PC | "Send me your current command" after a reconnect. |

## Line following

For each frame, in the processing thread:

1. Keep a strip from 80% to 95% of the image height (the floor just ahead).
2. Grayscale, then **adaptive threshold** (block 151, C 15), so uneven lighting does not break it.
3. **Opening** with an 11×11 ellipse removes small specks.
4. Find contours and reject the ones that are too small (`MIN_AREA`), too wide (`MAX_WIDTH_PCT`, usually reflections), too short, or more than 150 px away from where the line was last seen.
5. The line's centre gives the error from the image centre. Inside `DEADBAND` the car goes straight; outside it, steering = error × `GAIN` / (half the image width), limited to ±1.
6. Speed is `BASE_SPEED`. In tight curves (steering above `CURVE_THRESHOLD`) it rises gradually up to `MAX_CURVE_SPEED`, so the outer wheels have enough force to turn.
7. If the line is lost, the car **reverses** at `REVERSE_SPEED`, steering towards the side where the line was last seen, until it finds it again.

## Settings

All values are at the top of the script:

| Setting | What it does |
| --- | --- |
| `GAIN` | Steering strength. Raise if the car does not turn enough; lower if it snakes. |
| `DEADBAND` | Pixels around the centre where small errors are ignored. |
| `BASE_SPEED` | Speed on straights. |
| `MAX_CURVE_SPEED`, `CURVE_THRESHOLD` | Extra speed in tight curves, and from which steering level it starts. |
| `REVERSE_SPEED` | Speed while reversing to find a lost line. |
| `MIN_AREA`, `MAX_WIDTH_PCT` | Size limits that separate the line from noise and reflections. |
| `HEARTBEAT_SECONDS` | How often the heartbeat is sent. |

The minimum speed that makes the motors turn rises as the battery drains, so the speed
values may need adjusting during a session.
