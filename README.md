# Conan-ImGui-MP3Player
Interactive MP3 player built with ImGui/GLFW/OpenGL on Windows. Audio is decoded via Windows Media/ACM APIs and rendered with ImGui widgets and a PCM-derived waveform.

## Features
- Playlist: add paths manually, select, and jump prev/next.
- Transport: play, pause/resume, stop, and seek slider tied to playback position.
- Volume and stereo balance controls.
- Metadata display (title/artist/album/bitrate when present).
- Waveform preview computed from decoded PCM.

## Prerequisites (Windows)
- Visual Studio 2022 with C++ toolset
- CMake (>=3.28)
- Conan 2

## Build and Run
1) Install and configure dependencies via Conan + CMake:
```
run_debug.bat
```
   - If the legacy `build` folder is locked, use a fresh dir:
```
conan install . --output-folder=build_clean --build=missing --settings=build_type=Debug
cmake -S . -B build_clean -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=build_clean/build/generators/conan_toolchain.cmake
cmake --build build_clean --config Debug
```
2) Copy runtime assets next to the EXE (if not already placed):
```
copy build_clean\*.ttf build_clean\Debug\
copy test\Oryza.mp3 build_clean\Debug\
```
3) Run:
```
build_clean\Debug\mp3player.exe
```
If `run_debug.bat` is required, ensure no stale `build/build/generators` folder remains from prior runs; otherwise, use a fresh build directory as above.

## Usage
- Add track: type a path in the “Enter MP3 path” box and click “Add to Playlist”.
  - Relative paths are resolved against the EXE directory, its parents, and the repo/test folder.
- Select a track from the playlist; press Play. Use Pause/Resume, Stop, and Next/Previous.
- Drag the Seek slider to jump within the track (restarts playback from that position).
- Adjust Volume (0–1) and Balance (-1 left to +1 right).
- Metadata and waveform appear when a track is successfully loaded.
- Status messages show load errors (path attempted and HRESULT-derived failures).

## Troubleshooting
- “Failed to load file”: ensure the file exists; if using relative paths, place the MP3 next to the EXE or provide an absolute path.
- Font or asset issues: run the copy commands above to ensure `.ttf` files and `Oryza.mp3` sit beside `mp3player.exe`.
- Locked old build dirs: delete or ignore the legacy `build/build/generators` if it blocks `run_debug.bat`; you can use a fresh `build_clean` folder as shown.
