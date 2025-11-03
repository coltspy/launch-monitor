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

struct contour_info {
    float area;
    float circularity;
    cv::Point2f center;
    float radius;
    bool passed_area;
    bool passed_circularity;
};

struct detection_debug {
    cv::Mat threshold_img;
    cv::Mat morphed_img;
    int contours_found;
    int contours_passed_area;
    int contours_passed_circularity;
    float max_brightness;
    std::vector<contour_info> all_contours;
    detection_debug() : contours_found(0), contours_passed_area(0), contours_passed_circularity(0), max_brightness(0) {}
};

class ball_detector {
private:
    int brightness_threshold;
    float min_circularity;
    float min_area;
    float max_area;
    cv::Rect roi;
    bool use_roi;
public:
    ball_detector(int threshold = 200, float min_circ = 0.7);
    ball_detection find_ball(const cv::Mat& frame);
    ball_detection find_ball_visual(cv::Mat& frame);
    ball_detection find_ball_debug(const cv::Mat& frame, detection_debug& debug);
    void set_threshold(int threshold);
    void set_circularity(float min_circ);
    void set_min_area(float area);
    void set_max_area(float area);
    void set_roi(cv::Rect rect);
    void disable_roi();
    int get_threshold() const { return brightness_threshold; }
    float get_circularity() const { return min_circularity; }
    float get_min_area() const { return min_area; }
    float get_max_area() const { return max_area; }
    cv::Rect get_roi() const { return roi; }
    bool is_using_roi() const { return use_roi; }
};