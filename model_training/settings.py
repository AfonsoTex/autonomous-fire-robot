# Adapter only: edit firmware/raspberry/settings.h to change settings.
# Python and C++ read the same values, without a generated configuration copy.

from pathlib import Path
import re

SETTINGS_PATH = Path(__file__).resolve().parents[1] / "firmware" / "raspberry" / "settings.h"


def read_settings(path):
    values = {}
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = line.split("//", 1)[0].strip()
        if not line.startswith("constexpr"):
            continue
        declaration = re.fullmatch(r"constexpr (bool|int|float|double) ([A-Z_]+) = (.+);", line)
        if declaration is None:
            raise ValueError(f"Invalid setting at {path}:{line_number}")
        kind, name, literal = declaration.groups()
        if name in values:
            raise ValueError(f"Duplicate setting: {name}")
        if kind == "bool":
            if literal not in ("true", "false"):
                raise ValueError(f"{name} must be true or false")
            value = literal == "true"
        elif kind == "int":
            value = int(literal)
        else:
            value = float(literal.removesuffix("f"))
        values[name] = value
    return values


# Expose the header values so other files can use ordinary 'from settings import ...'.
globals().update(read_settings(SETTINGS_PATH))

if EPOCHS < 1 or BATCH_SIZE < 1:
    raise ValueError("EPOCHS and BATCH_SIZE must be positive")
if LEARNING_RATE <= 0 or LEARNING_RATE_PATIENCE < 1 or MIN_LEARNING_RATE_DIVISOR < 1:
    raise ValueError("Invalid learning rate settings")
if not 0 < CONFIDENCE_THRESHOLD < 1:
    raise ValueError("CONFIDENCE_THRESHOLD must be between 0 and 1 (exclusive)")
if min(POSITION_WEIGHT, COUNT_WEIGHT, FIRE_WEIGHT, NO_FIRE_WEIGHT) < 0:
    raise ValueError("Loss weights cannot be negative")
if not 0 <= NMS_IOU_THRESHOLD <= 1 or MAX_DISTANCE <= 0:
    raise ValueError("Invalid detection evaluation settings")
if not all(0 <= value <= 1 for value in (FLIP_PROBABILITY, ROTATION_PROBABILITY, CROP_PROBABILITY)):
    raise ValueError("Augmentation probabilities must be between 0 and 1")
if not 0 < MIN_CROP_SCALE <= 1 or MAX_ROTATION_DEGREES < 0:
    raise ValueError("Invalid crop or rotation settings")
if not 0 < MIN_BRIGHTNESS <= MAX_BRIGHTNESS or not 0 < MIN_CONTRAST <= MAX_CONTRAST:
    raise ValueError("Lighting ranges must be positive and ordered")

MIN_LEARNING_RATE = LEARNING_RATE / MIN_LEARNING_RATE_DIVISOR
