#pragma once

#include "settings.h"

// BCM GPIO23 (physical pin 16) connects to GPIO2 on the ESP32-CAM; share GND.
// Set -1 to leave the output unconfigured; the ESP will not receive permission.
constexpr int CAR_PERMISSION_GPIO = 23;
constexpr int CAR_GPIO_CHIP = 0;

// HIGH permits movement; LOW tells the ESP32 to keep the motors stopped.
class CarControl {
public:
    bool initialize(double currentTime);
    bool update(bool flameVisible, bool aimingBusy, double currentTime);
    bool readyToAim(double currentTime) const;
    // This is permission to move, not a measurement of actual wheel movement.
    bool isMovementAllowed() const { return movementAllowed; }
    bool stop();
    ~CarControl();

private:
    bool setMovementAllowed(bool allowed, const char* reason);
    int gpioHandle = -1;
    bool movementAllowed = false;
    int flameFrames = 0;
    int emptyFrames = 0;
    double noFlameSince = -1;
    double settleUntil = 0;
    double lastFrameTime = -1;
};
