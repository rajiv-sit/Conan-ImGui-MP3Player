#pragma once

#include "mp3/MP3Player.h"
#include "UTILITYMath.h"
#include "VisualizationBase.h"
#include <vector>
#include <string>
#include <filesystem>

namespace Player
{

	enum Modes_e
	{
		STOP = 0,
		PLAY = 1,
		PAUSE = 2,
		FWD = 3,
		RWD = 4
	};

	// display options
	struct WorldFrameSettings_t
	{
		bool displayFile;
		bool play;
		bool pause;
		bool stop;
		bool fwd;
		float fwdValue;
		bool rwd;
		float volume;
		WorldFrameSettings_t()
			: displayFile(true)
			, play(false)
			, pause(false)
			, stop(false)
			, fwd(false)
			, fwdValue(0.0F)
			, rwd(false)	
			, volume(255.0F)
		{
		}
	};

	class MP3Visualization : public VisualizationBase
	{

	public:
		// Constructor
		MP3Visualization();

		// Destructor
		~MP3Visualization();

		static MP3Visualization& getInstance();

		void initialize();

		// Set the worldvis functions
		WorldFrameSettings_t mWorldFrameSettings;
		void                 worldReadIniSettings() override;
		void                 worldInitFcn() override;
		void                 worldFramePreDisplayFcn(bool demoMode) override;
		void                 localFrameDisplayFcn() override;

		// Data mutex to be used whenever reading/writing to data members
		std::mutex mVisionLock;

		std::string getTime();
		std::string getDayAndTime();

		void setMP3FileName(std::string& str);
		std::string getMP3FileName();

		MP3Player mAudioPlayer;
		std::string mMP3FileName;
		bool mPlayStatus;
		bool mPauseStatus;
		unsigned long mVolumeLevel;
		bool mVisualFrameStatus;

		std::vector<std::string> mPlaylist;
		int                      mCurrentIndex;
		float                    mVolumeNormalized;
		float                    mBalance;
		float                    mSeekSeconds;
		bool                     mUserSeeking;
		std::string              mStatusMessage;

		char mFileInputBuffer[512];

		bool loadCurrentTrack();
		std::filesystem::path getExecutableDir() const;
		void playSelected(double startSeconds = 0.0);
		void moveToTrack(int delta);
		std::string wideToUtf8(const std::wstring& wstr);

		char* mBuffer;

	};

}





