from pathlib import Path

import torch
from model import FireNet
from data import IMAGE_SIZE
from settings import CONFIDENCE_THRESHOLD, SETTINGS_PATH

# Resolve model files relative to this script, regardless of the working directory.
model_directory = Path(__file__).resolve().parent
output_path = model_directory / "firenet.onnx"

if not 0 < CONFIDENCE_THRESHOLD < 1:
    raise ValueError("CONFIDENCE_THRESHOLD must be between 0 and 1 (exclusive).")

# Create the network using the architecture defined in model.py.
model = FireNet()

# Load the saved checkpoint onto the CPU.
checkpoint = torch.load(
    model_directory / "firenet_grid.pt",
    map_location="cpu",
    weights_only=True,
)

# Replace the initial parameters with the trained weights and biases.
model.load_state_dict(checkpoint["weights"])

# Set the network to evaluation mode.
model.eval()

# Create an example input: 1 RGB image, IMAGE_SIZE pixels high and wide.
# This defines the input shape; it does not train the network.
example = torch.zeros(1, 3, IMAGE_SIZE, IMAGE_SIZE)

# Export the network operations and trained parameters into one ONNX file.
torch.onnx.export(
    model,
    example,
    output_path,
    input_names=["image"],           # Name of the model input.
    output_names=["predictions"],    # Name of the model output.
    opset_version=17,                # ONNX operator set version.
    dynamo=False,                    # Use the classic PyTorch exporter.
)

print(f"ONNX model saved to: {output_path}")

# Settings are shared directly; exporting must never overwrite the user's settings.
print(f"Shared confidence threshold: {CONFIDENCE_THRESHOLD:g}")
print(f"Copy {SETTINGS_PATH.name} with the model and rebuild the Raspberry program.")
