#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <deque>
#include <chrono>

int main() {
    std::cout << "Launch Monitor Camera Test\n";
    std::cout << "OpenCV Version: " << CV_VERSION << std::endl;
    
    // Try to find your InnoMaker cameras
    std::vector<cv::VideoCapture> cameras;
    
    for (int i = 0; i < 4; i++) {
        cv::VideoCapture cap(i, cv::CAP_DSHOW); // Force DirectShow backend
        if (cap.isOpened()) {
            std::cout << "Found camera at index " << i << std::endl;
            
            // Try to set to your camera specs
            cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
            cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
            cap.set(cv::CAP_PROP_FPS, 120);
            
            // Check what we actually got
            int width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
            int height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
            double fps = cap.get(cv::CAP_PROP_FPS);
            
            std::cout << "  Resolution: " << width << "x" << height << std::endl;
            std::cout << "  FPS: " << fps << std::endl;
            
            cameras.push_back(std::move(cap));
        }
    }
    
    if (cameras.empty()) {
        std::cout << "No cameras found! Check USB connections." << std::endl;
        return -1;
    }
    
    std::cout << "\nFound " << cameras.size() << " cameras. Press ESC to exit.\n";
    
    // Ball tracking data structures
    std::deque<cv::Point2f> ball_trail; // Store ball positions for trail
    std::deque<std::chrono::high_resolution_clock::time_point> timestamps;
    const int max_trail_length = 20; // Number of positions to keep in trail
    
    // Ball detection and tracking
    while (true) {
        for (size_t i = 1; i < cameras.size(); i++) { // Start from index 1 to skip webcam
            cv::Mat frame;
            if (cameras[i].read(frame)) {
                // Convert to grayscale for better circle detection
                cv::Mat gray;
                cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
                
                // Apply Gaussian blur to reduce noise
                cv::Mat blurred;
                cv::GaussianBlur(gray, blurred, cv::Size(9, 9), 2, 2);
                
                // Detect circles (golf balls) using HoughCircles
                std::vector<cv::Vec3f> circles;
                cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT, 1, 
                    gray.rows/8,   // min distance between circles
                    50, 20,        // lower thresholds for better detection
                    20, 200        // much larger radius range for close ball
                );
                
                // Process detected circles
                auto current_time = std::chrono::high_resolution_clock::now();
                
                for (size_t j = 0; j < circles.size(); j++) {
                    cv::Vec3i c = circles[j];
                    cv::Point2f center = cv::Point2f(c[0], c[1]);
                    int radius = c[2];
                    
                    // Add to trail
                    ball_trail.push_back(center);
                    timestamps.push_back(current_time);
                    
                    // Keep trail length manageable
                    if (ball_trail.size() > max_trail_length) {
                        ball_trail.pop_front();
                        timestamps.pop_front();
                    }
                    
                    // Calculate velocity if we have previous positions
                    if (ball_trail.size() >= 2) {
                        cv::Point2f prev_pos = ball_trail[ball_trail.size()-2];
                        auto prev_time = timestamps[timestamps.size()-2];
                        
                        // Calculate distance and time difference
                        float distance = cv::norm(center - prev_pos); // pixels
                        auto time_diff = std::chrono::duration_cast<std::chrono::microseconds>(current_time - prev_time);
                        float dt = time_diff.count() / 1000000.0f; // convert to seconds
                        
                        // Velocity in pixels per second
                        float velocity_pps = distance / dt;
                        
                        // Convert to real units (you'll need to calibrate this)
                        // For now, assuming ~1 pixel = 1mm at your distance
                        float velocity_mps = velocity_pps * 0.001f; // m/s
                        float velocity_mph = velocity_mps * 2.237f; // mph
                        
                        // Display velocity
                        std::string vel_text = "Speed: " + std::to_string((int)velocity_mph) + " mph";
                        cv::putText(frame, vel_text, cv::Point(10, 30), 
                                   cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 0), 2);
                    }
                    
                    // Draw current ball
                    cv::circle(frame, center, radius, cv::Scalar(0, 255, 0), 3);
                    cv::circle(frame, center, 2, cv::Scalar(0, 0, 255), 3);
                    
                    // Print position
                    std::cout << "Ball at (" << center.x << ", " << center.y << ") radius: " << radius;
                    if (ball_trail.size() >= 2) {
                        cv::Point2f prev_pos = ball_trail[ball_trail.size()-2];
                        float distance = cv::norm(center - prev_pos);
                        std::cout << " | Movement: " << distance << " pixels";
                    }
                    std::cout << std::endl;
                }
                
                // Draw ball trail
                if (ball_trail.size() > 1) {
                    for (size_t k = 1; k < ball_trail.size(); k++) {
                        // Fade trail colors from bright to dim
                        float alpha = (float)k / ball_trail.size();
                        cv::Scalar color = cv::Scalar(255 * alpha, 100 * alpha, 255 * alpha);
                        
                        // Draw line between consecutive positions
                        cv::line(frame, ball_trail[k-1], ball_trail[k], color, 2);
                        // Draw small circles at each position
                        cv::circle(frame, ball_trail[k], 3, color, -1);
                    }
                }
                
                std::string window_name = "Ball Detection - Camera " + std::to_string(i);
                cv::imshow(window_name, frame);
            } else {
                std::cout << "Failed to read from camera " << i << std::endl;
            }
        }
        
        if (cv::waitKey(1) == 27) break; // ESC key
    }
    
    return 0;
}