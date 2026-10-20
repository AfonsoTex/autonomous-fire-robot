#include "car_control.h"
#include "pump.h"
#include "servos.h"

#include <lgpio.h>
#include <iostream>

namespace {
const ResponseSettings response = getResponseSettings(FAST_TEST_MODE);
}

bool CarControl::initialize(double currentTime)
{
    settleUntil = currentTime + response.carSettleSeconds;
    if (CAR_PERMISSION_GPIO < 0) {
        return true;
    }
    // Do not accidentally use an output already connected to an actuator.
    if (CAR_PERMISSION_GPIO == GPIO_PUMP || CAR_PERMISSION_GPIO == GPIO_PAN ||
        CAR_PERMISSION_GPIO == GPIO_TILT) {
        return false;
    }
    gpioHandle = lgGpiochipOpen(CAR_GPIO_CHIP);
    if (gpioHandle < 0) {
        return false;
    }
    if (lgGpioClaimOutput(gpioHandle, 0, CAR_PERMISSION_GPIO, 0) < 0) {
        lgGpiochipClose(gpioHandle);
        gpioHandle = -1;
        return false;
    }
    return true;
}

bool CarControl::setMovementAllowed(bool allowed, const char* reason)
{
    if (allowed == movementAllowed) {
        return true;
    }
    int level = 0;
    if (allowed) {
        level = 1;
    }
    if (gpioHandle >= 0 && lgGpioWrite(gpioHandle, CAR_PERMISSION_GPIO, level) < 0) {
        return false;
    }
    movementAllowed = allowed;
    if (allowed) {
        std::cout << "Car: movement permitted (" << reason << ")." << std::endl;
    } else {
        std::cout << "Car: stopped (" << reason << ")." << std::endl;
    }
    return true;
}

bool CarControl::update(bool flameVisible, bool aimingBusy, double currentTime)
{
    // A pause in processing is not evidence that the scene stayed free of flames.
    // Start counting clear frames again and request a stop when processing resumes.
    if (lastFrameTime >= 0 && currentTime - lastFrameTime > CAR_NO_FLAME_SECONDS) {
        flameFrames = 0;
        emptyFrames = 0;
        noFlameSince = -1;
        settleUntil = currentTime + response.carSettleSeconds;
        if (movementAllowed) {
            std::cout << "Car: stopped (camera processing paused for more than 1 second)." << std::endl;
        }
        if (!stop()) {
            return false;
        }
    }
    lastFrameTime = currentTime;

    if (flameVisible) {
        if (flameFrames < response.carFlameFrames) {
            flameFrames++;
        }
        emptyFrames = 0;
        noFlameSince = -1;
    } else {
        flameFrames = 0;
        if (noFlameSince < 0) {
            noFlameSince = currentTime;
        }
        if (emptyFrames < response.clearFrames) {
            emptyFrames++;
        }
    }

    // Keep the car stopped throughout tracking, spraying, returning and settling.
    if (aimingBusy) {
        if (movementAllowed) {
            settleUntil = currentTime + response.carSettleSeconds;
        }
        return setMovementAllowed(false, "aiming, returning or settling");
    }

    // Stop after the required consecutive frames; a clear frame resets the count above.
    if (flameFrames >= response.carFlameFrames) {
        if (movementAllowed) {
            settleUntil = currentTime + response.carSettleSeconds;
        }
        return setMovementAllowed(false, "flame detection reached the stop threshold");
    }

    if (emptyFrames < response.clearFrames) {
        return true;
    }
    if (currentTime - noFlameSince < response.clearSeconds) {
        return true;
    }
    return setMovementAllowed(true, "no flames and camera ready");
}

bool CarControl::readyToAim(double currentTime) const
{
    // Let the chassis stop vibrating before using frames to control the servos.
    return !movementAllowed && currentTime >= settleUntil;
}

bool CarControl::stop()
{
    // Write LOW even if our stored state already says stopped: this also retries errors.
    if (gpioHandle >= 0 && lgGpioWrite(gpioHandle, CAR_PERMISSION_GPIO, 0) < 0) {
        return false;
    }
    movementAllowed = false;
    return true;
}

CarControl::~CarControl()
{
    stop();
    if (gpioHandle >= 0) {
        lgGpiochipClose(gpioHandle);
    }
}
