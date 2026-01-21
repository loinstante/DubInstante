# DUBSync - Professional Video Dubbing Studio

DUBSync is a professional video dubbing software designed to be powerful, intuitive, and visually refined. It allows you to play videos, write dubbing text on a rythmo band, record synchronized voice tracks, and export the final result.

## ✨ Key Features

### 🎬 Video Playback
- **High-Performance Player**: Hardware-accelerated OpenGL rendering via Qt 6 Multimedia
- **Precise Navigation**: Frame-by-frame scrubbing with visual timeline
- **Real-Time Synchronization**: Audio and video perfectly synced with rythmo bands
- **Speed Control**: Adjustable playback speed (1% to 400%) for practice and review

### 📝 Rythmo Band System
- **Dual Rythmo Bands**: Two independent scrolling text bands for complex dubbing workflows
- **Interactive Editing**: Direct text input on the rythmo band with real-time preview
- **Visual Styles**: Multiple display modes (Classic box, Modern gradient, Minimal text-only, Outlined)
- **Time-Synchronized**: Automatically scrolls in sync with video playback
- **Click-to-Navigate**: Click anywhere on the rythmo to jump to that timestamp

### 🎙️ Multi-Track Recording
- **Dual Track Support**: Record two separate voice tracks simultaneously
- **Device Selection**: Independent microphone selection for each track
- **Real-Time Monitoring**: Live gain control with visual feedback sliders
- **Volume Control**: Per-track volume adjustment (0-100%)
- **Professional Recording**: High-quality WAV capture with configurable gain

### 🎨 Modern UI Design
- **Refined Interface**: Clean, professional light theme with polished controls
- **Responsive Sliders**: Smooth, elegant sliders with gradient fills
- **Compact Spinboxes**: Optimized numeric inputs for precise control
- **Intuitive Layout**: Well-organized controls with clear visual hierarchy
- **Custom Styling**: Modern Qt stylesheet with attention to detail

### 📤 Export & Integration
- **FFmpeg Integration**: Professional video/audio merging
- **Multi-Track Export**: Combines original video with both voice tracks
- **Quality Preservation**: Maintains original video quality while adding dubbed audio
- **Progress Tracking**: Visual progress bar during export

## 🏗️ Architecture

- **MainWindow**: Central hub coordinating all UI components and workflows
- **RythmoWidget**: Scrolling text band synchronized with video playback
- **RythmoOverlay**: Transparent overlay system for dual rythmo display
- **AudioRecorderManager**: Multi-track audio capture with device management
- **PlayerController**: Core multimedia playback engine (Qt 6 Multimedia)
- **VideoWidget**: Hardware-accelerated video rendering (OpenGL)
- **Exporter**: FFmpeg-based video/audio merger

## 📋 Requirements

- **Qt 6.5+** (Modules: `Widgets`, `Multimedia`, `OpenGLWidgets`)
- **FFmpeg**: Required for final export (`sudo apt install ffmpeg` on Linux)
- **Codecs (GStreamer)**: For MP4 playback on Linux
    ```bash
    sudo apt install gstreamer1.0-libav gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly
    ```

## 🚀 Installation & Build

### Windows
The project is automatically compiled for Windows via GitHub Actions.
1. Go to the **Actions** tab of this repository
2. Download the latest **DUBSync-Windows** artifact

### Linux (Manual Build)
1. Install dependencies:
   ```bash
   sudo apt install qt6-multimedia-dev libqt6opengl6-dev ffmpeg
   ```
2. Build:
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ./DUBSync
   ```

## 🎹 Keyboard Shortcuts & Usage

### Playback Controls
- **Space**: Play / Pause
- **Esc**: Insert space on rythmo and start playback
- **Left/Right Arrows**: Frame-by-frame navigation

### Recording Workflow
1. **Load Video**: Click "Ouvrir Vidéo" to select your video file
2. **Configure Tracks**:
   - Select microphone for Track 1 (and Track 2 if enabled)
   - Adjust gain levels with sliders (0-100%)
   - Set volume levels for monitoring
3. **Edit Rythmo**:
   - Type directly on the rythmo band to add text
   - Text automatically scrolls with video playback
   - Click to jump to specific timestamps
4. **Record**:
   - Click **REC** button to start recording
   - Speak your lines in sync with the rythmo
   - Click **REC** again to stop
5. **Export**:
   - Review your recording
   - Export final video with merged audio tracks

### Advanced Features
- **Dual Track Mode**: Enable "Activer Piste 2" for simultaneous two-track recording
- **Speed Adjustment**: Use "Vitesse Défilement" spinbox to slow down or speed up playback
- **Visual Styles**: Configure rythmo band appearance in the code (RythmoWidget::VisualStyle)

## 🔧 Configuration

### Rythmo Visual Styles
Edit `RythmoWidget.cpp` to customize the rythmo appearance:
- **ClassicBox**: Traditional box-based display
- **ModernGradient**: Gradient-filled modern look
- **MinimalText**: Clean text-only display
- **Outlined**: Text with outline for better contrast

### Audio Settings
- Sample Rate: 48000 Hz
- Format: WAV (uncompressed)
- Channels: Mono per track

## 🎨 UI Design Philosophy

DUBSync features a carefully crafted user interface with:
- **Clean Professional Theme**: Light color scheme with subtle depth
- **Refined Controls**: Polished spinboxes, sliders, and buttons
- **Visual Hierarchy**: Clear organization of controls by function
- **Responsive Design**: Smooth hover effects and interactions
- **Accessibility**: High contrast text and clear labeling

## 🔄 CI/CD Pipeline

The project uses GitHub Actions to ensure stability and provide ready-to-use Windows builds. Each commit triggers:
- Automated compilation checks
- Cross-platform build verification
- Downloadable binary generation

## 📜 License

This project is open-source. Feel free to contribute, fork, or use it for your dubbing projects!

## 🤝 Contributing

Contributions are welcome! Whether it's:
- Bug reports
- Feature requests
- Code improvements
- UI/UX enhancements
- Documentation updates

Please open an issue or submit a pull request.

---

**DUBSync** - Making professional video dubbing accessible to everyone.
