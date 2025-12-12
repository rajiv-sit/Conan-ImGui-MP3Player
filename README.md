# Conan-ImGui-MP3Player
Modern ImGui-based MP3 player that decodes via Windows Media/ACM, renders transport controls, EQ, metadata, and a cached waveform preview drawn with OpenGL/ImGui.

<img width="417" height="820" alt="image" src="https://github.com/user-attachments/assets/265d4983-71ae-4600-95bb-7694af6f807b" />

## Highlights
- **Modern layout**: two-column UI with playlist, playback controls, metadata, waveform, and EQ sliders laid out with subtle rounding and spacing.
- **Playback control**: play/pause/resume, stop, seek slider, balance, and volume are all tied to the native waveOut playback.
- **Waveform with playhead**: PCM-derived preview stored when playback starts, rendered in orange with a red hover line that reflects the current position.
- **EQ state**: slider-driven gains stored in `MP3Player` (future DSP hookup) to keep UI responsive without audio glitches.
- **Flexible track loading**: paths resolved against the executable directory, repo root, and provided `test/` folder.

## Build & Run (Windows)
1. Open a PowerShell shell in the repo root and run:
   ```
   run_debug.bat
   ```
   That script:
   - installs dependencies via Conan,
   - configures and builds in `build_debug_modern`,
   - copies fonts from `assets/visualizer/webfonts` and `test/Oryza.mp3` into the debug folder,
   - launches `Debug\mp3player.exe`.
2. If the script fails because `build_debug_modern/build/generators` was left locked from a previous run, rerun manually:
   ```
   conan install . --output-folder=build_debug_modern --build=missing --settings=build_type=Debug
   cmake -S . -B build_debug_modern -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=build_debug_modern/build/generators/conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
   cmake --build build_debug_modern --config Debug
   ```
   Afterward, run the EXE:
   ```
   build_debug_modern\Debug\mp3player.exe
   ```

## Workflow / Usage
- **Add files**: paste a path into the `Enter MP3 path` field and click `Add to Playlist`. Relative paths are resolved around the EXE and repo.
- **Playback**: select an entry, hit `Play`, and the waveform loads on demand. The Seek bar and playhead remain synchronized via the native position query.
- **Volume / balance**: sliders immediately update `waveOut` volume. EQ sliders store gain values without touching the core buffer.
- **Navigator**: use `Previous` / `Next` buttons to stroll through the playlist; the waveform and metadata refresh each time.
- **Status feedback**: errors show file-not-found, load failures, and waveform availability tips (visible while playing).

## Design Notes
- **Waveform cache**: prevents re-decoding while the track plays and guards the plot with `isPlaying()` so the visual shimmer only shows during active playback.
- **Orange waveform + red playhead**: `ImGui::PlotLines` uses the available width to draw horizontal data, and a draw-list line marks progress.
- **EQ alignment**: six sliders are arranged vertically with space, and changes are stored in `MP3Player` for future DSP integration.

## Troubleshooting
- **Waveform missing**: ensure playback is running; the plot is intentionally gated to only show while the audio loop is active.
- **File loading failure**: check the resolved path printed in status, and confirm the MP3 exists relative to the exe.
- **Locked generators folder**: delete or rebuild with a new folder name before rerunning `run_debug.bat`.

