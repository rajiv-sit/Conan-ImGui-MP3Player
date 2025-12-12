#include "MP3Visualization.h"

#undef e
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <cfloat>
#include <filesystem>
#include <limits>
#include <windows.h>

// Helper to wire demo markers located in code to an interactive browser
typedef void (*ImGuiDemoMarkerCallback)(const char* file, int line, const char* section, void* user_data);
extern ImGuiDemoMarkerCallback      GImGuiDemoMarkerCallback;
extern void*                        GImGuiDemoMarkerCallbackUserData;
ImGuiDemoMarkerCallback             GImGuiDemoMarkerCallback = NULL;
void*                               GImGuiDemoMarkerCallbackUserData = NULL;
#define IMGUI_DEMO_MARKER(section)  \
    do                              \
    {                               \
        if (GImGuiDemoMarkerCallback != NULL)                                                    \
            GImGuiDemoMarkerCallback(__FILE__, __LINE__, section, GImGuiDemoMarkerCallbackUserData); \
    } while (0)


Player::MP3Visualization::MP3Visualization()
    : VisualizationBase()
    , mAudioPlayer()
    , mMP3FileName{}
    , mPlayStatus(false)
    , mPauseStatus(false)
    , mVolumeLevel(255)
    , mVisualFrameStatus(true)
    , mCurrentIndex(-1)
    , mVolumeNormalized(0.5F)
    , mBalance(0.0F)
    , mSeekSeconds(0.0F)
    , mUserSeeking(false)
    , mStatusMessage()
    , mQuitRequested(false)
    , mBuffer(new char[1000])
{
    memset(mFileInputBuffer, 0, sizeof(mFileInputBuffer));
    // Read in the INI settings
    worldReadIniSettings();
}

Player::MP3Visualization::~MP3Visualization()
{
}

Player::MP3Visualization& Player::MP3Visualization::getInstance()
{
    static MP3Visualization instance;
    return instance;
}

void Player::MP3Visualization::initialize()
{
}

void Player::MP3Visualization::setMP3FileName(std::string& str)
{
    mMP3FileName = str;
}

std::string Player::MP3Visualization::getMP3FileName()
{
    return mMP3FileName;
}

void Player::MP3Visualization::worldReadIniSettings()
{
}

void Player::MP3Visualization::worldInitFcn()
{
    if (!mMP3FileName.empty())
    {
        mPlaylist.push_back(mMP3FileName);
        mCurrentIndex = 0;
        loadCurrentTrack();
    }
}

// World frame drawing functions
void Player::MP3Visualization::worldFramePreDisplayFcn(bool demoMode)
{
    // Create a new window for MP3 player
    ImGui::Begin("Cross-Hair Player", &mVisualFrameStatus, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", "Ctrl+O")) { }
            if (ImGui::MenuItem("Close", "Ctrl+W")) { mVisualFrameStatus = false; mQuitRequested = true; }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) { mQuitRequested = true; }
        ImGui::EndMenuBar();
    }

    ImGui::TextColored(ImVec4(UTILITYColors::White.r, UTILITYColors::White.g, UTILITYColors::White.b, 1.0F), "Day:");
    ImGui::SameLine(100);
    ImGui::TextColored(ImVec4{UTILITYColors::Green.r, UTILITYColors::Green.g, UTILITYColors::Green.b, 1.0f}, getDayAndTime().c_str());

    ImGui::Spacing();
    ImGui::InputTextWithHint("##mp3input", "Enter MP3 path", mFileInputBuffer, IM_ARRAYSIZE(mFileInputBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Add to Playlist"))
    {
        if (strlen(mFileInputBuffer) > 0)
        {
            mPlaylist.emplace_back(mFileInputBuffer);
            if (mCurrentIndex < 0)
            {
                mCurrentIndex = 0;
                loadCurrentTrack();
            }
            memset(mFileInputBuffer, 0, sizeof(mFileInputBuffer));
        }
    }

    if (ImGui::BeginListBox("Playlist", ImVec2(-FLT_MIN, 130)))
    {
        for (int idx = 0; idx < static_cast<int>(mPlaylist.size()); ++idx)
        {
            const bool selected = idx == mCurrentIndex;
            if (ImGui::Selectable(mPlaylist[idx].c_str(), selected))
            {
                mCurrentIndex = idx;
                loadCurrentTrack();
            }
        }
        ImGui::EndListBox();
    }

    if (ImGui::Button("Previous"))
    {
        moveToTrack(-1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Play"))
    {
        playSelected(mUserSeeking ? mSeekSeconds : 0.0);
    }
    ImGui::SameLine();
    if (ImGui::Button(mAudioPlayer.isPaused() ? "Resume" : "Pause"))
    {
        if (mAudioPlayer.isPaused())
        {
            mAudioPlayer.unSetPause();
        }
        else
        {
            mAudioPlayer.setPause();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        mAudioPlayer.stop();
        mSeekSeconds = 0.0F;
    }
    ImGui::SameLine();
    if (ImGui::Button("Next"))
    {
        moveToTrack(1);
    }

    if (mWorldFrameSettings.displayFile)
    {
        const float duration = static_cast<float>(mAudioPlayer.getDuration());
        if (duration > 0.0F)
        {
            if (!mUserSeeking)
            {
                mSeekSeconds = static_cast<float>(mAudioPlayer.getPosition());
            }
            ImGui::SliderFloat("Seek", &mSeekSeconds, 0.0f, duration, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemActivated())
            {
                mUserSeeking = true;
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                playSelected(mSeekSeconds);
                mUserSeeking = false;
            }
        }

        ImGui::SliderFloat("Volume", &mVolumeNormalized, 0.0F, 1.0F);
        ImGui::SliderFloat("Balance", &mBalance, -1.0F, 1.0F);
        const auto& meta = mAudioPlayer.getMetadata();
        ImGui::Text("Title: %s", wideToUtf8(meta.title).c_str());
        ImGui::Text("Artist: %s", wideToUtf8(meta.artist).c_str());
        ImGui::Text("Album: %s", wideToUtf8(meta.album).c_str());
        if (meta.bitrate > 0)
        {
            ImGui::Text("Bitrate: %u kbps", meta.bitrate / 1000);
        }

        auto waveform = mAudioPlayer.getWaveformPreview(256);
        if (!waveform.empty())
        {
            ImGui::PlotLines("Waveform", waveform.data(), static_cast<int>(waveform.size()), 0, nullptr, -1.0F, 1.0F, ImVec2(0, 90));
        }
        else
        {
            ImGui::TextDisabled("Waveform unavailable. Load a track to view.");
        }

        if (!mStatusMessage.empty())
        {
            ImGui::TextColored(ImVec4(UTILITYColors::Orange.r, UTILITYColors::Orange.g, UTILITYColors::Orange.b, 1.0F),
                               "%s",
                               mStatusMessage.c_str());
        }
    }
    ImGui::End();
}

void Player::MP3Visualization::localFrameDisplayFcn()
{
    if (mWorldFrameSettings.displayFile)
    {
        if (!mUserSeeking)
        {
            mSeekSeconds = static_cast<float>(mAudioPlayer.getPosition());
        }
        mAudioPlayer.setVolume(mVolumeNormalized, mBalance);
    }
}

std::string Player::MP3Visualization::getTime()
{
    auto        today = std::chrono::system_clock::now();
    auto        ptr   = std::chrono::system_clock::to_time_t(today);
    auto        tm    = localtime(&ptr);
    std::string strLocalTime =
        std::to_string(tm->tm_hour) + ":" + std::to_string(tm->tm_min) + ":" + std::to_string(tm->tm_sec);
    return strLocalTime;
}

std::string Player::MP3Visualization::getDayAndTime()
{
    auto        today = std::chrono::system_clock::now();
    auto        ptr   = std::chrono::system_clock::to_time_t(today);
    std::string strDayAndTime(ctime(&ptr));
    return strDayAndTime;
}

bool Player::MP3Visualization::loadCurrentTrack()
{
    if (mCurrentIndex < 0 || mCurrentIndex >= static_cast<int>(mPlaylist.size()))
    {
        mStatusMessage = "No track selected.";
        return false;
    }

    const std::string& source = mPlaylist[mCurrentIndex];
    std::filesystem::path requested(source);

    // Build candidate search paths for relative inputs
    std::vector<std::filesystem::path> candidates;
    if (requested.is_absolute())
    {
        candidates.push_back(requested);
    }
    else
    {
        candidates.push_back(std::filesystem::current_path() / requested);
        auto exeDir = getExecutableDir();
        candidates.push_back(exeDir / requested);
        candidates.push_back(exeDir.parent_path() / requested);       // build_clean/
        candidates.push_back(exeDir.parent_path().parent_path() / requested); // repo root
        candidates.push_back(exeDir.parent_path().parent_path() / "test" / requested);
    }

    std::filesystem::path resolved;
    for (const auto& candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
        {
            resolved = candidate;
            break;
        }
    }

    if (resolved.empty())
    {
        mStatusMessage = "File not found: " + source;
        return false;
    }

    const std::string resolvedStr = resolved.string();

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, resolvedStr.c_str(), -1, nullptr, 0);
    std::wstring wide;
    wide.resize(static_cast<size_t>(wideLen));
    MultiByteToWideChar(CP_UTF8, 0, resolvedStr.c_str(), -1, wide.data(), wideLen);

    HRESULT hr = mAudioPlayer.openFromFile(wide.c_str());
    if (FAILED(hr))
    {
        mStatusMessage = "Failed to load file: " + resolvedStr;
        return false;
    }

    mMP3FileName  = resolvedStr;
    mStatusMessage.clear();
    return true;
}

void Player::MP3Visualization::playSelected(double startSeconds)
{
    if (!mAudioPlayer.isOpen())
    {
        if (!loadCurrentTrack())
        {
            return;
        }
    }
    mAudioPlayer.play(startSeconds);
}

void Player::MP3Visualization::moveToTrack(int delta)
{
    if (mPlaylist.empty())
    {
        mStatusMessage = "Playlist is empty.";
        return;
    }
    mCurrentIndex += delta;
    if (mCurrentIndex < 0)
        mCurrentIndex = static_cast<int>(mPlaylist.size()) - 1;
    if (mCurrentIndex >= static_cast<int>(mPlaylist.size()))
        mCurrentIndex = 0;
    if (loadCurrentTrack())
    {
        playSelected(0.0);
    }
}

std::string Player::MP3Visualization::wideToUtf8(const std::wstring& wstr)
{
    if (wstr.empty())
    {
        return "";
    }
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result;
    result.resize(static_cast<size_t>(sizeNeeded));
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, result.data(), sizeNeeded, nullptr, nullptr);
    if (!result.empty() && result.back() == '\0')
    {
        result.pop_back();
    }
    return result;
}

std::filesystem::path Player::MP3Visualization::getExecutableDir() const
{
    std::array<wchar_t, MAX_PATH> pathBuf{};
    DWORD len = GetModuleFileNameW(nullptr, pathBuf.data(), static_cast<DWORD>(pathBuf.size()));
    std::filesystem::path exePath(std::wstring(pathBuf.data(), len));
    return exePath.parent_path();
}
