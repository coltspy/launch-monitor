#pragma once

#include "detect.h"
#include <vector>

struct shot_data {
    float speed_mph;
    float launch_angle_deg;
    float distance_ft;
    float carry_ft;
    bool valid;
    
    shot_data() : speed_mph(0), launch_angle_deg(0), distance_ft(0), carry_ft(0), valid(false) {}
};
struct camera_calibration {
    float distance_between_inches;
    float pixels_per_inch;        
    float frame_rate;               
    camera_calibration() 
        : distance_between_inches(5.0f)
        , pixels_per_inch(720.0f / 10.0f)
        , frame_rate(120.0f)
    {}
};
class shot_calculator {
private:
    camera_calibration calibration;
    double time_between(const ball_detection& first, const ball_detection& last);
    float pixel_distance(const cv::Point2f& p1, const cv::Point2f& p2);
public:
    shot_calculator();
    shot_calculator(const camera_calibration& cal);
    shot_data calculate_shot(
        const std::vector<ball_detection>& top_camera,
        const std::vector<ball_detection>& bottom_camera
    );
    void set_camera_distance(float inches);
    void set_pixels_per_inch(float ppi);
    void set_frame_rate(float fps);
};