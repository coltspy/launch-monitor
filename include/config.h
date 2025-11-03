#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <fstream>
#include <iostream>

struct detector_config {
    int threshold;
    float circularity;
    float min_area;
    float max_area;
    bool use_roi;
    cv::Rect roi;

    detector_config()
        : threshold(200)
        , circularity(0.7f)
        , min_area(50.0f)
        , max_area(5000.0f)
        , use_roi(false)
        , roi(250, 200, 640, 360)
    {}
};

struct app_config {
    bool flip_top;
    bool flip_bottom;
    bool swap;
    detector_config top_detector;
    detector_config bottom_detector;

    app_config()
        : flip_top(true)
        , flip_bottom(false)
        , swap(false)
    {}

    bool save(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to save config to " << filename << std::endl;
            return false;
        }

        file << "# Launch Monitor Configuration\n";
        file << "flip_top=" << flip_top << "\n";
        file << "flip_bottom=" << flip_bottom << "\n";
        file << "swap=" << swap << "\n";

        file << "\n# Top Camera\n";
        file << "top_threshold=" << top_detector.threshold << "\n";
        file << "top_circularity=" << top_detector.circularity << "\n";
        file << "top_min_area=" << top_detector.min_area << "\n";
        file << "top_max_area=" << top_detector.max_area << "\n";
        file << "top_use_roi=" << top_detector.use_roi << "\n";
        file << "top_roi_x=" << top_detector.roi.x << "\n";
        file << "top_roi_y=" << top_detector.roi.y << "\n";
        file << "top_roi_w=" << top_detector.roi.width << "\n";
        file << "top_roi_h=" << top_detector.roi.height << "\n";

        file << "\n# Bottom Camera\n";
        file << "bottom_threshold=" << bottom_detector.threshold << "\n";
        file << "bottom_circularity=" << bottom_detector.circularity << "\n";
        file << "bottom_min_area=" << bottom_detector.min_area << "\n";
        file << "bottom_max_area=" << bottom_detector.max_area << "\n";
        file << "bottom_use_roi=" << bottom_detector.use_roi << "\n";
        file << "bottom_roi_x=" << bottom_detector.roi.x << "\n";
        file << "bottom_roi_y=" << bottom_detector.roi.y << "\n";
        file << "bottom_roi_w=" << bottom_detector.roi.width << "\n";
        file << "bottom_roi_h=" << bottom_detector.roi.height << "\n";

        file.close();
        std::cout << "Config saved to " << filename << std::endl;
        return true;
    }

    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to load config from " << filename << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);

            if (key == "flip_top") flip_top = (value == "1");
            else if (key == "flip_bottom") flip_bottom = (value == "1");
            else if (key == "swap") swap = (value == "1");

            else if (key == "top_threshold") top_detector.threshold = std::stoi(value);
            else if (key == "top_circularity") top_detector.circularity = std::stof(value);
            else if (key == "top_min_area") top_detector.min_area = std::stof(value);
            else if (key == "top_max_area") top_detector.max_area = std::stof(value);
            else if (key == "top_use_roi") top_detector.use_roi = (value == "1");
            else if (key == "top_roi_x") top_detector.roi.x = std::stoi(value);
            else if (key == "top_roi_y") top_detector.roi.y = std::stoi(value);
            else if (key == "top_roi_w") top_detector.roi.width = std::stoi(value);
            else if (key == "top_roi_h") top_detector.roi.height = std::stoi(value);

            else if (key == "bottom_threshold") bottom_detector.threshold = std::stoi(value);
            else if (key == "bottom_circularity") bottom_detector.circularity = std::stof(value);
            else if (key == "bottom_min_area") bottom_detector.min_area = std::stof(value);
            else if (key == "bottom_max_area") bottom_detector.max_area = std::stof(value);
            else if (key == "bottom_use_roi") bottom_detector.use_roi = (value == "1");
            else if (key == "bottom_roi_x") bottom_detector.roi.x = std::stoi(value);
            else if (key == "bottom_roi_y") bottom_detector.roi.y = std::stoi(value);
            else if (key == "bottom_roi_w") bottom_detector.roi.width = std::stoi(value);
            else if (key == "bottom_roi_h") bottom_detector.roi.height = std::stoi(value);
        }

        file.close();
        std::cout << "Config loaded from " << filename << std::endl;
        return true;
    }
};
