#include "calculate.h"
#include <cmath>
#include <iostream>

shot_calculator::shot_calculator() {
    calibration = camera_calibration();
}

shot_calculator::shot_calculator(const camera_calibration& cal) 
    : calibration(cal) 
{
}

double shot_calculator::time_between(const ball_detection& first, const ball_detection& last) {
    auto duration = last.timestamp - first.timestamp;
    return std::chrono::duration<double>(duration).count();
}

float shot_calculator::pixel_distance(const cv::Point2f& p1, const cv::Point2f& p2) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    return sqrt(dx * dx + dy * dy);
}

shot_data shot_calculator::calculate_shot(
    const std::vector<ball_detection>& top_camera,
    const std::vector<ball_detection>& bottom_camera
) {
    shot_data result;

    if (top_camera.size() < 2 || bottom_camera.size() < 2) {
        std::cout << "not enough detections (need 2+ per camera)" << std::endl;
        return result;
    }

    const ball_detection& first_det = bottom_camera.front();
    const ball_detection& last_det = top_camera.back();

    if (!first_det.found || !last_det.found) {
        std::cout << "invalid detections" << std::endl;
        return result;
    }

    double time_seconds = time_between(first_det, last_det);

    if (time_seconds <= 0 || time_seconds > 1.0) {
        std::cout << "invalid time: " << time_seconds << "s" << std::endl;
        return result;
    }

    float horizontal_distance_inches = calibration.distance_between_inches;
    float horizontal_speed_ips = horizontal_distance_inches / time_seconds;
    result.speed_mph = horizontal_speed_ips * 0.0568182;

    float vertical_pixels = first_det.position.y - last_det.position.y;
    float vertical_inches = vertical_pixels / calibration.pixels_per_inch;

    result.launch_angle_deg = atan2(vertical_inches, horizontal_distance_inches) * 180.0 / CV_PI;

    float speed_fps = result.speed_mph * 1.46667;
    float angle_rad = result.launch_angle_deg * CV_PI / 180.0;
    float gravity = 32.174;

    result.carry_ft = (speed_fps * speed_fps * sin(2.0 * angle_rad)) / gravity;
    result.distance_ft = result.carry_ft;
    result.valid = true;
    std::cout << "shot calculated:" << std::endl;
    std::cout << "  time: " << time_seconds << "s" << std::endl;
    std::cout << "  speed: " << result.speed_mph << " mph" << std::endl;
    std::cout << "  launch angle: " << result.launch_angle_deg << " deg" << std::endl;
    std::cout << "  distance: " << result.distance_ft << " ft" << std::endl;
    
    return result;
}

void shot_calculator::set_camera_distance(float inches) {
    calibration.distance_between_inches = inches;
}

void shot_calculator::set_pixels_per_inch(float ppi) {
    calibration.pixels_per_inch = ppi;
}

void shot_calculator::set_frame_rate(float fps) {
    calibration.frame_rate = fps;
}