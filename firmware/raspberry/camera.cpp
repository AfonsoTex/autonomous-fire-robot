#include "camera.h"
#include <iostream>
#include <string>

Camera::Camera() {}

Camera::~Camera() {
    release();
}

bool Camera::open(int index) {
    // Capture through libcamera and deliver BGR frames to OpenCV.
    const std::string pipeline =
    "libcamerasrc ! "
    "video/x-raw,format=NV12,width=640,height=480,"
    "framerate=30/1,colorimetry=bt709 ! "
    "videoconvert ! "
    "video/x-raw,format=BGR ! "
    "appsink max-buffers=1 drop=true sync=false";

    cap.open(pipeline, cv::CAP_GSTREAMER);

    if (!cap.isOpened()) {
        std::cerr << "Could not open the camera." << std::endl;
        return false;
    }
    return true;
}

bool Camera::readFrame(cv::Mat& frame) {
    if (!cap.isOpened()) return false;

    cv::Mat capturedFrame;
    cap >> capturedFrame; // Capture a BGR frame from GStreamer.

    if (capturedFrame.empty()) return false;

    // Keep the captured resolution for display and correct the camera orientation.
    cv::rotate(capturedFrame, frame, cv::ROTATE_180);

    // Convert BGR to RGB for the model.
    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

    return true;
}

void Camera::release() {
    if (cap.isOpened()) {
        cap.release();
    }
}
