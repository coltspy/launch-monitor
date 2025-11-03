#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <deque>
#include "detect.h"
#include "calculate.h"
#include "config.h"

class image_texture {
private:
    int width, height;
    GLuint texture_id;
public:
    image_texture() : width(0), height(0), texture_id(0) {}

    ~image_texture() {
        if (texture_id != 0) {
            glDeleteTextures(1, &texture_id);
        }
    }
    void update(cv::Mat& frame) {
        if (frame.empty()) return;

        width = frame.cols;
        height = frame.rows;

        cv::Mat rgb;
        if (frame.channels() == 1) {
            cv::cvtColor(frame, rgb, cv::COLOR_GRAY2RGB);
        } else {
            cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
        }

        if (texture_id == 0) {
            glGenTextures(1, &texture_id);
        }
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    void* get_id() {
        return (void*)(intptr_t)texture_id;
    }
    ImVec2 size() {
        return ImVec2((float)width, (float)height);
    }
};

static void overlay(float fps, bool cam0, bool cam1) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    const float pad = 10.0f;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 pos = vp->WorkPos;
    ImVec2 sz = vp->WorkSize;
    ImVec2 win_pos, pivot;
    win_pos.x = pos.x + sz.x - pad;
    win_pos.y = pos.y + sz.y - pad;
    pivot.x = 1.0f;
    pivot.y = 1.0f;
    ImGui::SetNextWindowPos(win_pos, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowBgAlpha(0.35f);

    if (ImGui::Begin("overlay", nullptr, flags)) {
        ImGui::Text("fps: %.1f", fps);
        ImGui::Text("top: %s", cam0 ? "ok" : "x");
        ImGui::Text("bottom: %s", cam1 ? "ok" : "x");
    }
    ImGui::End();
}

int main() {
    bool show_overlay = true;
    bool flip_top = true;
    bool flip_bottom = false;
    bool monitoring = false;
    bool swap = false;
    bool show_viz = true;
    bool debug_mode = true;
    bool test_mode = false;

    app_config config;
    std::string config_file = "launch_monitor.conf";

    cv::Mat prev_top, prev_bottom;
    std::deque<ball_detection> dets_top, dets_bottom;
    std::vector<cv::Mat> saved_frames;
    std::vector<cv::Mat> all_captured_frames;

    
    const int pre_trigger_buffer_size = 15; 
    std::deque<cv::Mat> frame_buffer_top;
    std::deque<cv::Mat> frame_buffer_bottom;
    image_texture frame_tex[3];
    image_texture debug_tex_top;
    image_texture debug_tex_bottom;
    image_texture playback_tex;
    image_texture streak_tex; 
    int playback_frame = 0;
    bool show_playback = false;
    bool show_streak = false;

    bool motion = false;
    int cooldown = 0;
    const int burst_frames = 40; 
    const int motion_thresh = 30;

    ball_detection prev_ball_top, prev_ball_bottom;
    bool have_prev_ball = false;
    const float ball_motion_threshold = 50.0f; 
    int frames_since_prev = 0;

    ball_detector detector_top(200, 0.7);
    ball_detector detector_bottom(200, 0.7);
    shot_calculator calc;
    shot_data shot;

    detection_debug debug_top, debug_bottom;

    bool use_roi_top = false;
    bool use_roi_bottom = false;
    int roi_x_top = 250, roi_y_top = 200, roi_w_top = 640, roi_h_top = 360;
    int roi_x_bottom = 250, roi_y_bottom = 200, roi_w_bottom = 640, roi_h_bottom = 360;

    if (!glfwInit()) {
        std::cerr << "glfw init failed" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    GLFWwindow* win = glfwCreateWindow(1280, 720, "launch monitor", NULL, NULL);
    if (!win) {
        std::cerr << "window create failed" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    image_texture tex_top;
    image_texture tex_bottom;

    std::cout << "init cameras..." << std::endl;
    cv::VideoCapture cap_top(0, cv::CAP_V4L2);
    cv::VideoCapture cap_bottom(2, cv::CAP_V4L2);

    bool ready = false;
    bool top_ok = false;
    bool bottom_ok = false;
    
    if (cap_top.isOpened()) {
        cap_top.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
        cap_top.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
        cap_top.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
        cap_top.set(cv::CAP_PROP_FPS, 120);
        cap_top.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25); 
        cap_top.set(cv::CAP_PROP_EXPOSURE, -6); 

        cv::Mat test;
        if (cap_top.read(test) && !test.empty()) {
            top_ok = true;
            std::cout << "top ok: " << test.cols << "x" << test.rows << std::endl;
        }
    }

    if (cap_bottom.isOpened()) {
        cap_bottom.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
        cap_bottom.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
        cap_bottom.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
        cap_bottom.set(cv::CAP_PROP_FPS, 120);
        cap_bottom.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25); 
        cap_bottom.set(cv::CAP_PROP_EXPOSURE, -6); 

        cv::Mat test;
        if (cap_bottom.read(test) && !test.empty()) {
            bottom_ok = true;
            std::cout << "bottom ok: " << test.cols << "x" << test.rows << std::endl;
        }
    }
    
    ready = top_ok && bottom_ok;
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                 ImGuiWindowFlags_MenuBar;

        ImGui::Begin("launch monitor", nullptr, flags);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("file")) {
                if (ImGui::MenuItem("save config")) {
                    config.flip_top = flip_top;
                    config.flip_bottom = flip_bottom;
                    config.swap = swap;
                    config.top_detector.threshold = detector_top.get_threshold();
                    config.top_detector.circularity = detector_top.get_circularity();
                    config.top_detector.min_area = detector_top.get_min_area();
                    config.top_detector.max_area = detector_top.get_max_area();
                    config.top_detector.use_roi = use_roi_top;
                    config.top_detector.roi = cv::Rect(roi_x_top, roi_y_top, roi_w_top, roi_h_top);
                    config.bottom_detector.threshold = detector_bottom.get_threshold();
                    config.bottom_detector.circularity = detector_bottom.get_circularity();
                    config.bottom_detector.min_area = detector_bottom.get_min_area();
                    config.bottom_detector.max_area = detector_bottom.get_max_area();
                    config.bottom_detector.use_roi = use_roi_bottom;
                    config.bottom_detector.roi = cv::Rect(roi_x_bottom, roi_y_bottom, roi_w_bottom, roi_h_bottom);
                    config.save(config_file);
                }
                if (ImGui::MenuItem("load config")) {
                    if (config.load(config_file)) {
                        flip_top = config.flip_top;
                        flip_bottom = config.flip_bottom;
                        swap = config.swap;
                        detector_top.set_threshold(config.top_detector.threshold);
                        detector_top.set_circularity(config.top_detector.circularity);
                        detector_top.set_min_area(config.top_detector.min_area);
                        detector_top.set_max_area(config.top_detector.max_area);
                        use_roi_top = config.top_detector.use_roi;
                        roi_x_top = config.top_detector.roi.x;
                        roi_y_top = config.top_detector.roi.y;
                        roi_w_top = config.top_detector.roi.width;
                        roi_h_top = config.top_detector.roi.height;
                        detector_bottom.set_threshold(config.bottom_detector.threshold);
                        detector_bottom.set_circularity(config.bottom_detector.circularity);
                        detector_bottom.set_min_area(config.bottom_detector.min_area);
                        detector_bottom.set_max_area(config.bottom_detector.max_area);
                        use_roi_bottom = config.bottom_detector.use_roi;
                        roi_x_bottom = config.bottom_detector.roi.x;
                        roi_y_bottom = config.bottom_detector.roi.y;
                        roi_w_bottom = config.bottom_detector.roi.width;
                        roi_h_bottom = config.bottom_detector.roi.height;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("view")) {
                ImGui::Checkbox("overlay", &show_overlay);
                ImGui::Checkbox("detection viz", &show_viz);
                ImGui::Checkbox("debug mode", &debug_mode);
                ImGui::Separator();
                ImGui::Checkbox("flip top", &flip_top);
                ImGui::Checkbox("flip bottom", &flip_bottom);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        float w = io.DisplaySize.x;
        float h = io.DisplaySize.y;
        float cam_sz = h * 0.45f;
        float left_w = cam_sz + 10;
        float right_w = w - left_w - 10;

        ImGui::BeginChild("cams", ImVec2(left_w, cam_sz * 2), false, ImGuiWindowFlags_NoScrollbar);

        void* t1 = tex_top.get_id();
        if (t1) {
            ImVec2 img_pos = ImGui::GetCursorScreenPos();
            float img_w = cam_sz;
            float img_h = cam_sz * 0.5625f;
            ImGui::Image(t1, ImVec2(img_w, img_h));

            if (debug_mode && use_roi_top) {
                float scale_x = img_w / 1280.0f;
                float scale_y = img_h / 720.0f;
                ImVec2 roi_tl(img_pos.x + roi_x_top * scale_x, img_pos.y + roi_y_top * scale_y);
                ImVec2 roi_br(roi_tl.x + roi_w_top * scale_x, roi_tl.y + roi_h_top * scale_y);
                ImGui::GetWindowDrawList()->AddRect(roi_tl, roi_br, IM_COL32(0, 255, 0, 255), 0.0f, 0, 2.0f);
            }
        } else {
            ImGui::BeginChild("ph1", ImVec2(cam_sz, cam_sz * 0.5625f), true);
            ImGui::Text("top cam\nno frame");
            ImGui::EndChild();
        }

        void* t2 = tex_bottom.get_id();
        if (t2) {
            ImVec2 img_pos = ImGui::GetCursorScreenPos();
            float img_w = cam_sz;
            float img_h = cam_sz * 0.5625f;
            ImGui::Image(t2, ImVec2(img_w, img_h));

            if (debug_mode && use_roi_bottom) {
                float scale_x = img_w / 1280.0f;
                float scale_y = img_h / 720.0f;
                ImVec2 roi_tl(img_pos.x + roi_x_bottom * scale_x, img_pos.y + roi_y_bottom * scale_y);
                ImVec2 roi_br(roi_tl.x + roi_w_bottom * scale_x, roi_tl.y + roi_h_bottom * scale_y);
                ImGui::GetWindowDrawList()->AddRect(roi_tl, roi_br, IM_COL32(0, 255, 0, 255), 0.0f, 0, 2.0f);
            }
        } else {
            ImGui::BeginChild("ph2", ImVec2(cam_sz, cam_sz * 0.5625f), true);
            ImGui::Text("bottom cam\nno frame");
            ImGui::EndChild();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("metrics", ImVec2(right_w, left_w * 2), false, ImGuiWindowFlags_NoScrollbar);

        if (debug_mode) {
            if (ImGui::CollapsingHeader("Top Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                float controls_width = (right_w - 20) * 0.55f;
                float debug_width = (right_w - 20) * 0.45f;

                ImGui::BeginChild("top_controls", ImVec2(controls_width, 280), false);

                int thresh_top = detector_top.get_threshold();
                if (ImGui::SliderInt("threshold##top", &thresh_top, 50, 255)) {
                    detector_top.set_threshold(thresh_top);
                }

                float circ_top = detector_top.get_circularity();
                if (ImGui::SliderFloat("circularity##top", &circ_top, 0.1f, 1.0f)) {
                    detector_top.set_circularity(circ_top);
                }

                float min_a_top = detector_top.get_min_area();
                if (ImGui::SliderFloat("min area##top", &min_a_top, 10.0f, 500.0f)) {
                    detector_top.set_min_area(min_a_top);
                }

                float max_a_top = detector_top.get_max_area();
                if (ImGui::SliderFloat("max area##top", &max_a_top, 500.0f, 50000.0f)) {
                    detector_top.set_max_area(max_a_top);
                }

                ImGui::Checkbox("use ROI##top", &use_roi_top);
                if (use_roi_top) {
                    ImGui::SliderInt("x##top", &roi_x_top, 0, 1280);
                    ImGui::SliderInt("y##top", &roi_y_top, 0, 720);
                    ImGui::SliderInt("w##top", &roi_w_top, 50, 1280);
                    ImGui::SliderInt("h##top", &roi_h_top, 50, 720);
                    detector_top.set_roi(cv::Rect(roi_x_top, roi_y_top, roi_w_top, roi_h_top));
                } else {
                    detector_top.disable_roi();
                }

                ImGui::Spacing();
                ImGui::Text("bright: %.0f | cnt: %d | area: %d | circ: %d",
                    debug_top.max_brightness, debug_top.contours_found,
                    debug_top.contours_passed_area, debug_top.contours_passed_circularity);

                if (ImGui::TreeNode("Contours##top")) {
                    for (size_t i = 0; i < debug_top.all_contours.size() && i < 3; i++) {
                        const auto& c = debug_top.all_contours[i];
                        ImGui::Text("%.0f px, %.2f %s%s",
                            c.area, c.circularity,
                            c.passed_area ? "" : "[a]",
                            c.passed_circularity ? "" : "[c]");
                    }
                    if (debug_top.all_contours.size() > 3) {
                        ImGui::Text("...%zu more", debug_top.all_contours.size() - 3);
                    }
                    ImGui::TreePop();
                }

                ImGui::EndChild();
                ImGui::SameLine();

                ImGui::BeginChild("top_debug_img", ImVec2(debug_width, 280), true);
                void* dt_top = debug_tex_top.get_id();
                if (dt_top) {
                    ImVec2 tex_size = debug_tex_top.size();
                    float aspect = tex_size.x / tex_size.y;
                    float fit_w = debug_width - 10;
                    float fit_h = fit_w / aspect;
                    if (fit_h > 270) {
                        fit_h = 270;
                        fit_w = fit_h * aspect;
                    }
                    ImGui::Image(dt_top, ImVec2(fit_w, fit_h));
                } else {
                    ImGui::Text("No debug image");
                }
                ImGui::EndChild();
            }

            if (ImGui::CollapsingHeader("Bottom Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                float controls_width = (right_w - 20) * 0.55f;
                float debug_width = (right_w - 20) * 0.45f;

                ImGui::BeginChild("bottom_controls", ImVec2(controls_width, 280), false);

                int thresh_bottom = detector_bottom.get_threshold();
                if (ImGui::SliderInt("threshold##bottom", &thresh_bottom, 50, 255)) {
                    detector_bottom.set_threshold(thresh_bottom);
                }

                float circ_bottom = detector_bottom.get_circularity();
                if (ImGui::SliderFloat("circularity##bottom", &circ_bottom, 0.1f, 1.0f)) {
                    detector_bottom.set_circularity(circ_bottom);
                }

                float min_a_bottom = detector_bottom.get_min_area();
                if (ImGui::SliderFloat("min area##bottom", &min_a_bottom, 10.0f, 500.0f)) {
                    detector_bottom.set_min_area(min_a_bottom);
                }

                float max_a_bottom = detector_bottom.get_max_area();
                if (ImGui::SliderFloat("max area##bottom", &max_a_bottom, 500.0f, 50000.0f)) {
                    detector_bottom.set_max_area(max_a_bottom);
                }

                ImGui::Checkbox("use ROI##bottom", &use_roi_bottom);
                if (use_roi_bottom) {
                    ImGui::SliderInt("x##bottom", &roi_x_bottom, 0, 1280);
                    ImGui::SliderInt("y##bottom", &roi_y_bottom, 0, 720);
                    ImGui::SliderInt("w##bottom", &roi_w_bottom, 50, 1280);
                    ImGui::SliderInt("h##bottom", &roi_h_bottom, 50, 720);
                    detector_bottom.set_roi(cv::Rect(roi_x_bottom, roi_y_bottom, roi_w_bottom, roi_h_bottom));
                } else {
                    detector_bottom.disable_roi();
                }

                ImGui::Spacing();
                ImGui::Text("bright: %.0f | cnt: %d | area: %d | circ: %d",
                    debug_bottom.max_brightness, debug_bottom.contours_found,
                    debug_bottom.contours_passed_area, debug_bottom.contours_passed_circularity);

                if (ImGui::TreeNode("Contours##bottom")) {
                    for (size_t i = 0; i < debug_bottom.all_contours.size() && i < 3; i++) {
                        const auto& c = debug_bottom.all_contours[i];
                        ImGui::Text("%.0f px, %.2f %s%s",
                            c.area, c.circularity,
                            c.passed_area ? "" : "[a]",
                            c.passed_circularity ? "" : "[c]");
                    }
                    if (debug_bottom.all_contours.size() > 3) {
                        ImGui::Text("...%zu more", debug_bottom.all_contours.size() - 3);
                    }
                    ImGui::TreePop();
                }
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild("bottom_debug_img", ImVec2(debug_width, 280), true);
                void* dt_bottom = debug_tex_bottom.get_id();
                if (dt_bottom) {
                    ImVec2 tex_size = debug_tex_bottom.size();
                    float aspect = tex_size.x / tex_size.y;
                    float fit_w = debug_width - 10;
                    float fit_h = fit_w / aspect;
                    if (fit_h > 270) {
                        fit_h = 270;
                        fit_w = fit_h * aspect;
                    }
                    ImGui::Image(dt_bottom, ImVec2(fit_w, fit_h));
                } else {
                    ImGui::Text("No debug image");
                }
                ImGui::EndChild();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        ImGui::Text("shot metrics");
        ImGui::Separator();
        ImGui::Spacing();

        if (shot.valid) {
            ImGui::Text("speed: %.1f mph", shot.speed_mph);
            ImGui::Text("angle: %.1f deg", shot.launch_angle_deg);
            ImGui::Text("carry: %.0f ft", shot.carry_ft);
            ImGui::Text("total: %.0f ft", shot.distance_ft);
        } else {
            ImGui::Text("speed: --");
            ImGui::Text("angle: --");
            ImGui::Text("carry: --");
            ImGui::Text("total: --");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button(monitoring ? "stop" : "start", ImVec2(right_w - 20, 40))) {
            monitoring = !monitoring;
            if (monitoring) {
                shot = shot_data();
                motion = false;
                cooldown = 0;
                test_mode = false;
                have_prev_ball = false;
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("test capture", ImVec2(right_w - 20, 30))) {
            test_mode = true;
            dets_top.clear();
            dets_bottom.clear();
            saved_frames.clear();
            all_captured_frames.clear();
            shot = shot_data();
            playback_frame = 0;
            show_playback = false;
            std::cout << "manual test capture triggered" << std::endl;
        }

        ImGui::Spacing();
        if (monitoring) {
            ImGui::Text("status: active");
            if (motion) {
                ImGui::Text("capturing %d/%d", (int)dets_top.size(), burst_frames);
            } else if (cooldown > 0) {
                ImGui::Text("cooldown %d", cooldown);
            } else {
                ImGui::Text("waiting...");
            }
        } else if (test_mode) {
            ImGui::Text("status: test mode");
            ImGui::Text("captured: top %d, bottom %d", (int)dets_top.size(), (int)dets_bottom.size());
        } else {
            ImGui::Text("status: idle");
        }
        
        if (shot.valid && ImGui::Button("reset", ImVec2(right_w - 20, 30))) {
            shot = shot_data();
            all_captured_frames.clear();
            show_playback = false;
        }

        ImGui::Spacing();
        if (!all_captured_frames.empty()) {
            if (ImGui::Button(show_playback ? "hide playback" : "show playback", ImVec2((right_w - 30) / 2, 30))) {
                show_playback = !show_playback;
            }
            ImGui::SameLine();
            if (ImGui::Button(show_streak ? "hide streak" : "show streak", ImVec2((right_w - 30) / 2, 30))) {
                show_streak = !show_streak;
            }

            if (show_playback) {
                ImGui::Text("frame %d / %d", playback_frame + 1, (int)all_captured_frames.size());
                if (ImGui::SliderInt("##playback", &playback_frame, 0, all_captured_frames.size() - 1)) {
                    if (playback_frame >= 0 && playback_frame < all_captured_frames.size()) {
                        playback_tex.update(all_captured_frames[playback_frame]);
                    }
                }

                void* playback_id = playback_tex.get_id();
                if (playback_id) {
                    float playback_w = right_w - 40;
                    ImGui::Image(playback_id, ImVec2(playback_w, playback_w * 0.5625f));
                }
            }

            if (show_streak) {
                ImGui::Text("composite view - all ball positions");
                void* streak_id = streak_tex.get_id();
                if (streak_id) {
                    float streak_w = right_w - 40;
                    ImGui::Image(streak_id, ImVec2(streak_w, streak_w * 0.5625f));
                }
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("swap cams", ImVec2(right_w - 20, 30))) {
            swap = !swap;
        }

        if (!saved_frames.empty()) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("captured frames");
            float thumb_sz = (right_w - 30) / 3.0f;
            for (int i = 0; i < 3 && i < saved_frames.size(); i++) {
                if (i > 0) ImGui::SameLine();
                void* tid = frame_tex[i].get_id();
                if (tid) {
                    ImGui::Image(tid, ImVec2(thumb_sz, thumb_sz));
                }
            }
        }

        ImGui::EndChild();
        ImGui::End();

        if (show_overlay) {
            overlay(io.Framerate, top_ok, bottom_ok);
        }
        
        if (ready) {
            cv::Mat f_top, f_bottom;
            bool got_top = cap_top.read(f_top);
            bool got_bottom = cap_bottom.read(f_bottom);
            if (got_top && got_bottom) {
                if (flip_top) cv::flip(f_top, f_top, -1);
                if (flip_bottom) cv::flip(f_bottom, f_bottom, -1);

                cv::Mat g_top, g_bottom;
                if (f_top.channels() == 3) {
                    cv::cvtColor(f_top, g_top, cv::COLOR_BGR2GRAY);
                } else {
                    g_top = f_top;
                }

                if (f_bottom.channels() == 3) {
                    cv::cvtColor(f_bottom, g_bottom, cv::COLOR_BGR2GRAY);
                } else {
                    g_bottom = f_bottom;
                }

                if (debug_mode) {
                    detector_top.find_ball_debug(g_top, debug_top);
                    detector_bottom.find_ball_debug(g_bottom, debug_bottom);
                    if (!debug_top.morphed_img.empty()) {
                        debug_tex_top.update(debug_top.morphed_img);
                    }
                    if (!debug_bottom.morphed_img.empty()) {
                        debug_tex_bottom.update(debug_bottom.morphed_img);
                    }
                }

                if (!monitoring) {
                    if (swap) {
                        tex_top.update(g_bottom);
                        tex_bottom.update(g_top);
                    } else {
                        tex_top.update(g_top);
                        tex_bottom.update(g_bottom);
                    }
                }

                if (test_mode && dets_top.size() < burst_frames) {
                    ball_detection d_top, d_bottom;

                    if (show_viz) {
                        cv::Mat viz_top = g_top.clone();
                        cv::Mat viz_bottom = g_bottom.clone();
                        d_top = detector_top.find_ball_visual(viz_top);
                        d_bottom = detector_bottom.find_ball_visual(viz_bottom);
                        if (saved_frames.size() < 3) {
                            cv::Mat combined;
                            cv::vconcat(viz_top, viz_bottom, combined);
                            saved_frames.push_back(combined);
                        }
                    } else {
                        d_top = detector_top.find_ball(g_top);
                        d_bottom = detector_bottom.find_ball(g_bottom);
                        if (saved_frames.size() < 3) {
                            cv::Mat combined;
                            cv::vconcat(g_top, g_bottom, combined);
                            saved_frames.push_back(combined);
                        }
                    }

                    if (d_top.found) {
                        dets_top.push_back(d_top);
                        std::cout << "top: ball at (" << d_top.position.x << ", " << d_top.position.y
                                  << ") r=" << d_top.radius << std::endl;
                    }
                    if (d_bottom.found) {
                        dets_bottom.push_back(d_bottom);
                        std::cout << "bottom: ball at (" << d_bottom.position.x << ", " << d_bottom.position.y
                                  << ") r=" << d_bottom.radius << std::endl;
                    }

                    if (dets_top.size() >= burst_frames || dets_bottom.size() >= burst_frames) {
                        test_mode = false;

                        std::vector<ball_detection> v_top(dets_top.begin(), dets_top.end());
                        std::vector<ball_detection> v_bottom(dets_bottom.begin(), dets_bottom.end());

                        if (swap) {
                            shot = calc.calculate_shot(v_bottom, v_top);
                        } else {
                            shot = calc.calculate_shot(v_top, v_bottom);
                        }

                        for (int i = 0; i < saved_frames.size() && i < 3; i++) {
                            frame_tex[i].update(saved_frames[i]);
                        }

                        std::cout << "test capture complete! top: " << v_top.size()
                                  << " bottom: " << v_bottom.size() << std::endl;
                    }
                }
            }
        }

        if (monitoring && ready) {
            cv::Mat f_top, f_bottom;
            bool got_top = cap_top.read(f_top);
            bool got_bottom = cap_bottom.read(f_bottom);
            if (got_top && got_bottom) {
                if (flip_top) cv::flip(f_top, f_top, -1);
                if (flip_bottom) cv::flip(f_bottom, f_bottom, -1);

                cv::Mat g_top, g_bottom;
                if (f_top.channels() == 3) {
                    cv::cvtColor(f_top, g_top, cv::COLOR_BGR2GRAY);
                } else {
                    g_top = f_top;
                }

                if (f_bottom.channels() == 3) {
                    cv::cvtColor(f_bottom, g_bottom, cv::COLOR_BGR2GRAY);
                } else {
                    g_bottom = f_bottom;
                }
                
                
                frame_buffer_top.push_back(g_top.clone());
                frame_buffer_bottom.push_back(g_bottom.clone());
                if (frame_buffer_top.size() > pre_trigger_buffer_size) {
                    frame_buffer_top.pop_front();
                    frame_buffer_bottom.pop_front();
                }

                if (!motion && cooldown == 0) {
                    
                    ball_detection curr_ball_top = detector_top.find_ball(g_top);
                    ball_detection curr_ball_bottom = detector_bottom.find_ball(g_bottom);

                    frames_since_prev++;

                    
                    if (have_prev_ball && frames_since_prev <= 3 && (curr_ball_top.found || curr_ball_bottom.found)) {
                        float ball_movement = 0;

                        if (curr_ball_top.found && prev_ball_top.found) {
                            float dx = curr_ball_top.position.x - prev_ball_top.position.x;
                            float dy = curr_ball_top.position.y - prev_ball_top.position.y;
                            ball_movement = sqrt(dx * dx + dy * dy);
                        }

                        if (curr_ball_bottom.found && prev_ball_bottom.found) {
                            float dx = curr_ball_bottom.position.x - prev_ball_bottom.position.x;
                            float dy = curr_ball_bottom.position.y - prev_ball_bottom.position.y;
                            float bottom_movement = sqrt(dx * dx + dy * dy);
                            ball_movement = std::max(ball_movement, bottom_movement);
                        }

                        if (ball_movement > ball_motion_threshold) {
                            motion = true;
                            dets_top.clear();
                            dets_bottom.clear();
                            saved_frames.clear();
                            all_captured_frames.clear();

                            
                            std::cout << "ball motion detected: " << ball_movement << " pixels in "
                                      << frames_since_prev << " frames" << std::endl;
                            std::cout << "adding " << frame_buffer_top.size() << " pre-trigger frames" << std::endl;

                            
                            for (size_t i = 0; i < frame_buffer_top.size(); i++) {
                                cv::Mat buffered_top = frame_buffer_top[i];
                                cv::Mat buffered_bottom = frame_buffer_bottom[i];

                                if (show_viz) {
                                    cv::Mat viz_top = buffered_top.clone();
                                    cv::Mat viz_bottom = buffered_bottom.clone();
                                    ball_detection d_top = detector_top.find_ball_visual(viz_top);
                                    ball_detection d_bottom = detector_bottom.find_ball_visual(viz_bottom);

                                    if (d_top.found) dets_top.push_back(d_top);
                                    if (d_bottom.found) dets_bottom.push_back(d_bottom);

                                    cv::Mat combined;
                                    cv::vconcat(viz_top, viz_bottom, combined);
                                    all_captured_frames.push_back(combined);
                                } else {
                                    ball_detection d_top = detector_top.find_ball(buffered_top);
                                    ball_detection d_bottom = detector_bottom.find_ball(buffered_bottom);

                                    if (d_top.found) dets_top.push_back(d_top);
                                    if (d_bottom.found) dets_bottom.push_back(d_bottom);

                                    cv::Mat combined;
                                    cv::vconcat(buffered_top, buffered_bottom, combined);
                                    all_captured_frames.push_back(combined);
                                }
                            }

                            
                            frame_buffer_top.clear();
                            frame_buffer_bottom.clear();
                        }
                    }

                    
                    if (frames_since_prev >= 3) {
                        prev_ball_top = curr_ball_top;
                        prev_ball_bottom = curr_ball_bottom;
                        frames_since_prev = 0;
                        if (curr_ball_top.found || curr_ball_bottom.found) {
                            have_prev_ball = true;
                        }
                    }
                }
                
                if (motion && all_captured_frames.size() < burst_frames) {
                    ball_detection d_top, d_bottom;

                    if (show_viz) {
                        cv::Mat viz_top = g_top.clone();
                        cv::Mat viz_bottom = g_bottom.clone();
                        d_top = detector_top.find_ball_visual(viz_top);
                        d_bottom = detector_bottom.find_ball_visual(viz_bottom);

                        
                        if (saved_frames.size() < 3) {
                            cv::Mat combined;
                            cv::vconcat(viz_top, viz_bottom, combined);
                            saved_frames.push_back(combined);
                        }

                        
                        cv::Mat playback_combined;
                        cv::vconcat(viz_top, viz_bottom, playback_combined);
                        all_captured_frames.push_back(playback_combined);
                    } else {
                        d_top = detector_top.find_ball(g_top);
                        d_bottom = detector_bottom.find_ball(g_bottom);

                        if (saved_frames.size() < 3) {
                            cv::Mat combined;
                            cv::vconcat(g_top, g_bottom, combined);
                            saved_frames.push_back(combined);
                        }

                        cv::Mat playback_combined;
                        cv::vconcat(g_top, g_bottom, playback_combined);
                        all_captured_frames.push_back(playback_combined);
                    }

                    if (d_top.found) dets_top.push_back(d_top);
                    if (d_bottom.found) dets_bottom.push_back(d_bottom);

                    if (all_captured_frames.size() >= burst_frames) {
                        motion = false;
                        cooldown = 90;
                        have_prev_ball = false;

                        std::vector<ball_detection> v_top(dets_top.begin(), dets_top.end());
                        std::vector<ball_detection> v_bottom(dets_bottom.begin(), dets_bottom.end());

                        if (swap) {
                            shot = calc.calculate_shot(v_bottom, v_top);
                        } else {
                            shot = calc.calculate_shot(v_top, v_bottom);
                        }

                        for (int i = 0; i < saved_frames.size() && i < 3; i++) {
                            frame_tex[i].update(saved_frames[i]);
                        }

                        
                        if (!all_captured_frames.empty() && (!v_top.empty() || !v_bottom.empty())) {
                            std::cout << "generating streak view with " << v_top.size() << " top + "
                                     << v_bottom.size() << " bottom detections..." << std::endl;

                            
                            int first_ball_frame = -1;
                            cv::Point2f prev_ball_pos(-1, -1);

                            for (size_t i = 0; i < v_top.size(); i++) {
                                if (v_top[i].found) {
                                    if (prev_ball_pos.x < 0) {
                                        prev_ball_pos = v_top[i].position;
                                    } else {
                                        float dist = cv::norm(v_top[i].position - prev_ball_pos);
                                        if (dist > 10.0f) {
                                            first_ball_frame = i;
                                            break;
                                        }
                                    }
                                }
                            }

                            
                            int bg_frame = (first_ball_frame > 0) ? first_ball_frame : 15;
                            if (bg_frame >= (int)all_captured_frames.size()) bg_frame = all_captured_frames.size() - 1;

                            cv::Mat streak_img = all_captured_frames[bg_frame].clone();

                            std::cout << "using frame " << bg_frame << "/" << all_captured_frames.size()
                                     << " as background" << std::endl;

                            
                            cv::line(streak_img, cv::Point(0, 720), cv::Point(1280, 720),
                                    cv::Scalar(255, 0, 255), 3);
                            cv::putText(streak_img, "BALL FLIGHT", cv::Point(20, 30),
                                       cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);

                            
                            std::vector<cv::Point2f> ball_positions;
                            int frame_num = 0;
                            const float MIN_MOVEMENT = 10.0f; 

                            
                            for (const auto& det : v_top) {
                                if (det.found) {
                                    
                                    cv::Point2f pos(det.position.x + (use_roi_top ? roi_x_top : 0),
                                                   det.position.y + (use_roi_top ? roi_y_top : 0));

                                    
                                    bool should_draw = ball_positions.empty();
                                    if (!ball_positions.empty()) {
                                        float dist = cv::norm(ball_positions.back() - pos);
                                        if (dist > MIN_MOVEMENT) {
                                            should_draw = true;
                                            cv::line(streak_img, ball_positions.back(), pos,
                                                    cv::Scalar(0, 150, 255), 2);
                                        }
                                    }

                                    if (should_draw) {
                                        ball_positions.push_back(pos);
                                        cv::circle(streak_img, pos, (int)det.radius + 5, cv::Scalar(0, 255, 0), 4);
                                        cv::circle(streak_img, pos, 10, cv::Scalar(0, 255, 255), -1);
                                        cv::putText(streak_img, std::to_string(frame_num),
                                                   cv::Point(pos.x + 20, pos.y - 20),
                                                   cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(255, 255, 0), 3);
                                        frame_num++;
                                    }
                                }
                            }

                            
                            for (const auto& det : v_bottom) {
                                if (det.found) {
                                    
                                    cv::Point2f pos(det.position.x + (use_roi_bottom ? roi_x_bottom : 0),
                                                   det.position.y + (use_roi_bottom ? roi_y_bottom : 0) + 720);

                                    
                                    bool should_draw = ball_positions.empty();
                                    if (!ball_positions.empty()) {
                                        float dist = cv::norm(ball_positions.back() - pos);
                                        if (dist > MIN_MOVEMENT) {
                                            should_draw = true;
                                            cv::line(streak_img, ball_positions.back(), pos,
                                                    cv::Scalar(0, 150, 255), 2);
                                        }
                                    }

                                    if (should_draw) {
                                        ball_positions.push_back(pos);
                                        cv::circle(streak_img, pos, (int)det.radius + 5, cv::Scalar(0, 255, 0), 4);
                                        cv::circle(streak_img, pos, 10, cv::Scalar(0, 255, 255), -1);
                                        cv::putText(streak_img, std::to_string(frame_num),
                                                   cv::Point(pos.x + 20, pos.y - 20),
                                                   cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(255, 255, 0), 3);
                                        frame_num++;
                                    }
                                }
                            }

                            streak_tex.update(streak_img);
                            std::cout << "streak view created with " << frame_num << " ball positions" << std::endl;
                        }
                    }
                }
                
                if (swap) {
                    tex_top.update(g_bottom);
                    tex_bottom.update(g_top);
                } else {
                    tex_top.update(g_top);
                    tex_bottom.update(g_bottom);
                }
                
                prev_top = g_top;
                prev_bottom = g_bottom;
            }
        }
        
        if (cooldown > 0) cooldown--;

        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(win, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();

    return 0;
}