#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <deque>
#include "detect.h"
#include "calculate.h"

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
    bool flip_top = false;
    bool flip_bottom = true;
    bool monitoring = false;
    bool swap = false;
    bool show_viz = true;
    
    cv::Mat prev_top, prev_bottom;
    std::deque<ball_detection> dets_top, dets_bottom;
    std::vector<cv::Mat> saved_frames;
    image_texture frame_tex[3];

    bool motion = false;
    int cooldown = 0;
    const int burst_frames = 15;
    const int motion_thresh = 30;
    
    ball_detector detector(200, 0.7);
    shot_calculator calc;
    shot_data shot;

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
            if (ImGui::BeginMenu("view")) {
                ImGui::Checkbox("overlay", &show_overlay);
                ImGui::Checkbox("detection viz", &show_viz);
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
            ImGui::Image(t1, ImVec2(cam_sz, cam_sz));
        } else {
            ImGui::BeginChild("ph1", ImVec2(cam_sz, cam_sz), true);
            ImGui::Text("top cam\nno frame");
            ImGui::EndChild();
        }

        void* t2 = tex_bottom.get_id();
        if (t2) {
            ImGui::Image(t2, ImVec2(cam_sz, cam_sz));
        } else {
            ImGui::BeginChild("ph2", ImVec2(cam_sz, cam_sz), true);
            ImGui::Text("bottom cam\nno frame");
            ImGui::EndChild();
        }

        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("metrics", ImVec2(right_w, left_w * 2), false, ImGuiWindowFlags_NoScrollbar);

        ImGui::Spacing();
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
            }
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
        } else {
            ImGui::Text("status: idle");
        }
        
        if (shot.valid && ImGui::Button("reset", ImVec2(right_w - 20, 30))) {
            shot = shot_data();
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
                
                if (!prev_top.empty() && !prev_bottom.empty() && !motion && cooldown == 0) {
                    cv::Mat diff_top, diff_bottom;
                    cv::absdiff(g_top, prev_top, diff_top);
                    cv::absdiff(g_bottom, prev_bottom, diff_bottom);
                    
                    double m_top = cv::mean(diff_top)[0];
                    double m_bottom = cv::mean(diff_bottom)[0];
                    
                    if (m_top > motion_thresh || m_bottom > motion_thresh) {
                        motion = true;
                        dets_top.clear();
                        dets_bottom.clear();
                        saved_frames.clear();
                        std::cout << "motion detected" << std::endl;
                    }
                }
                
                if (motion && dets_top.size() < burst_frames) {
                    ball_detection d_top, d_bottom;

                    if (show_viz) {
                        cv::Mat viz_top = g_top.clone();
                        cv::Mat viz_bottom = g_bottom.clone();
                        d_top = detector.find_ball_visual(viz_top);
                        d_bottom = detector.find_ball_visual(viz_bottom);
                        if (saved_frames.size() < 3) {
                            saved_frames.push_back(viz_top.clone());
                        }
                    } else {
                        d_top = detector.find_ball(g_top);
                        d_bottom = detector.find_ball(g_bottom);
                        if (saved_frames.size() < 3) {
                            saved_frames.push_back(g_top.clone());
                        }
                    }

                    if (d_top.found) dets_top.push_back(d_top);
                    if (d_bottom.found) dets_bottom.push_back(d_bottom);
                    
                    if (dets_top.size() >= burst_frames || dets_bottom.size() >= burst_frames) {
                        motion = false;
                        cooldown = 90;
                        
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