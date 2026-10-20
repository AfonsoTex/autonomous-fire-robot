#pragma once

// Servos typically receive one control pulse every 20 ms (20000 us).
constexpr int SERVO_PERIOD_US = 20000;

// Initial control range: 1000 to 2000 us.
// A pulse width of 1500 us requests the middle position.
// Verify the limits on the actual servo and assembly.
constexpr int MIN_PULSE_US = 1000;
constexpr int CENTER_PULSE_US = 1500;
constexpr int MAX_PULSE_US = 2000;

constexpr int GPIO_PAN = 18;
constexpr int GPIO_TILT = 19;

// Returns true on success or false on failure.
bool initializeServos(int pan_us = CENTER_PULSE_US, int tilt_us = CENTER_PULSE_US);

// Selects the GPIO pin and requests a servo position using pulse width.
// gpio: BCM GPIO number, not the physical connector pin number.
// pulse_us: pulse width in microseconds, within our 1000-2000 us range.
// Returns true if accepted or false if validation or pulse generation fails.
bool setServoPulse(int gpio, int pulse_us);

// Stops the pulses.
void shutdownServos();
