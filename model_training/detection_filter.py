# Removes overlapping predictions that probably describe the same flame.
# Each prediction contains (x, y, width, height, confidence).

from settings import NMS_IOU_THRESHOLD


def box_overlap(first, second):
    left = max(first[0] - first[2] / 2, second[0] - second[2] / 2)
    right = min(first[0] + first[2] / 2, second[0] + second[2] / 2)
    top = max(first[1] - first[3] / 2, second[1] - second[3] / 2)
    bottom = min(first[1] + first[3] / 2, second[1] + second[3] / 2)

    intersection = max(0, right - left) * max(0, bottom - top)
    union = first[2] * first[3] + second[2] * second[3] - intersection
    if union <= 0:
        return 0.0
    return intersection / union


def remove_duplicate_boxes(boxes):
    # Keep the most confident box first. Equal confidences keep their original order.
    ordered_boxes = sorted(boxes, key=lambda box: box[4], reverse=True)
    kept_boxes = []

    for box in ordered_boxes:
        duplicate = False
        for kept_box in kept_boxes:
            if box_overlap(box, kept_box) > NMS_IOU_THRESHOLD:
                duplicate = True
                break

        if not duplicate:
            kept_boxes.append(box)

    return kept_boxes
