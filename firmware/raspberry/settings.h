#pragma once

// EDIT SETTINGS HERE: Python reads this file; Raspberry C++ includes it.
// Keep each value as a number or true/false on one line (no calculations).
// Restart Python after editing. For Raspberry changes, copy this header and recompile.
// Training weights affect future training, not the saved model immediately.
// Export and copy the ONNX model after training to use its new weights on Raspberry.

// false = use the normal settings below. true = use the fast test preset at the end.
// This shortens confirmation/waiting and uses servo speed level 4; inference itself
// runs at the same speed. Less settling can cause blur, overshoot and false stops.
// Spray duration, attempt limit, calibration, confidence and servo limits stay unchanged.
constexpr bool FAST_TEST_MODE = true;

// TRAINING
// More epochs = more passes through the data, more time and possible overfitting.
// Fewer epochs = a shorter run. Only a better validation score replaces the checkpoint.
constexpr int EPOCHS = 50;

// Higher = larger weight updates, potentially faster learning but more overshooting.
// Lower = smaller updates, useful for fine-tuning but potentially slower learning.
// Each run starts at this rate, even when resuming saved model weights.
constexpr double LEARNING_RATE = 0.001;
// More patience = wait longer without a new best score before halving the rate.
// Less patience = halve sooner. A later improvement keeps the reduced rate.
constexpr int LEARNING_RATE_PATIENCE = 5;
// Minimum rate = LEARNING_RATE / this divisor. Larger allows a lower minimum.
constexpr int MIN_LEARNING_RATE_DIVISOR = 32;

// Higher = prioritize accurate box positions and sizes; lower = less emphasis.
constexpr float POSITION_WEIGHT = 1.0f;
// Higher = penalize incorrect total flame counts more, both excess and missing flames.
// Lower = less count pressure; zero disables this loss. This does not replace NMS.
constexpr float COUNT_WEIGHT = 3.0f;
// Higher = penalize missing real flames more, possibly increasing false detections.
// Lower = less pressure to detect real flames, possibly missing more small flames.
constexpr float FIRE_WEIGHT = 1.0f;
// Higher = penalize predicted flames in empty cells more, possibly missing real flames.
// Lower = less background penalty, possibly detecting more reflections as flames.
// Reflections must be correctly left unlabelled; real flames still need labels.
constexpr float NO_FIRE_WEIGHT = 2.0f;

// Larger batches use more memory and average more images per update.
// Smaller batches use less memory but make noisier updates.
constexpr int BATCH_SIZE = 32;
// true also evaluates the training split, helping compare train and validation scores.
// false saves evaluation time. It does not disable training.
constexpr bool MEASURE_TRAIN = false;
// Maximum centre error in resized-image pixels when scoring a matched flame.
// Higher makes scoring more forgiving; lower makes it stricter, not better trained.
constexpr float MAX_DISTANCE = 10.0f;

// DETECTION: shared by Python evaluation and Raspberry display/target selection.
// Higher accepts fewer detections: usually fewer false positives and more missed flames.
// Lower accepts more detections. This filters output; it is NOT a training loss weight.
constexpr float CONFIDENCE_THRESHOLD = 0.65f;
// Lower removes more overlapping boxes; too low can merge distinct nearby flames.
// Higher keeps more boxes, including possible duplicates.
constexpr float NMS_IOU_THRESHOLD = 0.3f;

// AUGMENTATION: training only; original files and validation images stay unchanged.
// true creates random variations when loading images; false uses originals.
// It does not add files. Spatial changes that cut a flame box are rejected.
constexpr bool ENABLE_AUGMENTATION = true;
// Probabilities: 0 = never, 1 = every image. Higher applies the change more often.
constexpr float FLIP_PROBABILITY = 0.5f;
constexpr float ROTATION_PROBABILITY = 0.3f;
// Higher allows stronger rotations in either direction; lower keeps images more level.
constexpr float MAX_ROTATION_DEGREES = 5.0f;
constexpr float CROP_PROBABILITY = 0.5f;
// Lower allows stronger crops/zoom; closer to 1 keeps more of the original view.
constexpr float MIN_CROP_SCALE = 0.90f;
// Factors are sampled between each minimum and maximum; 1 leaves that property unchanged.
// Lower brightness darkens; higher brightens. Lower contrast flattens; higher strengthens it.
// Widening these ranges creates stronger variations, which can become unrealistic.
constexpr float MIN_BRIGHTNESS = 0.90f;
constexpr float MAX_BRIGHTNESS = 1.10f;
constexpr float MIN_CONTRAST = 0.90f;
constexpr float MAX_CONTRAST = 1.10f;

// AIMING
// true uses the calibration table in aim_controller.cpp; false uses fixed positions below.
constexpr bool USE_VERTICAL_AIM_CALIBRATION = true;
// Normalized image coordinates: X grows rightwards; Y grows downwards; 0.5 is the middle.
constexpr float AIM_HORIZONTAL_POSITION = 0.50f;
constexpr float AIM_VERTICAL_POSITION = 0.50f;
// Added to every table/fixed target. For the CURRENT mounting and direction arguments:
// increasing X moves the jet left; decreasing X moves it right.
// increasing Y moves the jet up; decreasing Y moves it down. Verify after remounting.
constexpr float HORIZONTAL_AIM_ADJUSTMENT = 0.02f;
constexpr float VERTICAL_AIM_ADJUSTMENT = 0.01f;
// Higher tolerates a larger position error and makes fewer corrections; lower aims tighter.
constexpr float CENTER_DEADBAND = 0.01f;
// 1 = original slow. 2 = faster far away. 3 = also faster small corrections.
// 4 = fastest near and far. Higher can increase overshoot and vibration.
constexpr int AIM_SPEED_LEVEL = 3;
// Confirm the selected target this many times within the recent observation window.
// More required detections rejects brief mistakes but takes longer to start aiming.
constexpr int CONFIRMATION_WINDOW = 6;
constexpr int REQUIRED_DETECTIONS = 4;
// More centred observations or a longer centred time delays spraying for more stability.
// Both conditions must be met before requesting water.
constexpr int REQUIRED_CENTERED_FRAMES = 3;
constexpr double CENTERED_BEFORE_PUMP_SECONDS = 0.5;
// Higher waits through longer detection losses before returning; lower returns sooner.
constexpr double MAX_MISSING_SECONDS = 3.0;
// Longer waits reduce blur while assessing frames, but slow aiming/returning.
constexpr double MOVE_SETTLE_SECONDS = 0.25;
constexpr double RETURN_STEP_SECONDS = 0.15;
constexpr double HOME_SETTLE_SECONDS = 2.0;

// WATER
// Higher sprays longer and uses more water; lower gives a shorter pulse.
constexpr double PUMP_SPRAY_SECONDS = 0.3;
// Higher allows more attempts on the same selected flame; lower limits water use.
constexpr int MAX_PUMP_ATTEMPTS = 3;
// Higher waits longer between attempts; lower retries sooner after the pulse ends.
constexpr double PUMP_RETRY_COOLDOWN_SECONDS = 10.0;

// CAR STOP / RESUME
// More consecutive flame frames reduces isolated false stops but delays stopping.
constexpr int CAR_FLAME_CONFIRM_FRAMES = 2;
// Both clear-frame count and time must pass before moving again when aiming is ready.
// Higher tolerates longer detection gaps; lower resumes sooner.
constexpr int CAR_NO_FLAME_FRAMES = 5;
constexpr double CAR_NO_FLAME_SECONDS = 1.0;
// Higher gives the stopped car longer to settle before measuring the target.
constexpr double CAR_SETTLE_SECONDS = 0.5;

// REFLECTION FILTER: positions alone cannot reliably identify reflections.
// Higher follows detections farther from the previous target; lower rejects larger jumps.
constexpr float MAX_TARGET_DISTANCE = 0.15f;
// Tighter matching after water: lower rejects more jumps but may lose the real flame.
constexpr float POST_SPRAY_MATCH_DISTANCE = 0.04f;
// Larger ignores NEW detections in a wider area around the sprayed target at home.
// Smaller protects more nearby real flames. Coordinates are image fractions, not metres.
constexpr float REFLECTION_IGNORE_RADIUS = 0.15f;
// Higher ignores new nearby detections longer AFTER permission to drive resumes.
// Lower accepts new nearby targets sooner. Existing targets are not blanket-ignored.
constexpr double REFLECTION_IGNORE_SECONDS = 2.5;
// Higher remembers an unreachable flame through longer losses; lower forgets it sooner.
constexpr double IGNORED_FLAME_FORGET_SECONDS = 2.0;

// Normal response settings, copied without changing the values above.
// The C++ controllers use this preset. Python training is unaffected by fast mode.
struct ResponseSettings {
    int carFlameFrames = CAR_FLAME_CONFIRM_FRAMES;
    double carSettleSeconds = CAR_SETTLE_SECONDS;
    int requiredDetections = REQUIRED_DETECTIONS;
    int aimSpeedLevel = AIM_SPEED_LEVEL;
    double moveSettleSeconds = MOVE_SETTLE_SECONDS;
    int centeredFrames = REQUIRED_CENTERED_FRAMES;
    double centeredSeconds = CENTERED_BEFORE_PUMP_SECONDS;
    double returnStepSeconds = RETURN_STEP_SECONDS;
    int returnStepMicroseconds = 12;
    double homeSettleSeconds = HOME_SETTLE_SECONDS;
    double pumpRetrySeconds = PUMP_RETRY_COOLDOWN_SECONDS;
    int clearFrames = CAR_NO_FLAME_FRAMES;
    double clearSeconds = CAR_NO_FLAME_SECONDS;
};

inline constexpr ResponseSettings getResponseSettings(bool fastMode)
{
    ResponseSettings response;
    if (fastMode) {
        // Stop on the first accepted flame, then use a fresh frame after a short settling wait.
        response.carFlameFrames = 1;
        response.carSettleSeconds = 0.15;
        response.requiredDetections = 1;
        // Larger corrections with shorter waits; preserve the normal aiming deadband.
        response.aimSpeedLevel = 4;
        response.moveSettleSeconds = 0.08;
        // Still require the target to stay centred before requesting water.
        response.centeredFrames = 1;
        response.centeredSeconds = 0.10;
        // Return faster and allow the camera a shorter settling period at home.
        response.returnStepSeconds = 0.05;
        response.returnStepMicroseconds = 24;
        response.homeSettleSeconds = 0.30;
        // Retry sooner only AFTER the full spray finishes; the attempt limit still applies.
        response.pumpRetrySeconds = 2.0;
        response.clearFrames = 2;
        response.clearSeconds = 0.30;
    }
    return response;
}

static_assert(CONFIDENCE_THRESHOLD > 0 && CONFIDENCE_THRESHOLD < 1,
              "Confidence must be between 0 and 1 (exclusive)");
static_assert(AIM_SPEED_LEVEL >= 1 && AIM_SPEED_LEVEL <= 4, "Aim speed must be 1..4");
static_assert(REQUIRED_DETECTIONS > 0 && REQUIRED_DETECTIONS <= CONFIRMATION_WINDOW,
              "Required detections must fit inside the confirmation window");
static_assert(PUMP_SPRAY_SECONDS > 0 && MAX_PUMP_ATTEMPTS > 0, "Water settings must be positive");
