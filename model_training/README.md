# FireNet — training tools

This folder contains the tools to record, annotate, train, evaluate and export FireNet,
the small neural network that detects flames on the Raspberry Pi.

The current model was trained on 4,922 images recorded with the robot's own camera
(3,933 for training and 989 for validation), with 1,508 flames annotated by hand.

The dataset in `Datasets/` (about 250 MB) and generated files in `model_training/results/`
stay local and are excluded from Git. On a new checkout, create a dataset with the
annotator before training; the cache is generated automatically.

## Settings

Training and detection settings are **not** in `train.py`. Edit
[`firmware/raspberry/settings.h`](../firmware/raspberry/settings.h): epochs, learning
rate, loss weights, batch size, confidence threshold, NMS and augmentation. Each value
has a comment explaining its effect. `settings.py` reads that header so Python and the
Raspberry program always use the same values. Restart Python after editing it.

## Tools

| File | Purpose |
| --- | --- |
| `annotate_images.py` | Opens a video, extracts images, lets you draw flame boxes, and sends images and labels to the dataset when you finalize the session. Can resume a session. |
| `train.py` | Updates the cache, trains for `EPOCHS` epochs and saves the best model to `firenet_grid.pt`. Resumes saved weights unless `--fresh` is given. |
| `view_results.py` | Reports validation performance and saves images with predictions (green) and annotations (red) to `results/validation_predictions/`. |
| `model_converter.py` | Converts `firenet_grid.pt` to `firenet.onnx` for the Raspberry Pi. |
| `extract_frames.py` | Extracts images from a video. Optional: the annotator already calls it. |
| `build_cache.py` | Prepares 180×180 images and labels to speed up training. Optional: training does it automatically. |
| `model.py` | The FireNet architecture. |
| `data.py` | Reads the dataset and prepares training batches. |
| `augmentation.py` | Training-only image variations (lighting, flips, small rotations, crops). |
| `detection_filter.py` | NMS: keeps the most confident box and removes boxes overlapping it by more than `NMS_IOU_THRESHOLD`. |
| `settings.py` | Reads `settings.h`; do not put values here. |
| `requirements-converter.txt` | Pinned Python dependencies. |

## The model

FireNet is a small convolutional network (three convolution layers and two
max-pooling layers). The 180×180 input image becomes a 45×45 grid. For each cell it
predicts five values: is there a flame here, the x/y offset of its centre inside the
cell, and the box width and height. This is the same idea as YOLO-style detectors, but
much smaller so it runs in real time on a Raspberry Pi CPU.

## Setup on Windows

Use Python 3.12. Run these commands **from the repository root**:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r model_training/requirements-converter.txt
.\.venv\Scripts\python.exe -m pip install opencv-python==4.10.0.84 pillow
```

The commands below use the environment's Python directly, so activation is not needed.

## Workflow: record → annotate → train → export

### 1. Record video on the Raspberry Pi

Record real situations with and without flames. Stop FireNet first (Ctrl+C) to free
the camera, then:

```bash
rpicam-vid -n -t 60000 --width 640 --height 480 \
  --framerate 30 --rotation 180 --codec libav -o ~/session01.mp4
```

This records 60 seconds. The 180° rotation matches the current camera mounting. Use a
new filename for each recording.

### 2. Copy the video to the PC

```powershell
scp afonso@BaraoForrester.local:~/session01.mp4 "C:\Users\afons\Desktop\"
```

The video can be in any folder; it does not need to be inside the project.

### 3. Annotate

```powershell
.\.venv\Scripts\python.exe model_training/annotate_images.py
```

- Choose **Open new video** and select the video.
- Choose the interval: 60 saves one image every 60 frames (about two seconds at 30 fps).
- Draw a box around each flame and press **Enter**. If there is no flame, choose **No flame**.
- To continue later, choose **Resume extracted images** and open the session folder in `model_training/results/frames/`.

Buttons:

- **Skip** moves on without saving; the image still needs a label before finalizing.
- **Delete image** removes the image from the session (kept in `.deleted/` for recovery, never exported).
- **No flame** saves an empty label. Use it only when there is no flame, not when you are unsure.

Keep blurry images if every flame can still be boxed reliably; delete images where
flame positions cannot be determined.

### 4. Finalize

Click **Finalize** and confirm the split (default 80% training, 20% validation; earlier
frames go to training, later frames to validation). The tool copies the files to:

```text
Datasets/fire_detection/
  train/images/   train/labels/
  valid/images/   valid/labels/
  sessions/       imported session records
```

**After confirming and verifying the copies, the tool deletes the session files and
the selected video on the PC.** The Raspberry Pi copy remains. Annotate a copy if you
want to keep the PC video.

### 5. Train

```powershell
.\.venv\Scripts\python.exe model_training/train.py
```

- The cache in `model_training/results/cache/` is updated automatically.
- Without `--fresh`, training continues from `firenet_grid.pt`. Only a better validation score replaces it.
- With `--fresh`, training starts from new weights; the old checkpoint is first backed up to `model_training/results/model_backups/`.
- Each run starts at `LEARNING_RATE`. After `LEARNING_RATE_PATIENCE` epochs without a new best score the rate is halved, down to `LEARNING_RATE / MIN_LEARNING_RATE_DIVISOR`.

Tip: when training from scratch, a large `COUNT_WEIGHT` can dominate at the start,
because it sums probabilities over all 2025 grid cells. If a fresh run gets stuck at a
score of zero, try `COUNT_WEIGHT = 0` first.

### 6. Check the results

```powershell
.\.venv\Scripts\python.exe model_training/view_results.py
```

### 7. Export and deploy

```powershell
.\.venv\Scripts\python.exe model_training/model_converter.py
```

This writes `model_training/firenet.onnx`. Copy it to `firmware/raspberry/firenet.onnx`
and then to the Raspberry Pi; the copy is not automatic. If you changed `settings.h`,
copy it too and rebuild the Raspberry program. The ONNX file contains only the network;
the confidence threshold and NMS come from `settings.h`.

## Evaluation and detection settings

- `CONFIDENCE_THRESHOLD` decides which predictions count as detections, in evaluation and on the Raspberry. It does not change training.
- NMS removes overlapping duplicate boxes. It does not remove isolated false positives such as reflections in another place, and it can merge two flames whose boxes overlap strongly.
- The score matches predicted and annotated flame centres (within `MAX_DISTANCE` pixels), not simple image accuracy.
- `MEASURE_TRAIN = true` also scores the training split, useful to compare with validation.
- Keep the same threshold and NMS settings when comparing scores between runs.

Images from the same video can be very similar. Use separate recordings for a reliable
final evaluation.

### Data augmentation

With `ENABLE_AUGMENTATION = true`, each training image is randomly changed as it is
loaded: brightness and contrast, horizontal flip, small rotation and crop. Boxes are
moved with the image. If a rotation or crop would cut an annotated flame, that change is
skipped. Original files, the cache and validation images are never changed. The ranges
and probabilities are in `settings.h`.

## Common model problems

**The model misses some flames.** Record more examples of those situations (distance,
lighting, position), annotate them and retrain. Running a video through the model does
not teach it anything.

**The model detects flames where there are none.** Record those places and objects
without flames, mark them **No flame**, and retrain.

**Several boxes on one flame.** NMS should remove them; check `NMS_IOU_THRESHOLD`.

**High training score, low validation score.** The model has learned the training
images too specifically. Collect more varied data, use augmentation, and check that
training and validation cover similar situations.

**Similar, mediocre scores on both.** The model is missing something everywhere. Check
the annotations for mistakes, try more training, and only then consider settings or a
larger model.

## Optional tools

```powershell
# Extract images without the annotator
.\.venv\Scripts\python.exe model_training/extract_frames.py session.mp4 --every 60

# Force a cache rebuild
.\.venv\Scripts\python.exe model_training/build_cache.py
```

## Input size

`IMAGE_SIZE = 180` in `data.py` gives a 45×45 grid. The Raspberry uses the matching
`MODEL_INPUT_SIZE` in `inference.h`. Changing the size requires retraining, exporting
and updating both values; the cache rebuilds automatically.

Keep videos, datasets, caches and the `.venv` folder out of Git commits.
