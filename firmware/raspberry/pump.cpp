#include "pump.h"

#include <lgpio.h>
#include <stdexcept>

// Stores the GPIO controller handle. -1 means it is not open.
static int gpioHandle = -1;

bool initializePump()
{
    gpioHandle = lgGpiochipOpen(0);
    if (gpioHandle < 0) {
        return false;
    }

    // The last argument is the initial level: 0 keeps the pump off.
    int result = lgGpioClaimOutput(gpioHandle, 0, GPIO_PUMP, 0);
    if (result < 0) {
        lgGpiochipClose(gpioHandle);
        gpioHandle = -1;
        return false;
    }

    return true;
}

bool setPumpEnabled(bool enabled)
{
    if (gpioHandle < 0) {
        return false;
    }

    // Cancel the timed pulse before writing LOW, so it cannot turn the output back on.
    int stopResult = 0;
    if (!enabled) {
        // lgpio rejects a zero-length pulse when there is no active pulse to cancel.
        int busy = lgTxBusy(gpioHandle, GPIO_PUMP, LG_TX_PWM);
        if (busy > 0) {
            stopResult = lgTxPulse(gpioHandle, GPIO_PUMP, 0, 0, 0, 0);
            // The pulse may finish between the busy check and the cancel request.
            if (stopResult == LG_BAD_PWM_MICROS &&
                lgTxBusy(gpioHandle, GPIO_PUMP, LG_TX_PWM) == 0) {
                stopResult = 0;
            }
        } else if (busy < 0) {
            stopResult = busy;
        }
    }

    int level;
    if (enabled) {
        level = 1;
    } else {
        level = 0;
    }

    int result = lgGpioWrite(gpioHandle, GPIO_PUMP, level);
    if (result < 0 || stopResult < 0) {
        return false;
    }

    return true;
}

bool startPumpPulse()
{
    if (gpioHandle < 0) {
        return false;
    }

    // Arguments: controller, GPIO, HIGH duration, LOW duration, offset, cycle count.
    // One cycle means one configured spray, then off; 0 cycles would repeat indefinitely.
    int result = lgTxPulse(gpioHandle, GPIO_PUMP, PUMP_ON_MICROSECONDS, 1, 0, 1);
    if (result < 0) {
        setPumpEnabled(false);
        return false;
    }
    return true;
}

bool pumpIsRunning()
{
    int result = lgTxBusy(gpioHandle, GPIO_PUMP, LG_TX_PWM);
    if (result < 0) {
        throw std::runtime_error("Could not check the pump pulse.");
    }
    return result > 0;
}

void shutdownPump()
{
    if (gpioHandle < 0) {
        return;
    }

    setPumpEnabled(false);
    lgGpiochipClose(gpioHandle);
    gpioHandle = -1;
}
