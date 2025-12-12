#pragma once

#include <iostream>
#include <filesystem>

#include "IconsFontAwesome5.h"
#include "MP3Player.h"
#include "MP3Visualization.h"

class ImGuiVis : public VisualizationBase
{
  public:
    ImGuiVis(const std::string mp3Source);
    ~ImGuiVis();

  private:
    // Process the data
    void run();

    // ImGui initialization
    GLFWwindow* initFcn();
    std::filesystem::path getExecutableDir() const;

    // Callback functions.
    static void closeRequestCallback(GLFWwindow* window);
    static void glfw_error_callback(int error, const char* description);
    std::string displayTime();

    // Clean up
    void cleanUp();
    void Distroy(GLFWwindow* window);

    // Additional
    void getFontIcon();
    void toLower(std::string& data);

  private:
    Player::MP3Visualization& mMP3PlayerVisualization;

    // Declaring variables
    std::string   mOpenGLVersion = "Unavailable";
    GLFWwindow*   mWorldWindow;
    GLuint        mWorldTexture;
    bool          mPauseRequested;
    float         mBaseFontSize;
    std::mutex    mFontIcon;
    std::string   mMP3SourceFile;
};
