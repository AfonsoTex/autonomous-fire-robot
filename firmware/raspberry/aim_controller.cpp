#include "aim_controller.h"
#include "servos.h"
#include "detection_filter.h"
#include "settings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace {
const ResponseSettings response = getResponseSettings(FAST_TEST_MODE);
// Use the actual image center when choosing the first flame to follow.
constexpr float IMAGE_CENTER = 0.5f;
// Common aiming controls are in settings.h; measured calibration points stay below.

struct AimCalibrationPoint {
    float initialFlameY;       // Flame center before the camera starts aiming: 0 = top, 1 = bottom.
    float horizontalPosition; // Desired horizontal flame position after aiming.
    float verticalPosition;   // Desired vertical flame position after aiming.
};

// Empirical image-space calibration for the current camera/nozzle mounting.
// Recalibrate after changing the mounting, home tilt or candle height.
// Keep rows ordered by increasing Y, with the same home tilt and camera mounting.
// These are image coordinates, not distances in centimetres.
constexpr std::array<AimCalibrationPoint, 19> AIM_CALIBRATION = {{
    // Initial Y, table X, table Y

    {0.0102057f, 0.540f, 0.44f},
    {0.0200000f, 0.560f, 0.47f},
    {0.0298382f, 0.530f, 0.44f},

    {0.0400000f, 0.530f, 0.44f},

    {0.0509159f, 0.535f, 0.482314f},
    {0.0554785f, 0.530f, 0.50f},
    {0.0570346f, 0.541854f, 0.50f},

    {0.0756257f, 0.546132f, 0.50f},

    {0.0806615f, 0.560f, 0.47f},

    {0.1003940f, 0.549356f, 0.465638f},

    {0.1218830f, 0.559203f, 0.470394f},

    {0.1243810f, 0.554767f, 0.476528f},

    {0.1448330f, 0.557907f, 0.485813f},

    {0.2129450f, 0.565685f, 0.479014f},

    {0.2339360f, 0.548f, 0.485f},

    {0.2820860f, 0.567786f, 0.483653f},

    {0.3657410f, 0.590f, 0.50f},

    {0.4346060f, 0.550f, 0.49f},
    {0.4558870f, 0.550f, 0.49f},
}};

AimCalibrationPoint aimForInitialY(float initialY)
{
    if (!USE_VERTICAL_AIM_CALIBRATION) {
        return {initialY, AIM_HORIZONTAL_POSITION, AIM_VERTICAL_POSITION};
    }
    if (initialY <= AIM_CALIBRATION.front().initialFlameY) {
        return AIM_CALIBRATION.front();
    }

    for (std::size_t i = 1; i < AIM_CALIBRATION.size(); i++) {
        const AimCalibrationPoint& previous = AIM_CALIBRATION[i - 1];
        const AimCalibrationPoint& next = AIM_CALIBRATION[i];
        if (initialY <= next.initialFlameY) {
            // Halfway between two table Y values means halfway between their aim positions.
            float fraction = (initialY - previous.initialFlameY) /
                             (next.initialFlameY - previous.initialFlameY);
            AimCalibrationPoint aim;
            aim.initialFlameY = initialY;
            aim.horizontalPosition = previous.horizontalPosition +
                fraction * (next.horizontalPosition - previous.horizontalPosition);
            aim.verticalPosition = previous.verticalPosition +
                fraction * (next.verticalPosition - previous.verticalPosition);
            return aim;
        }
    }

    // Outside the table, keep the nearest endpoint instead of inventing a larger correction.
    return AIM_CALIBRATION.back();
}
// Internal pulse limits used by response.aimSpeedLevel.
constexpr float PULSE_GAIN = 60.0f;
constexpr int MAX_PULSE_STEP_US = 12;
constexpr float FAST_AIM_DISTANCE = 0.15f;
constexpr float FAST_PULSE_GAIN = 120.0f;
constexpr int MAX_FAST_PULSE_STEP_US = 24;
}

AimController::AimController(int homePan, int homeTilt,
                             int horizontalDirection, int verticalDirection)
{
    // pan and tilt will change as we aim at the flame.
    // At startup, we want them to equal the home pulses received as parameters.
    // These assignments only store values; they do not physically move the servos.
    pan = homePan;
    tilt = homeTilt;

    // Keep the home pulses unchanged so we know where to return after aiming.
    homePanPulse = homePan;
    homeTiltPulse = homeTilt;

    // Store the correction direction for each servo, according to its mounting.
    panDirection = horizontalDirection;
    tiltDirection = verticalDirection;
}

const char* AimController::status() const
{
    if (state == State::Search) {
        return "Confirming flame";
    }
    if (state == State::Track) {
        return "Aiming";
    }
    if (state == State::Hold) {
        if (pumpAttempts >= MAX_PUMP_ATTEMPTS) {
            return "Target centered - attempt limit reached";
        }
        if (pumpAttempts > 0) {
            return "Target centered - waiting to retry";
        }
        return "Target centered";
    }
    if (state == State::Return) {
        return "Returning to search position";
    }
    return "Settling";
}

void AimController::pumpPulseFinished(double currentTime)
{
    if (!waitingForPumpToFinish) {
        return;
    }

    waitingForPumpToFinish = false;
    nextPumpAttemptTime = currentTime + response.pumpRetrySeconds;
}

void AimController::pumpPulseStarted()
{
    filterSprayReflections = true;
    reflectionIgnoreUntil = -1;
}

void AimController::movementPermitted(double currentTime)
{
    if (filterSprayReflections && reflectionIgnoreUntil < 0) {
        reflectionIgnoreUntil = currentTime + REFLECTION_IGNORE_SECONDS;
    }
}

std::vector<Detection> AimController::availableFlames(
    const std::vector<Detection>& flames, double currentTime)
{
    if (filterSprayReflections && reflectionIgnoreUntil >= 0 &&
        currentTime >= reflectionIgnoreUntil) {
        filterSprayReflections = false;
    }

    if (state != State::Search && state != State::Cooldown) {
        // The camera is away from home. These image coordinates cannot be compared.
        for (std::size_t i = 0; i < ignoredFlames.size(); i++) {
            ignoredFlames[i].missingSince = -1;
        }
        return flames;
    }

    std::vector<bool> skip(flames.size(), false);
    std::vector<IgnoredFlame> stillIgnored;
    for (std::size_t ignoredIndex = 0; ignoredIndex < ignoredFlames.size(); ignoredIndex++) {
        IgnoredFlame ignored = ignoredFlames[ignoredIndex];
        std::size_t closestIndex = flames.size();
        float closestDistance = MAX_TARGET_DISTANCE;
        for (std::size_t i = 0; i < flames.size(); i++) {
            if (skip[i]) {
                continue;
            }
            float distance = std::hypot(flames[i].x - ignored.lastPosition.x,
                                        flames[i].y - ignored.lastPosition.y);
            if (distance <= closestDistance) {
                closestIndex = i;
                closestDistance = distance;
            }
        }

        if (closestIndex < flames.size()) {
            skip[closestIndex] = true;
            // Follow small changes as the car moves, rather than forgetting the
            // target as soon as it leaves its original image position.
            ignored.lastPosition = flames[closestIndex];
            ignored.missingSince = -1;
            stillIgnored.push_back(ignored);
        } else {
            if (state == State::Cooldown) {
                ignored.missingSince = -1;
            } else if (ignored.missingSince < 0) {
                ignored.missingSince = currentTime;
            }
            if (ignored.missingSince < 0 ||
                currentTime - ignored.missingSince < IGNORED_FLAME_FORGET_SECONDS) {
                stillIgnored.push_back(ignored);
            }
        }
    }
    ignoredFlames = stillIgnored;

    // Only compare with the old home view after the camera has settled there.
    // Preserve detections already present before spraying, including other flames.
    if (filterSprayReflections && currentTime >= nextObservationTime) {
        for (std::size_t i = 0; i < flames.size(); i++) {
            float distanceFromSpray = std::hypot(
                flames[i].x - selectedFlameAtHome.x,
                flames[i].y - selectedFlameAtHome.y);
            if (distanceFromSpray > REFLECTION_IGNORE_RADIUS) {
                continue;
            }

            bool seenBeforeSpray = false;
            for (std::size_t j = 0; j < flamesBeforeSprayAtHome.size(); j++) {
                const Detection& previous = flamesBeforeSprayAtHome[j];
                float distance = std::hypot(flames[i].x - previous.x,
                                            flames[i].y - previous.y);
                if (distance <= POST_SPRAY_MATCH_DISTANCE) {
                    seenBeforeSpray = true;
                    break;
                }
            }
            if (!seenBeforeSpray) {
                skip[i] = true;
            }
        }
    }

    std::vector<Detection> available;
    for (std::size_t i = 0; i < flames.size(); i++) {
        if (!skip[i]) {
            available.push_back(flames[i]);
        }
    }
    return available;
}

bool AimController::update(const std::vector<Detection>& flames, double currentTime)
{
    // A request belongs only to this frame; never reuse an old request.
    pumpRequested = false;
    if (lastFrameTime >= 0 && currentTime - lastFrameTime > MAX_MISSING_SECONDS) {
        if (state == State::Track || state == State::Hold) {
            beginReturn(currentTime);
        }
        confirmedFrames = 0;
        recentDetections.clear();
    }
    lastFrameTime = currentTime;

    if (state == State::Return) {
        return moveHome(currentTime);
    }

    if (state == State::Cooldown) {
        if (nextObservationTime < 0) {
            nextObservationTime = currentTime + response.homeSettleSeconds;
        }
        if (currentTime < nextObservationTime) {
            return false;
        }
        state = State::Search;
        confirmedFrames = 0;
        recentDetections.clear();
    }

    if (currentTime < nextObservationTime) {
        return false;
    }

    Detection matchingFlame;
    bool flameFound = findMatchingFlame(flames, matchingFlame);
    if (!flameFound) {
        handleMissingFlame(currentTime);
        return false;
    }

    selectedFlame = matchingFlame;
    lastDetectionTime = currentTime;

    if (state == State::Search) {
        rememberDetection(true);
        if (confirmedFrames < response.requiredDetections) {
            return false;
        }
        // The car has stopped and the camera is still at its home position.
        // Choose the aim point once, before moving either servo. Later Y values
        // change as the camera tilts, so they must not change this calibration.
        AimCalibrationPoint aim = aimForInitialY(selectedFlame.y);
        selectedFlameAtHome = selectedFlame;
        flamesBeforeSprayAtHome = flames;
        // This is a new target and camera movement will change the image coordinates.
        filterSprayReflections = false;
        reflectionIgnoreUntil = -1;
        // Keep the adjusted target inside the image, with room around the edges.
        horizontalAimPosition = std::clamp(aim.horizontalPosition + HORIZONTAL_AIM_ADJUSTMENT,
                                           0.05f, 0.95f);
        verticalAimPosition = std::clamp(aim.verticalPosition + VERTICAL_AIM_ADJUSTMENT,
                                         0.05f, 0.95f);
        std::cout << "Aim calibration: initial Y=" << selectedFlame.y
                  << ", horizontal aim=" << horizontalAimPosition
                  << ", vertical aim=" << verticalAimPosition << std::endl;
        state = State::Track;
    }

    return aimAtSelectedFlame(currentTime);
}

bool AimController::findMatchingFlame(const std::vector<Detection>& flames,
                                      Detection& match) const
{
    float referenceX = IMAGE_CENTER;
    float referenceY = IMAGE_CENTER;
    bool hasSelectedFlame = false;

    // If we have left Search, a flame has already been selected.
    if (state != State::Search) {
        hasSelectedFlame = true;
    }

    // While still in Search, one or more confirmations mean we already
    // have a candidate flame to look for again in the next frame.
    if (confirmedFrames > 0) {
        hasSelectedFlame = true;
    }

    if (hasSelectedFlame) {
        referenceX = selectedFlame.x;
        referenceY = selectedFlame.y;
    }

    bool found = false;
    float closestDistance = 0;
    for (std::size_t i = 0; i < flames.size(); i++) {
        const Detection& flame = flames[i];
        // Check that the coordinates and confidence are finite numbers.
        // If any value is invalid, skip this detection and examine the next one in flames.
        if (!std::isfinite(flame.x) ||
            !std::isfinite(flame.y) ||
            !std::isfinite(flame.confidence)) {
            continue;
        }

        // Ignore detections with insufficient confidence or a center outside the image.
        // Coordinates must be between 0 and 1. Continue with the next detection.
        if (flame.confidence <= CONFIDENCE_THRESHOLD ||
            flame.x < 0 || flame.x > 1 ||
            flame.y < 0 || flame.y > 1) {
            continue;
        }

        float horizontalDistance = flame.x - referenceX;
        float verticalDistance = flame.y - referenceY;

        float distance = std::hypot(horizontalDistance, verticalDistance);
        // If we already selected a flame, ignore detections too far from its last position.
        // They may belong to another flame. Continue with the next detection.
        float matchingDistance = MAX_TARGET_DISTANCE;
        if (filterSprayReflections && state != State::Search) {
            matchingDistance = POST_SPRAY_MATCH_DISTANCE;
        }
        if (hasSelectedFlame && distance > matchingDistance) {
            continue;
        }

        // Compare each flame with the best candidate found so far in this image.
        // Keep the first candidate, then replace it if another is closer to the reference.
        // If both are equally close, prefer the one with higher confidence.
        bool chooseThisFlame = false;

        if (!found) {
            chooseThisFlame = true;
        } else if (distance < closestDistance) {
            chooseThisFlame = true;
        } else if (distance == closestDistance && flame.confidence > match.confidence) {
            chooseThisFlame = true;
        }

        // Store this flame as the best candidate and remember its distance,
        // so the following flames can be compared with it.
        if (chooseThisFlame) {
            match = flame;
            closestDistance = distance;
            found = true;
        }
    }
    return found;
}

void AimController::rememberDetection(bool detected)
{
    recentDetections.push_back(detected);
    if (recentDetections.size() > CONFIRMATION_WINDOW) {
        recentDetections.pop_front();
    }

    confirmedFrames = 0;
    for (bool frameHasFlame : recentDetections) {
        if (frameHasFlame) {
            confirmedFrames++;
        }
    }
}

void AimController::handleMissingFlame(double currentTime)
{
    if (state == State::Search) {
        rememberDetection(false);
    } else {
        // Keep the last target and servo positions while waiting for it to reappear.
        // A missing observation resets centering and prevents a new pulse.
        // main.cpp lets an already-started pulse finish before calling update again.
        if (currentTime - lastDetectionTime >= MAX_MISSING_SECONDS) {
            std::cout << "Target missing for " << MAX_MISSING_SECONDS
                      << " seconds; returning home." << std::endl;
            beginReturn(currentTime);
        }
    }

    centeredFrames = 0;
    centeredSince = -1;
    if (state == State::Hold) {
        state = State::Track;
    }
}

bool AimController::aimAtSelectedFlame(double currentTime)
{
    float horizontalError = selectedFlame.x - horizontalAimPosition;
    float verticalError = selectedFlame.y - verticalAimPosition;
    bool centeredHorizontally = std::abs(horizontalError) <= CENTER_DEADBAND;
    bool centeredVertically = std::abs(verticalError) <= CENTER_DEADBAND;

    if (centeredHorizontally && centeredVertically) {
        holdCenteredFlame(currentTime);
        return false;
    }

    state = State::Track;
    centeredFrames = 0;
    centeredSince = -1;
    int panChange = calculatePulseChange(horizontalError, panDirection);
    int tiltChange = calculatePulseChange(verticalError, tiltDirection);
    int nextPan = std::clamp(pan + panChange, MIN_PULSE_US, MAX_PULSE_US);
    int nextTilt = std::clamp(tilt + tiltChange, MIN_PULSE_US, MAX_PULSE_US);

    // Check each axis separately: the other servo might still be able to move.
    // If pan already reached its limit and needs to go farther, we cannot reach
    // the calibrated aim. Return without spraying instead of accepting a wrong aim.
    // These are commanded pulse limits, not feedback from the physical servo.
    if (!centeredHorizontally && nextPan == pan) {
        std::cout << "Aim unreachable: pan command limit reached; ignoring target and returning."
                  << std::endl;
        ignoreSelectedFlameAndReturn(currentTime);
        return false;
    }
    if (!centeredVertically && nextTilt == tilt) {
        std::cout << "Aim unreachable: tilt command limit reached; ignoring target and returning."
                  << std::endl;
        ignoreSelectedFlameAndReturn(currentTime);
        return false;
    }

    pan = nextPan;
    tilt = nextTilt;
    nextObservationTime = currentTime + response.moveSettleSeconds;
    return true;
}

int AimController::calculatePulseChange(float positionError, int direction) const
{
    if (std::abs(positionError) <= CENTER_DEADBAND) {
        return 0;
    }
    float gain = PULSE_GAIN;
    int maximumStep = MAX_PULSE_STEP_US;

    // Levels 3 and 4 also speed up the small corrections near the aim point.
    // Keep the same deadband; the selected preset controls the settling time.
    if (response.aimSpeedLevel == 3) {
        gain = 90.0f;
        maximumStep = 18;
    }
    if (response.aimSpeedLevel == 4) {
        gain = 120.0f;
        maximumStep = 24;
    }

    // Each axis uses larger steps only while far from its calibrated aim point.
    if (std::abs(positionError) > FAST_AIM_DISTANCE) {
        if (response.aimSpeedLevel >= 2) {
            gain = FAST_PULSE_GAIN;
            maximumStep = MAX_FAST_PULSE_STEP_US;
        }
        if (response.aimSpeedLevel == 4) {
            gain = 180.0f;
            maximumStep = 36;
        }
    }

    int pulseChange = static_cast<int>(std::round(positionError * gain));
    pulseChange = std::clamp(pulseChange, -maximumStep, maximumStep);
    return direction * pulseChange;
}

void AimController::holdCenteredFlame(double currentTime)
{
    // The first centered observation starts the waiting period selected by the preset.
    if (centeredSince < 0) {
        centeredSince = currentTime;
    }

    // Keep the existing frame confirmation before declaring the aim stable.
    if (state != State::Hold) {
        centeredFrames++;
        if (centeredFrames >= response.centeredFrames) {
            state = State::Hold;
        }
    }

    // Every attempt still requires a confirmed, continuously centered target.
    if (state != State::Hold) {
        return;
    }
    if (currentTime - centeredSince < response.centeredSeconds) {
        return;
    }
    if (pumpAttempts >= MAX_PUMP_ATTEMPTS) {
        return;
    }
    if (waitingForPumpToFinish) {
        return;
    }
    if (currentTime < nextPumpAttemptTime) {
        return;
    }

    // The deadband allows a range of final positions, not one exact point.
    // Log the detected center before each request so calibration can distinguish
    // a centering error from a water jet that misses despite correct centering.
    std::cout << "Aim before water request: flame X=" << selectedFlame.x
              << ", Y=" << selectedFlame.y
              << " | target X=" << horizontalAimPosition
              << ", Y=" << verticalAimPosition
              << " | pan=" << pan << ", tilt=" << tilt << std::endl;

    // Request one pulse now, then wait for main.cpp to report that it ended.
    pumpRequested = true;
    pumpAttempts++;
    waitingForPumpToFinish = true;
}

void AimController::ignoreSelectedFlameAndReturn(double currentTime)
{
    IgnoredFlame ignored;
    ignored.lastPosition = selectedFlameAtHome;
    ignoredFlames.push_back(ignored);
    beginReturn(currentTime);
}

void AimController::beginReturn(double currentTime)
{
    state = State::Return;
    confirmedFrames = 0;
    centeredFrames = 0;
    centeredSince = -1;
    pumpAttempts = 0;
    waitingForPumpToFinish = false;
    nextPumpAttemptTime = 0;
    pumpRequested = false;
    recentDetections.clear();
    nextObservationTime = currentTime;
}

bool AimController::moveHome(double currentTime)
{
    if (currentTime < nextObservationTime) {
        return false;
    }

    int oldPan = pan;
    int oldTilt = tilt;
    int panChange = std::clamp(homePanPulse - pan, -response.returnStepMicroseconds, response.returnStepMicroseconds);
    int tiltChange = std::clamp(homeTiltPulse - tilt, -response.returnStepMicroseconds, response.returnStepMicroseconds);
    pan += panChange;
    tilt += tiltChange;
    nextObservationTime = currentTime + response.returnStepSeconds;

    if (pan == homePanPulse && tilt == homeTiltPulse) {
        state = State::Cooldown;
        nextObservationTime = currentTime + response.homeSettleSeconds;
    }
    return pan != oldPan || tilt != oldTilt;
}
