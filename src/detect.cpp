#include  "detect.h"
#include <iostream>

ball_detector::ball_detector(int threshold, float min_circ) 
    : brightness_threshold(threshold)
    , min_circularity(min_circ)
    , min_area(50.0f)
    , max_area(5000.0f)
{
}

ball_detection ball_detector::find_ball(const cv::Mat& frame) {
    ball_detection result;
    
    if (frame.empty()) {
        return result;
    }
    
    cv::Mat gray;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame.clone();
    }

    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(9, 9), 2);

    cv::Mat thresh;
    cv::threshold(blurred, thresh, brightness_threshold, 255, cv::THRESH_BINARY);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(thresh, thresh, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    float best_score = 0;
    cv::Point2f best_center;
    float best_radius = 0;
    
    for (const auto& contour : contours) {
        float area = cv::contourArea(contour);
        if (area < min_area || area > max_area) {
            continue;
        }
        cv::Point2f center;
        float radius;
        cv::minEnclosingCircle(contour, center, radius);

        float perimeter = cv::arcLength(contour, true);
        float circularity = (4.0 * CV_PI * area) / (perimeter * perimeter);

        if (circularity > min_circularity) {
            float score = circularity * area;
            if (score > best_score) {
                best_score = score;
                best_center = center;
                best_radius = radius;
            }
        }
    }
    if (best_score > 0) {
        result.position = best_center;
        result.radius = best_radius;
        result.timestamp = std::chrono::high_resolution_clock::now();
        result.found = true;
    }
    return result;
}

ball_detection ball_detector::find_ball_visual(cv::Mat& frame) {
    ball_detection detection = find_ball(frame);

    if (detection.found) {
        cv::circle(frame, detection.position, (int)detection.radius,
                  cv::Scalar(255), 2);

        cv::circle(frame, detection.position, 3,
                  cv::Scalar(128), -1);

        int cross_size = 10;
        cv::line(frame,
                cv::Point(detection.position.x - cross_size, detection.position.y),
                cv::Point(detection.position.x + cross_size, detection.position.y),
                cv::Scalar(255), 1);
        cv::line(frame,
                cv::Point(detection.position.x, detection.position.y - cross_size),
                cv::Point(detection.position.x, detection.position.y + cross_size),
                cv::Scalar(255), 1);
    }
    return detection;
}
void ball_detector::set_threshold(int threshold) {
    brightness_threshold = threshold;
}
void ball_detector::set_circularity(float min_circ) {
    min_circularity = min_circ;
}