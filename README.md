# SkitzoJam — 8D Audio Effect

**Real-time 8D audio for Windows**

SkitzoJam captures your system audio via WASAPI loopback, applies a smooth rotating stereo effect (the classic "8D" sound), and plays it out through a different audio device — all with low latency and live controls.

---

## Features

- **True system-wide 8D audio** — works with any application (Spotify, YouTube, games, Discord, etc.)
- **WASAPI loopback capture** — no virtual cables or extra software required
- **Live controls**
  - Rotation speed (LFO frequency)
  - Pan depth
  - Quiet-side floor (minimum gain)
  - Randomization (organic variation in speed & depth)
- **System tray support** — closes/minimizes to tray so the effect keeps running
- **Run at Windows startup** option
- **Remembers your settings** and last-used devices

---

## Usage

Launch the app.
Select an Input (capture) device — this is the device whose audio you want to process (usually your main speakers/headphones).
Select a different Output (render) device — this is where the 8D-processed audio will be played.
Adjust the sliders to taste.
The effect starts automatically once valid devices are selected.
Important: Input and output must be different devices. Using the same device for both is blocked to prevent feedback loops.Close the window or minimize it and the app continues running in the system tray. Use the tray icon → Exit to fully quit.

---

## Requirements

- Windows 10 / 11
- Qt 6 (Widgets module)
- CMake 3.16+
- Visual Studio 2019+ or another C++17 compiler (MSVC recommended)

---

## Building

```bash
git clone https://github.com/sarkikarl/skitzojam.git
cd skitzojam

# Configure (adjust Qt path if needed)
cmake -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2019_64"

# Build
cmake --build build --config Release


