# DubInstante — Technical Documentation

DubInstante is a professional video dubbing studio built with **Qt 6 / C++17**. It allows dubbing artists to play a video, write text on a scrolling "rythmo band", record their voice in sync, and export the final result. This document is the onboarding guide — everything a new contributor needs to understand the codebase and start contributing.

---

## Table of Contents

1. [High-Level Overview](#high-level-overview)
2. [Project Structure](#project-structure)
3. [Architecture](#architecture)
4. [Core Layer — Business Logic](#core-layer--business-logic)
5. [GUI Layer — User Interface](#gui-layer--user-interface)
6. [Utils Layer](#utils-layer)
7. [Data Flow & Signals/Slots Wiring](#data-flow--signalsslots-wiring)
8. [Save System (`.dbi` format)](#save-system-dbi-format)
9. [Export Pipeline](#export-pipeline)
10. [Build & Run](#build--run)
11. [External Dependencies](#external-dependencies)
12. [Keyboard Shortcuts](#keyboard-shortcuts)
13. [Coding Conventions](#coding-conventions)
14. [Roadmap](#roadmap)

---

## High-Level Overview

The app workflow is:

1. **Open** a video file (MP4, MKV, etc.)
2. **Write** dubbing text on the rythmo band — the text scrolls in sync with the video
3. **Record** voice over the video (up to 2 independent tracks)
4. **Export** the final video with dubbed audio using FFmpeg

The entire application is a single-window Qt desktop app with no external frameworks beyond Qt 6.

---

## Project Structure

```
DubInstante/
├── main.cpp                      # Entry point
├── CMakeLists.txt                # Build system (CMake + Qt6)
├── resources.qrc                 # Qt resource file (icons, stylesheet)
├── src/
│   ├── core/                     # Business logic — NO UI dependencies
│   │   ├── PlaybackEngine.h/cpp  # Video/audio playback (wraps QMediaPlayer)
│   │   ├── RythmoManager.h/cpp   # Rythmo sync calculations & text management
│   │   ├── AudioRecorder.h/cpp   # Mic recording (wraps QMediaRecorder)
│   │   ├── ExportService.h/cpp   # FFmpeg export process management
│   │   └── SaveManager.h/cpp     # .dbi serialization & ZIP archiving
│   ├── gui/                      # Passive UI widgets — NO business logic
│   │   ├── MainWindow.h/cpp      # Main window — wiring only
│   │   ├── VideoWidget.h/cpp     # OpenGL video renderer
│   │   ├── RythmoWidget.h/cpp    # Single rythmo band renderer
│   │   ├── RythmoOverlay.h/cpp   # Container for 1-2 RythmoWidgets
│   │   ├── TrackPanel.h/cpp      # Audio track controls (device, gain)
│   │   └── ClickableSlider.h     # Custom slider with click-to-seek
│   └── utils/                    # Shared utilities
│       └── TimeFormatter.h/cpp   # ms → "MM:SS" / "HH:MM:SS.mmm"
├── deploy/
│   └── build_appimage.sh         # AppImage packaging script
├── docs/
│   ├── en/README.md              # This file
│   └── fr/README.md              # French version
└── .github/workflows/
    └── main.yml                  # CI: Windows build + AppImage
```

---

## Architecture

### The Golden Rule

> **Core classes NEVER include GUI headers. GUI classes NEVER contain business logic.**

The codebase is split into three strict layers:

| Layer | Directory | Responsibility | Depends on |
|-------|-----------|----------------|------------|
| **Core** | `src/core/` | All calculations, I/O, encoding | Qt Core modules only |
| **GUI** | `src/gui/` | Passive rendering & user input | Core (signals/slots only) |
| **Utils** | `src/utils/` | Shared helper functions | Qt Core |

`MainWindow` is the **wiring hub**: it creates Core and GUI objects, then connects them via signals/slots. It contains no business logic itself.

### Component Dependency Diagram

```
┌──────────────────────────────────────────────────────┐
│                     MainWindow                        │
│                  (creates & wires)                     │
├──────────────────┬───────────────────────────────────┤
│   CORE (owns)    │          GUI (owns)                │
│                  │                                    │
│  PlaybackEngine ─┼──→ VideoWidget                     │
│        │         │                                    │
│        ▼         │                                    │
│  RythmoManager ──┼──→ RythmoOverlay                   │
│                  │      ├── RythmoWidget (track 1)     │
│                  │      └── RythmoWidget (track 2)     │
│                  │                                    │
│  AudioRecorder ──┼──→ TrackPanel                      │
│  AudioRecorder ──┼──→ TrackPanel                      │
│                  │                                    │
│  ExportService   │    ClickableSlider (playback bar)  │
│  SaveManager     │                                    │
└──────────────────┴───────────────────────────────────┘
```

---

## Core Layer — Business Logic

### `PlaybackEngine`
**File**: `src/core/PlaybackEngine.h/cpp`

Wraps `QMediaPlayer` + `QAudioOutput`. Provides a clean API for playback control.

| Method | Description |
|--------|-------------|
| `openFile(QUrl)` | Loads a video file |
| `play()` / `pause()` / `stop()` | Playback control |
| `seek(qint64 ms)` | Seeks to timestamp |
| `setVolume(float)` | 0.0 – 1.0 |
| `setVideoSink(QVideoSink*)` | Connects to a VideoWidget |

**Key signals**: `positionChanged(qint64)`, `durationChanged(qint64)`, `playbackStateChanged(...)`, `errorOccurred(QString)`

This is the **heartbeat** of the app — every other component synchronizes off its `positionChanged` signal.

---

### `RythmoManager`
**File**: `src/core/RythmoManager.h/cpp`

The brain of the rythmo band. Handles:
- **Multi-track text storage** (`QVector<QString>`)
- **Time → cursor index** calculation (how many characters should have scrolled past by time T?)
- **Character insertion/deletion** at the correct cursor position
- **Seek requests** from user interaction on the rythmo band

| Method | Description |
|--------|-------------|
| `sync(qint64 positionMs)` | Main sync point — called on every `positionChanged` |
| `setText(int track, QString)` | Sets entire track text |
| `insertCharacter(int track, QString)` | Inserts at cursor position |
| `deleteCharacter(int track, bool before)` | Backspace / Delete |
| `cursorIndex(qint64 positionMs)` | Calculates cursor position |
| `charDurationMs()` | Duration of one character in ms |

**Key signals**: `trackDataChanged(RythmoTrackData)`, `textChanged(int, QString)`, `seekRequested(qint64)`

The `RythmoTrackData` struct is emitted to the GUI and contains everything a `RythmoWidget` needs to render: `trackIndex`, `text`, `cursorIndex`, `positionMs`, `speed`.

---

### `AudioRecorder`
**File**: `src/core/AudioRecorder.h/cpp`

Wraps `QMediaCaptureSession` + `QMediaRecorder` + `QAudioInput`. One instance per recording track.

| Method | Description |
|--------|-------------|
| `availableDevices()` | Lists microphones |
| `setDevice(QAudioDevice)` | Selects a mic |
| `setVolume(float)` | Input gain 0.0 – 1.0 |
| `startRecording(QUrl)` | Records to WAV file |
| `stopRecording()` | Stops recording |

**Key signals**: `errorOccurred(QString)`, `durationChanged(qint64)`, `recorderStateChanged(...)`

---

### `ExportService`
**File**: `src/core/ExportService.h/cpp`

Manages FFmpeg subprocess for merging video + audio tracks.

| Method | Description |
|--------|-------------|
| `startExport(ExportConfig)` | Launches FFmpeg process |
| `cancelExport()` | Kills the process |
| `isFFmpegAvailable()` | Checks if `ffmpeg` is in PATH |

The `ExportConfig` struct bundles: `videoPath`, `audioPath`, `secondAudioPath`, `outputPath`, `durationMs`, `startTimeMs`, `originalVolume`.

**Key signals**: `progressChanged(int)` (0–100), `exportFinished(bool, QString)`

---

### `SaveManager`
**File**: `src/core/SaveManager.h/cpp`

Handles project serialization. See [Save System](#save-system-dbi-format) for detailed format spec.

| Method | Description |
|--------|-------------|
| `save(QString path, SaveData)` | Saves `.dbi` file |
| `load(QString path, SaveData&)` | Loads `.dbi` file |
| `saveWithMedia(QString zipPath, SaveData, QString*)` | Creates ZIP with `.dbi` + video |
| `isZipAvailable(QString*)` | Checks for `zip` utility (Unix) |
| `sanitize(SaveData)` | Clamps values, normalizes data |

---

## GUI Layer — User Interface

All GUI classes are **passive** — they receive data via slots and emit signals for user interactions. They never perform calculations.

### `MainWindow`
**File**: `src/gui/MainWindow.h/cpp`

The wiring hub. Creates all Core and GUI objects, connects them with signals/slots, and handles top-level menu/keyboard shortcuts. This class should stay **thin** — if you're adding business logic, it belongs in Core.

### `VideoWidget`
**File**: `src/gui/VideoWidget.h/cpp`

Inherits `QOpenGLWidget`. Receives video frames from `QVideoSink` and renders them with GPU acceleration, maintaining aspect ratio. Usage: pass `videoWidget->videoSink()` to `PlaybackEngine::setVideoSink()`.

### `RythmoWidget`
**File**: `src/gui/RythmoWidget.h/cpp`

Renders a single scrolling rythmo band. Supports visual styles:
- `Standalone` — full borders
- `UnifiedTop` / `UnifiedBottom` — for dual-track display

Features:
- **60 FPS animation loop** for smooth scrolling (independent of VideoWidget frame rate)
- **Seek debouncing** to avoid disk saturation on large files
- **Mouse interaction** — click/drag to scrub, double-click to jump
- **Keyboard input** — captures typing for text editing

**Slots** (receives data from `RythmoManager`):
- `updateDisplay(cursorIndex, positionMs, text, speed)`
- `updatePosition(cursorIndex, positionMs)`
- `setPlaying(bool)`

**Signals** (user interactions → forwarded to `RythmoManager`):
- `scrubRequested(int deltaPixels)`
- `characterTyped(QString)`
- `backspacePressed()` / `deletePressed()`
- `navigationRequested(bool forward)`

### `RythmoOverlay`
**File**: `src/gui/RythmoOverlay.h/cpp`

Container widget managing 1–2 `RythmoWidget` instances. Handles layout, visibility of Track 2, and forwards proxy methods (`sync`, `setSpeed`, `setTextColor`) to both tracks.

### `TrackPanel`
**File**: `src/gui/TrackPanel.h/cpp`

UI panel for one audio track. Contains: device selector dropdown, volume slider + spinbox. Delegates all audio operations to its associated `AudioRecorder` instance.

### `ClickableSlider`
**File**: `src/gui/ClickableSlider.h`

Header-only subclass of `QSlider` that supports click-to-seek (clicking on the slider track jumps to that value rather than stepping).

---

## Utils Layer

### `TimeFormatter`
**File**: `src/utils/TimeFormatter.h/cpp`

Namespace with two functions:
- `format(qint64 ms)` → `"MM:SS"` or `"HH:MM:SS"`
- `formatWithMillis(qint64 ms)` → `"MM:SS.mmm"`

---

## Data Flow & Signals/Slots Wiring

Here is how data flows through the application during normal usage:

### Playback Sync
```
PlaybackEngine::positionChanged(ms)
    ├──→ RythmoManager::sync(ms)
    │        └──→ RythmoManager::trackDataChanged(RythmoTrackData)
    │                 └──→ RythmoWidget::updateDisplay(...)
    ├──→ MainWindow: updates slider position
    └──→ MainWindow: updates time label
```

### Text Editing
```
RythmoWidget::characterTyped("A")
    └──→ RythmoManager::insertCharacter(trackIndex, "A")
             └──→ RythmoManager::textChanged(trackIndex, newText)
                      └──→ RythmoWidget: receives updated text via trackDataChanged
```

### Recording
```
MainWindow::toggleRecording()
    ├──→ AudioRecorder1::startRecording(tempPath1)
    ├──→ AudioRecorder2::startRecording(tempPath2)
    └──→ PlaybackEngine::play()

MainWindow::toggleRecording() (second press)
    ├──→ AudioRecorder1::stopRecording()
    ├──→ AudioRecorder2::stopRecording()
    ├──→ PlaybackEngine::pause()
    └──→ ExportService::startExport(config)  [user-triggered]
```

### Save/Load
```
MainWindow::onSaveProject()
    ├──→ SaveManager::isZipAvailable()  [pre-check, main thread]
    ├──→ SaveManager::save() or saveWithMedia()  [ZIP runs in background thread]
    └──→ QProgressDialog  [shown during async ZIP]

MainWindow::onLoadProject()
    └──→ SaveManager::load(path, data)
              └──→ MainWindow: restores all UI state from SaveData
```

---

## Save System (`.dbi` format)

### Binary Layout

```
┌─────────────────────┬──────────┬───────┬──────────────────┬────────────────┬──────────┐
│ Header (15 bytes)   │ Version  │ Flags │ Payload Size     │ XOR-Masked     │ SHA-256  │
│ "DubInstanteFile"   │ (1 byte) │ (1 B) │ (4 B, LE)        │ JSON Payload   │ Checksum │
│                     │          │       │                  │ (N bytes)      │ (32 B)   │
└─────────────────────┴──────────┴───────┴──────────────────┴────────────────┴──────────┘
```

- **Payload Size** is stored **little-endian** for cross-platform portability
- **XOR mask** (key `0x5A`) is applied to the JSON payload for basic obfuscation
- **SHA-256 checksum** is computed on the **unmasked** JSON, then appended
- On load, the checksum is recomputed and compared to detect corruption

### `SaveData` struct

```cpp
struct SaveData {
    QString videoUrl;       // Relative to .dbi file
    float videoVolume;
    QString audioInput1;    // Device name
    float audioGain1;
    QString audioInput2;
    float audioGain2;
    bool enableTrack2;
    int scrollSpeed;
    bool isTextWhite;
    QStringList tracks;     // Track texts (whitespace preserved)
};
```

### ZIP Archives

When saving with video, the app:
1. Creates a temp directory
2. Saves `.dbi` inside (with relative video path)
3. Copies video file alongside
4. Creates ZIP using OS-native tools:
   - **Windows**: `powershell Compress-Archive`
   - **macOS/Linux**: `zip -r`
5. ZIP runs in a **background thread** (`QtConcurrent::run`) with a progress dialog

---

## Export Pipeline

1. User records voice → WAV files saved to temp directory
2. User triggers export → `ExportService` builds FFmpeg command:
   - Input: original video + 1-2 audio tracks
   - Encoding: H.264 CRF 18 (high quality)
   - Audio mixing with volume control
3. FFmpeg runs as `QProcess`, output parsed for progress percentage
4. `progressChanged(int)` updates the UI progress bar
5. On completion, `exportFinished(bool, QString)` notifies the user

---

## Build & Run

### Requirements

| Dependency | Version | Purpose |
|-----------|---------|---------|
| **Qt 6** | 6.5+ | Widgets, Multimedia, OpenGLWidgets, Concurrent |
| **CMake** | 3.16+ | Build system |
| **C++ compiler** | C++17 | GCC 9+, MSVC 2019+, Clang 10+ |
| **FFmpeg** | any | Export (runtime, not compile-time) |
| **zip** | any | ZIP archives (Linux/macOS only, runtime) |
| **GStreamer** | 1.x | Video codec support on Linux |

### Linux Build

```bash
# Install Qt6 + GStreamer codecs
sudo apt install qt6-multimedia-dev libqt6multimediawidgets6 \
    libqt6opengl6-dev libqt6concurrent6 ffmpeg zip \
    gstreamer1.0-libav gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)
./DubInstante
```

### Windows

Pre-built binaries are available from the **Actions** tab (CI artifact: `DubInstante-Windows`).

### AppImage

```bash
./deploy/build_appimage.sh
```

---

## External Dependencies

The project uses **no external C++ libraries** beyond Qt 6. External tools are invoked at runtime:

| Tool | Used by | Required? |
|------|---------|-----------|
| `ffmpeg` | `ExportService` | For export only |
| `zip` | `SaveManager` | For ZIP archives (Unix only) |
| `powershell` | `SaveManager` | For ZIP archives (Windows only) |

---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **Space** | Play / Pause |
| **Esc** | Insert space on rythmo + play |
| **← / →** | Frame-by-frame navigation |
| **Any letter** | Types on the active rythmo band |
| **Backspace** | Deletes character before cursor |
| **Delete** | Deletes character after cursor |

---

## Coding Conventions

1. **Layer separation is strict**: Core classes never `#include` GUI headers
2. **GUI = passive rendering**: Widgets receive data via slots, emit signals for user actions
3. **MainWindow = wiring only**: No calculations, only `connect()` calls
4. **Forward declarations** in headers, `#include` in `.cpp` files
5. **Doxygen comments** on all public methods
6. **Qt naming**: `m_` prefix for member variables, camelCase methods
7. **CMakeLists.txt**: Sources are grouped by layer (`CORE_SOURCES`, `GUI_SOURCES`, `UTILS_SOURCES`)

---

## Roadmap

- **v0.4.0 — Project Management** ✅
    - [x] Save/Load system with `.dbi` format
    - [x] ZIP archive bundling (project + video)
    - [x] Cross-platform compression
    - [x] Full state persistence
- **v0.5.0 — Customization**
    - [ ] Custom rythmo band colors (background + text)
    - [ ] Per-band independent settings
    - [ ] Visual tweaks without layout impact
- **v0.6.0 — Pro Experience**
    - [ ] Full-screen recording mode
    - [ ] Global keyboard shortcuts
- **And more…**
    - [ ] User suggestions welcome! 💡
