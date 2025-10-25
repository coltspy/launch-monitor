#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <opencv2/opencv.hpp>

class ImageTexture {
private:
    int width, height;
    GLuint texture_id;

public:
    ImageTexture() : width(0), height(0), texture_id(0) {}

    ~ImageTexture() {
        if (texture_id != 0) {
            glDeleteTextures(1, &texture_id);
        }
    }

    void update(cv::Mat& frame) {
        if (frame.empty()) return;

        width = frame.cols;
        height = frame.rows;

        cv::Mat rgb_frame;
        cv::cvtColor(frame, rgb_frame, cv::COLOR_BGR2RGB);

        if (texture_id == 0) {
            glGenTextures(1, &texture_id);
        }

        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, rgb_frame.data);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void* getTextureID() {
        return (void*)(intptr_t)texture_id;
    }

    ImVec2 getSize() {
        return ImVec2((float)width, (float)height);
    }
};

static void Overlay() {
    static int location = 2;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    const float PAD = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 window_pos, window_pos_pivot;

    window_pos.x = (location & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
    window_pos.y = (location & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
    window_pos_pivot.x = (location & 1) ? 1.0f : 0.0f;
    window_pos_pivot.y = (location & 2) ? 1.0f : 0.0f;

    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowBgAlpha(0.35f);

    if (ImGui::Begin("Overlay", nullptr, window_flags)) {
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Camera 0: Connected");
        ImGui::Text("Camera 1: Connected");
    }
    ImGui::End();
}

int main() {
    static bool show_overlay = true;

    if (!glfwInit()) {
        std::cerr << "failed to init glfw" << std::endl;
        return -1;
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "launch monitor", NULL, NULL);
    if (window == nullptr) {
        std::cerr << "failed to create glfw window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImageTexture cam0_texture;
    ImageTexture cam1_texture;

    std::cout << "initializing camera 0..." << std::endl;
    cv::VideoCapture cap(0);
    cv::VideoCapture cap1;

    bool cameras_ready = false;
    if (cap.isOpened()) {
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

        cv::Mat test_frame;
        if (cap.read(test_frame) && !test_frame.empty()) {
            cameras_ready = true;
            std::cout << "cam 0 ready: " << test_frame.cols << "x" << test_frame.rows << std::endl;
        } else {
            std::cerr << "cam opened but cant read frames" << std::endl;
        }
    } else {
        std::cerr << "cant open cam 0" << std::endl;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                         ImGuiWindowFlags_MenuBar;

        ImGui::Begin("launch monitor", nullptr, window_flags);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("view")) {
                ImGui::Checkbox("show overlay", &show_overlay);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        float window_width = io.DisplaySize.x;
        float window_height = io.DisplaySize.y;
        float cam_box_size = window_height * 0.45f;
        float lcw = cam_box_size + 10;
        float rcw = window_width - lcw - 10;

        ImGui::BeginChild("cam column", ImVec2(lcw, cam_box_size * 2), false, ImGuiWindowFlags_NoScrollbar);

        void* tex = cam0_texture.getTextureID();
        if (tex != nullptr) {
            ImGui::Image(tex, ImVec2(cam_box_size, cam_box_size));
        } else {
            ImGui::BeginChild("cam0_placeholder", ImVec2(cam_box_size, cam_box_size), true);
            ImGui::Text("camera 0\nno frame");
            ImGui::EndChild();
        }

        void* tex1 = cam1_texture.getTextureID();
        if (tex1 != nullptr) {
            ImGui::Image(tex1, ImVec2(cam_box_size, cam_box_size));
        } else {
            ImGui::BeginChild("cam1_placeholder", ImVec2(cam_box_size, cam_box_size), true);
            ImGui::Text("camera 1\nno frame");
            ImGui::EndChild();
        }

        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("ballmetrics", ImVec2(rcw, lcw * 2), false, ImGuiWindowFlags_NoScrollbar);

        ImGui::Spacing();
        ImGui::Text("speed: -- mph");
        ImGui::Text("launch angle: -- deg");
        ImGui::Text("distance: -- ft");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("capture shot", ImVec2(rcw - 20, 40))) {
            if (!cameras_ready) {
                std::cout << "cam not initialized" << std::endl;
            } else {
                std::cout << "capturing frame..." << std::endl;

                cv::Mat frame0;
                bool got0 = cap.read(frame0);

                if (got0) {
                    std::cout << "success! frame 0: " << frame0.cols << "x" << frame0.rows << std::endl;

                    cam0_texture.update(frame0);
                    cam1_texture.update(frame0);
                } else {
                    std::cout << "failed to capture frame" << std::endl;
                }
            }
        }

        ImGui::EndChild();

        ImGui::End();

        if (show_overlay) {
            Overlay();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
