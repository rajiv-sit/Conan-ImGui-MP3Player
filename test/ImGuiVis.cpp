#include "ImGuiVis.h"

#include <filesystem>
#include <array>
#include <windows.h>

static ImGuiID _dockSpaceId;

ImGuiVis::ImGuiVis(const std::string mp3Sourc)
    : mMP3PlayerVisualization(Player::MP3Visualization::getInstance())
    , mBaseFontSize(24.0f)
    , mPauseRequested(false)
    , mMP3SourceFile(mp3Sourc)
{
    try
    {
        std::filesystem::current_path(getExecutableDir());
    }
    catch (const std::exception&)
    {
        // if this fails, relative asset paths (fonts/mp3) may not load
    }

    std::cout << "visualizer start!" << std::endl;
    mMP3PlayerVisualization.setMP3FileName(mMP3SourceFile);
    run();
}

void ImGuiVis::run()
{
    // Initialize the glfw library.
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        std::cout << "Error in initializing glfw" << std::endl;
    }

    // Initialize the imGui
    auto window = initFcn();

    mMP3PlayerVisualization.worldInitFcn();

    // Setting up the text’s font.
    getFontIcon();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        newFrameImGui();
        ImGui::CreateContext();
        ImPlot::CreateContext();

        // Docking
        constexpr ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        _dockSpaceId = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), dockSpaceFlags);

        mMP3PlayerVisualization.worldFramePreDisplayFcn(true);

        // For convenience, get a reference to the I/O information
        const ImGuiIO& io = ImGui::GetIO();

        // Prepare the world frame
        glViewport(0, 0, mWorldWindowWidth, mWorldWindowHeight);
        glfwSetWindowSize(mWorldWindow, mWorldWindowWidth, mWorldWindowHeight);
        UTILITY::Color3f_t backgroundColor = UTILITYColors::VLightGray;
        glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        mMP3PlayerVisualization.localFrameDisplayFcn();


        // Transfer the pixels of the hidden context to the world texture object
        glBindTexture(GL_TEXTURE_2D, mWorldTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWorldWindowWidth, mWorldWindowHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, mWorldWindowWidth, mWorldWindowHeight);
        glBindTexture(GL_TEXTURE_2D, 0);

        renderImGui();

        if (mPauseRequested)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        glfwSwapBuffers(window);

        if (mMP3PlayerVisualization.quitRequested())
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    cleanUp();
    Distroy(window);
}

void ImGuiVis::glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void ImGuiVis::closeRequestCallback(GLFWwindow* window)
{
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

GLFWwindow* ImGuiVis::initFcn()
{
    // Create the world frame window
    mWorldWindow = createWindow(1480, 900, 100, 100, "Conan Imgui");

    // Add special callback to handle close request
    glfwSetWindowCloseCallback(mWorldWindow, closeRequestCallback);

    // Disable focus-on-show for this window since it is only shown for debug purposes
    glfwSetWindowAttrib(mWorldWindow, GLFW_FOCUS_ON_SHOW, GLFW_FALSE);

    // Hide the raw window by default to avoid the initial flicker
    hideWindow(mWorldWindow);

    // Create the ImGui context
    createImGuiContext(mWorldWindow, false);

    //// Create texture to store world display
    glGenTextures(1, &mWorldTexture);

    // Get the OpenGL version running on this machine
    std::array<char, 200> openGLVersion;

    sprintf(openGLVersion.data(), "%s", glGetString(GL_VERSION));
    mOpenGLVersion = std::string(openGLVersion.data());

    return mWorldWindow;
}

void ImGuiVis::getFontIcon()
{
    mFontIcon.lock();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    float baseFontSize = mBaseFontSize; // 13.0f is the size of the default font. Change to the font size you use.
    float iconFontSize =
        baseFontSize * 2.0f /
        3.0f; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly

    // merge in icons from Font Awesome
    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
    ImFontConfig         icons_config;
    icons_config.MergeMode        = true;
    icons_config.PixelSnapH       = true;
    icons_config.GlyphMinAdvanceX = iconFontSize;
    io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FAS, iconFontSize, &icons_config, icons_ranges);
    // use FONT_ICON_FILE_NAME_FAR if you want regular instead of solid
    mFontIcon.unlock();
}

ImGuiVis::~ImGuiVis()
{
    std::cout << "  visualizer stopped!" << std::endl;
}

void ImGuiVis::cleanUp()
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    ImPlot::DestroyContext();
}

void ImGuiVis::Distroy(GLFWwindow* window)
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

std::string ImGuiVis::displayTime()
{
    auto        today = std::chrono::system_clock::now();
    auto        ptr   = std::chrono::system_clock::to_time_t(today);
    std::string str(ctime(&ptr));
    return str;
}

void ImGuiVis::toLower(std::string& data)
{
    std::transform(data.begin(), data.end(), data.begin(), [](unsigned char c) { return std::tolower(c); });
}

std::filesystem::path ImGuiVis::getExecutableDir() const
{
    std::array<char, MAX_PATH> pathBuf{};
    DWORD len = GetModuleFileNameA(nullptr, pathBuf.data(), static_cast<DWORD>(pathBuf.size()));
    std::filesystem::path exePath(std::string(pathBuf.data(), len));
    return exePath.parent_path();
}
