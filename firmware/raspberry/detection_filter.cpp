#include "detection_filter.h"

#include <algorithm>

namespace {
float boxOverlap(const Detection& first, const Detection& second)
{
    float left = std::max(first.x - first.width / 2, second.x - second.width / 2);
    float right = std::min(first.x + first.width / 2, second.x + second.width / 2);
    float top = std::max(first.y - first.height / 2, second.y - second.height / 2);
    float bottom = std::min(first.y + first.height / 2, second.y + second.height / 2);

    float intersection = std::max(0.0f, right - left) * std::max(0.0f, bottom - top);
    float unionArea = first.width * first.height + second.width * second.height - intersection;
    if (unionArea <= 0) {
        return 0;
    }
    return intersection / unionArea;
}

bool higherConfidence(const Detection& first, const Detection& second)
{
    return first.confidence > second.confidence;
}
}

std::vector<Detection> removeDuplicateDetections(std::vector<Detection> detections)
{
    // Keep the strongest prediction first; preserve the original order when scores tie.
    std::stable_sort(detections.begin(), detections.end(), higherConfidence);
    std::vector<Detection> kept;

    for (std::size_t i = 0; i < detections.size(); i++) {
        bool duplicate = false;
        for (std::size_t j = 0; j < kept.size(); j++) {
            if (boxOverlap(detections[i], kept[j]) > NMS_IOU_THRESHOLD) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            kept.push_back(detections[i]);
        }
    }
    return kept;
}
