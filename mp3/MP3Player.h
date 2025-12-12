#pragma once
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <string>
#include <vector>
#include <algorithm>
#include <mmreg.h>
#include <msacm.h>
#include <wmsdk.h>

#pragma comment(lib, "msacm32.lib") 
#pragma comment(lib, "wmvcore.lib") 
#pragma comment(lib, "winmm.lib") 
#pragma intrinsic(memset,memcpy,memcmp)

#ifdef _DEBUG
#define mp3Assert(function) assert((function) == 0)
#else
#define mp3Assert(function) (function)
#endif

/// @brief       This is a MP3Player class: play, stop, pause, rewind and fwd music
///              Initally written by Alexandre Mutel and modifed by Rajiv Sit
class MP3Player
{
public:
	struct Metadata
	{
		std::wstring title;
		std::wstring artist;
		std::wstring album;
		DWORD        bitrate = 0;
	};

private:
	/// declaring variables
	HWAVEOUT     mHandleWaveOut = nullptr;
	DWORD        mBufferLength  = 0;
	double       mDurationInSecond = 0.0;
	double       mStartOffsetSeconds = 0.0;
	BYTE*        mSoundBuffer = nullptr;
	WAVEHDR      mWaveHeader{};
	WAVEFORMATEX mPcmFormat{};
	bool         mHeaderPrepared = false;
	bool         mIsOpen = false;
	bool         mIsPlaying = false;
	bool         mIsPaused = false;
	Metadata     mMetadata;
	std::vector<float> mEqGainsDb;

	/// helper to clear playback state
	void resetWaveOut()
	{
		if (mHandleWaveOut)
		{
			waveOutReset(mHandleWaveOut);
			if (mHeaderPrepared)
			{
				waveOutUnprepareHeader(mHandleWaveOut, &mWaveHeader, sizeof(mWaveHeader));
				mHeaderPrepared = false;
			}
			waveOutClose(mHandleWaveOut);
			mHandleWaveOut = nullptr;
		}
		mIsPlaying = false;
		mIsPaused  = false;
	}

	static std::wstring readHeaderString(IWMHeaderInfo* info, const WCHAR* key)
	{
		WORD             streamNum    = 0;
		WMT_ATTR_DATATYPE attrType    = WMT_TYPE_STRING;
		WORD             length       = 0;
		if (FAILED(info->GetAttributeByName(&streamNum, key, &attrType, nullptr, &length)) || length == 0)
		{
			return L"";
		}

		std::vector<WCHAR> buffer(length / sizeof(WCHAR) + 1, 0);
		if (FAILED(info->GetAttributeByName(&streamNum, key, &attrType, reinterpret_cast<BYTE*>(buffer.data()), &length)))
		{
			return L"";
		}
		return std::wstring(buffer.data());
	}

	static DWORD readHeaderDword(IWMHeaderInfo* info, const WCHAR* key)
	{
		WORD             streamNum    = 0;
		WMT_ATTR_DATATYPE attrType    = WMT_TYPE_DWORD;
		WORD             length       = sizeof(DWORD);
		DWORD            value        = 0;
		if (FAILED(info->GetAttributeByName(&streamNum, key, &attrType, reinterpret_cast<BYTE*>(&value), &length)))
		{
			return 0;
		}
		return value;
	}

public:
	MP3Player()  = default;
	~MP3Player() { close(); }

	/// @brief       loads a MP3 file (UTF-16 path) and convert it internally to a PCM format, ready for sound playback.
	HRESULT openFromFile(const wchar_t* inputFileName)
	{
		close();

		// Open the mp3 file
		HANDLE hFile = CreateFileW(inputFileName,
			GENERIC_READ,          // desired access
			FILE_SHARE_READ,       // share for reading
			NULL,                  // no security
			OPEN_EXISTING,         // existing file only
			FILE_ATTRIBUTE_NORMAL, // normal file
			NULL);                 // no attr
		if (hFile == INVALID_HANDLE_VALUE)
		{
			return HRESULT_FROM_WIN32(GetLastError());
		}

		// Get FileSize
		DWORD fileSize = GetFileSize(hFile, NULL);
		if (fileSize == INVALID_FILE_SIZE)
		{
			CloseHandle(hFile);
			return HRESULT_FROM_WIN32(GetLastError());
		}

		// Alloc buffer for file
		BYTE* mp3Buffer = (BYTE*)LocalAlloc(LPTR, fileSize);
		if (!mp3Buffer)
		{
			CloseHandle(hFile);
			return E_OUTOFMEMORY;
		}

		// Read file and fill mp3Buffer
		DWORD bytesRead;
		DWORD resultReadFile = ReadFile(hFile, mp3Buffer, fileSize, &bytesRead, NULL);
		if (resultReadFile == 0 || bytesRead != fileSize)
		{
			CloseHandle(hFile);
			LocalFree(mp3Buffer);
			return HRESULT_FROM_WIN32(GetLastError());
		}

		// Close File
		CloseHandle(hFile);

		// Open and convert MP3
		HRESULT hr = openFromMemory(mp3Buffer, fileSize);

		// Free mp3Buffer
		LocalFree(mp3Buffer);

		return hr;
	}

	/// @brief       loads a MP3 file and convert it internaly to a PCM format, ready for sound playback.
	///
	/// @param [in]  name of the input file
	/// @param [out] handle
	HRESULT openFromFile(TCHAR* inputFileName)
	{
		// TCHAR may be narrow if UNICODE is not defined; route everything through wide API.
		std::wstring widePath;
		if constexpr (sizeof(TCHAR) == sizeof(wchar_t))
		{
			widePath.assign(reinterpret_cast<wchar_t*>(inputFileName));
		}
		else
		{
			int wideLen = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(inputFileName), -1, nullptr, 0);
			widePath.resize(static_cast<size_t>(wideLen));
			MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(inputFileName), -1, widePath.data(), wideLen);
		}
		return openFromFile(widePath.c_str());
	}

	/// @brief       loads a MP3 from memory and convert it internaly to a PCM format, ready for sound playback.
	///
	/// @param [in]  mp3 input buffer
	/// @param [in]  size of the mp3 inpput buffer
	/// @param [out] handle results
	HRESULT openFromMemory(BYTE* mp3InputBuffer, DWORD mp3InputBufferSize) {
		IWMSyncReader* wmSyncReader;
		IWMHeaderInfo* wmHeaderInfo;
		IWMProfile* wmProfile;
		IWMStreamConfig* wmStreamConfig;
		IWMMediaProps* wmMediaProperties;
		WORD wmStreamNum = 0;
		WMT_ATTR_DATATYPE wmAttrDataType;
		DWORD durationInSecondInt;
		QWORD durationInNano;
		DWORD sizeMediaType;
		DWORD maxFormatSize = 0;
		HACMSTREAM acmMp3stream = NULL;
		HGLOBAL mp3HGlobal;
		IStream* mp3Stream;

		// Define output format
		mPcmFormat = {
		 WAVE_FORMAT_PCM, //format type
		 2,               // number of channels (i.e. mono, stereo...)
		 44100,           // sample rate
		 4 * 44100,       // for buffer estimation 
		 4,               // block size of data
		 16,              // number of bits per sample of mono data 
		 0,               // the count in bytes of the size of 
		};

		const DWORD MP3_BLOCK_SIZE = 522;

		// Define input format
		static MPEGLAYER3WAVEFORMAT mp3Format = {
		 {
		  WAVE_FORMAT_MPEGLAYER3,       // format type 
		   2,                           // number of channels (i.e. mono, stereo...) 
		   44100,                       // sample rate 
		   128 * (1024 / 8),            // average bytes per sec not really used but must be one of 64, 96, 112, 128, 160kbps
		   1,                           // block size of data 
		   0,                           // number of bits per sample of mono data 
		   MPEGLAYER3_WFX_EXTRA_BYTES,  // cbSize       
		 },
		 MPEGLAYER3_ID_MPEG,            // wID
		 MPEGLAYER3_FLAG_PADDING_OFF,   //fdwFlags
		 MP3_BLOCK_SIZE,                // nBlockSize
		 1,                             // nFramesPerBlock
		 1393,                          // nCodecDelay;
		};

		// -----------------------------------------------------------------------------------
		// Extract and verify mp3 info : duration, type = mp3, sampleRate = 44100, channels = 2
		// -----------------------------------------------------------------------------------

		// Initialize COM
		CoInitialize(0);

		// Create SyncReader
		mp3Assert(WMCreateSyncReader(NULL, WMT_RIGHT_PLAYBACK, &wmSyncReader));

		// Alloc With global and create IStream
		mp3HGlobal = GlobalAlloc(GPTR, mp3InputBufferSize);
		assert(mp3HGlobal != 0);
		void* mp3HGlobalBuffer = GlobalLock(mp3HGlobal);
		memcpy(mp3HGlobalBuffer, mp3InputBuffer, mp3InputBufferSize);
		GlobalUnlock(mp3HGlobal);
		mp3Assert(CreateStreamOnHGlobal(mp3HGlobal, FALSE, &mp3Stream));

		// Open MP3 Stream
		mp3Assert(wmSyncReader->OpenStream(mp3Stream));

		// Get HeaderInfo interface
		mp3Assert(wmSyncReader->QueryInterface(&wmHeaderInfo));

		// Retrieve mp3 song duration in seconds
		WORD lengthDataType = sizeof(QWORD);
		mp3Assert(wmHeaderInfo->GetAttributeByName(&wmStreamNum, L"Duration", &wmAttrDataType, (BYTE*)&durationInNano, &lengthDataType));
		mDurationInSecond = ((double)durationInNano) / 10000000.0;
		durationInSecondInt = (int)(durationInNano / 10000000) + 1;

		// Sequence of call to get the MediaType
		// WAVEFORMATEX for mp3 can then be extract from MediaType
		mp3Assert(wmSyncReader->QueryInterface(&wmProfile));
		mp3Assert(wmProfile->GetStream(0, &wmStreamConfig));
		mp3Assert(wmStreamConfig->QueryInterface(&wmMediaProperties));

		// Retrieve sizeof MediaType
		mp3Assert(wmMediaProperties->GetMediaType(NULL, &sizeMediaType));

		// Retrieve MediaType
		WM_MEDIA_TYPE* mediaType = (WM_MEDIA_TYPE*)LocalAlloc(LPTR, sizeMediaType);
		mp3Assert(wmMediaProperties->GetMediaType(mediaType, &sizeMediaType));

		// Check that MediaType is audio
		assert(mediaType->majortype == WMMEDIATYPE_Audio);

		// Check that input is mp3
		WAVEFORMATEX* inputFormat = (WAVEFORMATEX*)mediaType->pbFormat;
		assert(inputFormat->wFormatTag == WAVE_FORMAT_MPEGLAYER3);
		assert(inputFormat->nSamplesPerSec == 44100);
		assert(inputFormat->nChannels == 2);

		// Capture metadata (optional fields)
		mMetadata.title  = readHeaderString(wmHeaderInfo, L"Title");
		mMetadata.artist = readHeaderString(wmHeaderInfo, L"Author");
		mMetadata.album  = readHeaderString(wmHeaderInfo, L"WM/AlbumTitle");
		mMetadata.bitrate = readHeaderDword(wmHeaderInfo, L"Bitrate");

		// Release COM interface
		wmMediaProperties->Release();
		wmStreamConfig->Release();
		wmProfile->Release();
		wmHeaderInfo->Release();
		wmSyncReader->Release();

		// Free allocated mem
		LocalFree(mediaType);

		// -----------------------------------------------------------------------------------
		// Convert mp3 to pcm using acm driver
		// The following code is mainly inspired from http://david.weekly.org/code/mp3acm.html
		// -----------------------------------------------------------------------------------

		// Get maximum FormatSize for all acm
		mp3Assert(acmMetrics(NULL, ACM_METRIC_MAX_SIZE_FORMAT, &maxFormatSize));

		// Allocate PCM output sound buffer
		mBufferLength = mDurationInSecond * mPcmFormat.nAvgBytesPerSec;
		mSoundBuffer = (BYTE*)LocalAlloc(LPTR, mBufferLength);

		acmMp3stream = NULL;
		switch (acmStreamOpen(&acmMp3stream,    // Open an ACM conversion stream
			NULL,                       // Query all ACM drivers
			(LPWAVEFORMATEX)&mp3Format, // input format :  mp3
			&mPcmFormat,                // output format : pcm
			NULL,                       // No filters
			0,                          // No async callback
			0,                          // No data for callback
			0                           // No flags
		)
			) {
		case MMSYSERR_NOERROR:
			break; // success!
		case MMSYSERR_INVALPARAM:
			assert(!"Invalid parameters passed to acmStreamOpen");
			return E_FAIL;
		case ACMERR_NOTPOSSIBLE:
			assert(!"No ACM filter found capable of decoding MP3");
			return E_FAIL;
		default:
			assert(!"Some error opening ACM decoding stream!");
			return E_FAIL;
		}

		// Determine output decompressed buffer size
		unsigned long rawbufsize = 0;
		mp3Assert(acmStreamSize(acmMp3stream, MP3_BLOCK_SIZE, &rawbufsize, ACM_STREAMSIZEF_SOURCE));
		assert(rawbufsize > 0);

		// allocate our I/O buffers
		static BYTE mp3BlockBuffer[MP3_BLOCK_SIZE];
		LPBYTE rawbuf = (LPBYTE)LocalAlloc(LPTR, rawbufsize);

		// prepare the decoder
		static ACMSTREAMHEADER mp3streamHead;
		mp3streamHead.cbStruct = sizeof(ACMSTREAMHEADER);
		mp3streamHead.pbSrc = mp3BlockBuffer;
		mp3streamHead.cbSrcLength = MP3_BLOCK_SIZE;
		mp3streamHead.pbDst = rawbuf;
		mp3streamHead.cbDstLength = rawbufsize;
		mp3Assert(acmStreamPrepareHeader(acmMp3stream, &mp3streamHead, 0));

		BYTE* currentOutput = mSoundBuffer;
		DWORD totalDecompressedSize = 0;

		static ULARGE_INTEGER newPosition;
		static LARGE_INTEGER seekValue;
		mp3Assert(mp3Stream->Seek(seekValue, STREAM_SEEK_SET, &newPosition));

		while (1) {
			// suck in some MP3 data
			ULONG count;
			mp3Assert(mp3Stream->Read(mp3BlockBuffer, MP3_BLOCK_SIZE, &count));
			if (count != MP3_BLOCK_SIZE)
				break;

			// convert the data
			mp3Assert(acmStreamConvert(acmMp3stream, &mp3streamHead, ACM_STREAMCONVERTF_BLOCKALIGN));

			// write the decoded PCM to disk
			memcpy(currentOutput, rawbuf, mp3streamHead.cbDstLengthUsed);
			totalDecompressedSize += mp3streamHead.cbDstLengthUsed;
			currentOutput += mp3streamHead.cbDstLengthUsed;
		};

		mp3Assert(acmStreamUnprepareHeader(acmMp3stream, &mp3streamHead, 0));
		LocalFree(rawbuf);
		mp3Assert(acmStreamClose(acmMp3stream, 0));

		// Release allocated memory
		mp3Stream->Release();
		GlobalFree(mp3HGlobal);
		mIsOpen             = true;
		mIsPlaying          = false;
		mIsPaused           = false;
		mStartOffsetSeconds = 0.0;
		return S_OK;
	}

	/// @brief       start playback from a specific time (seconds)
	HRESULT play(double startSeconds = 0.0)
	{
		if (!mSoundBuffer || !mIsOpen)
		{
			return E_FAIL;
		}

		resetWaveOut();

		const double clampedSeconds = std::clamp(startSeconds, 0.0, mDurationInSecond);
		DWORD startByte             = static_cast<DWORD>(clampedSeconds * mPcmFormat.nAvgBytesPerSec);
		if (startByte >= mBufferLength)
		{
			startByte = mBufferLength - mPcmFormat.nBlockAlign;
		}

		mp3Assert(waveOutOpen(&mHandleWaveOut, WAVE_MAPPER, &mPcmFormat, NULL, 0, CALLBACK_NULL));

		mWaveHeader               = {};
		mWaveHeader.lpData        = (LPSTR)(mSoundBuffer + startByte);
		mWaveHeader.dwBufferLength = mBufferLength - startByte;

		mp3Assert(waveOutPrepareHeader(mHandleWaveOut, &mWaveHeader, sizeof(mWaveHeader)));
		mHeaderPrepared = true;
		mp3Assert(waveOutWrite(mHandleWaveOut, &mWaveHeader, sizeof(mWaveHeader)));

		mStartOffsetSeconds = startByte / static_cast<double>(mPcmFormat.nAvgBytesPerSec);
		mIsPlaying          = true;
		mIsPaused           = false;
		return S_OK;
	}

	/// @brief pause audio
	void __inline setPause()
	{
		if (mHandleWaveOut && mIsPlaying && !mIsPaused)
		{
			mp3Assert(waveOutPause(mHandleWaveOut));
			mIsPaused = true;
		}
	}

	/// @brief resume audio
	void __inline unSetPause()
	{
		if (mHandleWaveOut && mIsPlaying && mIsPaused)
		{
			mp3Assert(waveOutRestart(mHandleWaveOut));
			mIsPaused = false;
		}
	}

	/// @brief stop playback without releasing decoded audio
	void stop()
	{
		resetWaveOut();
		mStartOffsetSeconds = 0.0;
	}

	/// @brief adjust volume with balance (-1 left, 0 center, 1 right)
	void setVolume(float master, float balance = 0.0F)
	{
		const float clampedMaster  = std::clamp(master, 0.0F, 1.0F);
		const float clampedBalance = std::clamp(balance, -1.0F, 1.0F);

		float leftGain  = clampedMaster;
		float rightGain = clampedMaster;

		if (clampedBalance < 0.0F)
		{
			rightGain *= 1.0F + clampedBalance;
		}
		else if (clampedBalance > 0.0F)
		{
			leftGain *= 1.0F - clampedBalance;
		}

		const DWORD leftValue  = static_cast<DWORD>(leftGain * 0xFFFF);
		const DWORD rightValue = static_cast<DWORD>(rightGain * 0xFFFF);
		const DWORD value      = (rightValue << 16) | leftValue;

		HWAVEOUT outHandle = mHandleWaveOut ? mHandleWaveOut
			: reinterpret_cast<HWAVEOUT>(static_cast<UINT_PTR>(WAVE_MAPPER));
		waveOutSetVolume(outHandle, value);
	}

	/// @brief       close the current MP3Player, stop playback and free allocated memory
	void __inline close()
	{
		resetWaveOut();
		if (mSoundBuffer)
		{
			mp3Assert(LocalFree(mSoundBuffer));
			mSoundBuffer = nullptr;
		}
		mBufferLength      = 0;
		mDurationInSecond  = 0.0;
		mStartOffsetSeconds = 0.0;
		mIsOpen            = false;
	}

	MMRESULT getPitch(const HWAVEOUT& hwo, const LPDWORD& pdwPitch)
	{
		return waveOutGetPitch(hwo, pdwPitch);
	}

	/// @brief       get the total duration of audio 
	///
	/// @param [out] the music duration in seconds
	double __inline getDuration()
	{
		return mDurationInSecond;
	}

	/// @brief       get the current position from the playback
	///
	/// @param [out] current position from the sound playback (used from sync)
	double getPosition() {
		static MMTIME MMTime = { TIME_SAMPLES, 0 };
		if (mHandleWaveOut)
		{
			waveOutGetPosition(mHandleWaveOut, &MMTime, sizeof(MMTIME));
			const double played = ((double)MMTime.u.sample) / (44100.0);
			return (std::min)(mStartOffsetSeconds + played, mDurationInSecond);
		}
		return mStartOffsetSeconds;
	}

	bool isOpen() const { return mIsOpen; }
	bool isPlaying() const { return mIsPlaying; }
	bool isPaused() const { return mIsPaused; }

	const Metadata& getMetadata() const { return mMetadata; }

	/// @brief Placeholder for future DSP: store requested EQ gains (dB)
	void setEqualizerGains(const std::vector<float>& gainsDb)
	{
		mEqGainsDb = gainsDb;
	}

	/// @brief Extract a small waveform preview from the decoded PCM buffer
	std::vector<float> getWaveformPreview(size_t sampleCount = 256) const
	{
		std::vector<float> preview;
		if (!mSoundBuffer || mBufferLength == 0 || sampleCount == 0)
		{
			return preview;
		}

		const size_t bytesPerSample = mPcmFormat.wBitsPerSample / 8;
		const size_t frameSize      = bytesPerSample * mPcmFormat.nChannels;
		if (frameSize == 0)
		{
			return preview;
		}

		const size_t totalFrames = mBufferLength / frameSize;
		const size_t step        = std::max<size_t>(1, totalFrames / sampleCount);
		preview.reserve(sampleCount);

		const int16_t* samples = reinterpret_cast<const int16_t*>(mSoundBuffer);
		for (size_t frame = 0; frame < totalFrames && preview.size() < sampleCount; frame += step)
		{
			const size_t idx   = frame * mPcmFormat.nChannels;
			const int16_t left = samples[idx];
			const int16_t right = (mPcmFormat.nChannels > 1) ? samples[idx + 1] : left;
			const float   avg  = static_cast<float>((left + right) / 2.0f / 32768.0f);
			preview.push_back(avg);
		}
		return preview;
	}
};

#pragma function(memset, memcpy, memcmp)

