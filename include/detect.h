#pragma once

#include <opencv2/opencv.hpp>
#include <chrono>
#include <vector>

struct ball_detection {
    cv::Point2f position;
    float radius;
    std::chrono::high_resolution_clock::time_point timestamp;
    bool found;
    ball_detection() : position(0, 0), radius(0), found(false) {}
};

class ball_detector {
private:
    int brightness_threshold;
    float min_circularity;
    float min_area;
    float max_area;
public:
    ball_detector(int threshold = 200, float min_circ = 0.7);
    ball_detection find_ball(const cv::Mat& frame);
    ball_detection find_ball_visual(cv::Mat& frame);
    void set_threshold(int threshold);
    void set_circularity(float min_circ);
};