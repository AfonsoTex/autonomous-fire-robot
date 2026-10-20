#pragma once

#include "settings.h"

constexpr int GPIO_PUMP = 17;

// GPIO 17 controls the existing pump driver: HIGH = on, LOW = off.
// lgpio expects microseconds. Edit PUMP_SPRAY_SECONDS in settings.h.
constexpr int PUMP_ON_MICROSECONDS = static_cast<int>(PUMP_SPRAY_SECONDS * 1000000);
static_assert(PUMP_ON_MICROSECONDS > 0, "Pump spray duration must be positive");

// Opens the GPIO controller and reserves the pump pin as an output, LOW.
// Returns true on success or false on failure.
bool initializePump();

// true turns the pump on, false turns it off.
// Returns true if accepted or false on failure.
bool setPumpEnabled(bool enabled);

// Starts one HIGH pulse for PUMP_SPRAY_SECONDS, then LOW. Does not block the video loop.
// lgpio times the pulse independently, even if frame processing is delayed.
bool startPumpPulse();

// Reports whether the timed pulse is still running. Throws if the GPIO check fails.
bool pumpIsRunning();

// Turns the pump off and closes the controller.
void shutdownPump();
