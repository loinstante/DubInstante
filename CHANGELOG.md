# Changelog

All notable changes to **DubInstante** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.4] - 2026-02-09

### Added
- **Ultra-Smooth 60FPS Rythmo**: Implemented dedicated interpolation loop for decoupled, high-framerate band scrolling.
- **Precision Snap-to-Grid**: Smart nearest-neighbor character alignment when paused for intuitive editing.
- **Unified Cursors**: Perfectly aligned the timestamp line and the playback guide line for zero-parallax editing.

### Fixed
- **Rythmo Navigation**: Backspace now allows moving back through empty space beyond the end of text.
- **Stability**: Fixed a segmentation fault on startup related to uninitialized timers.
- **Video Engine**: Refined media loading sequence (PlaybackEngine) to ensure consistent frame rendering on file open.

## [1.3.3] - 2026-02-09

### Technical
- **Refactoring**: refactoring entire Main class with seperation of Core and UI preparing future platform availability 

## [1.3.2] - 2026-01-27

### Added
- **Track 2 audio recording not mendatory**: add possibility to record with only one mic but with the 
 rythmo band of the second track
### Fixed
- **Export**: Fixed export errors by ensuring proper file paths and permissions
- **Export**: Fixed export errors by ensuring proper audio recording and export

### Technical
- **refactoring mainwindows class**: fixing the mainclass that was godclass for better security, maintainability and bug preventing 

## [1.3.1] - 2026-01-24

### Fixed
- **Windows Build**: Fixed build errors on Windows using github actions

## [1.3.0] - 2026-01-24

### Added
- **Rythmo Text Color Toggle**: Switch between Black and White text for better visibility on different video backgrounds
- **Virtualized Rythmo Rendering**: Drastically improved performance by only rendering the visible portion of the text band.
- **Instant Editing Feedback**: Decoupled text input and scrolling from video engine latency, ensuring smooth typing even on massive (50GB+) files.
- **Professional Seek Debouncing**: Implemented smart batching of video seek requests during rapid editing or scrubbing to prevent disk I/O saturation.

### Fixed
- **Large File Lag**: Resolved significant performance drops and UI freezing when working with 50min+ videos.
- **Missing Icons**: Resolved "Cannot open file" errors for `volume_up.svg` and `fiber_manual_record.svg` by mapping them to existing assets

### Technical
- **Font Metric Caching**: Implemented caching for fixed-font character widths to reduce CPU overhead during text rendering.
- **Optimized Paint Pipeline**: Refined the RythmoWidget refresh cycle for smoother animations and lower power consumption.

## [1.2.0] - 2026-01-21

### Added
- **Project Rebranding**: Officially renamed the software from DUBSync to **DubInstante**

## [1.1.0] - 2026-01-21

### Added
- **Dual Track Recording**: Second independent audio input for simultaneous two-track dubbing
- **Track 2 Controls**: Dedicated microphone selection, gain control, and volume slider for second track
- **Track 2 Toggle**: "Activer Piste 2" checkbox to enable/disable second track recording
- **Dual Rythmo Display**: Second rythmo band overlay for complex dubbing workflows
- **Multi-Track Export**: FFmpeg export now merges both audio tracks with the original video

### Changed
- Refactored audio recording architecture to support multiple `AudioRecorderManager` instances
- Enhanced UI layout with collapsible Track 2 section
- Improved rythmo overlay system with transparent dual-band rendering

### Technical
- Added `AudioRecorderManager2` for second track audio capture
- Implemented `RythmoOverlay` component for dual rythmo display
- Extended `Exporter` to handle multi-track audio merging
- Updated file paths for second track (`temp_dub_2.wav`, `recorded_audio_2.wav`)

---

## [1.0.0] - 2026-01-12

### Initial Release
- **Single Track Recording**: Professional audio dubbing with one microphone input
- **Rythmo Band**: Interactive scrolling text synchronized with video playback
- **Video Playback**: Hardware-accelerated player with frame-by-frame navigation
- **Audio Controls**: Microphone selection, gain adjustment, and volume control
- **Speed Control**: Variable playback speed (1% to 400%) for practice
- **Export Functionality**: FFmpeg-based video/audio merging
- **Modern UI**: Clean, professional interface with Qt 6 styling
- **Keyboard Shortcuts**: Space (play/pause), Esc (insert & play), arrows (frame navigation)

### Features
- OpenGL-accelerated video rendering
- Direct text input on rythmo band
- Real-time audio monitoring
- WAV recording with configurable gain
- Interactive timeline scrubbing
- Cross-platform support (Windows, Linux)

### Core Infrastructure
- **Complete Dubbing Foundation**: Built the entire professional dubbing system from scratch
- **PlayerController Engine**: Full multimedia playback architecture with Qt 6 Multimedia
- **Audio Recording Pipeline**: High-quality WAV capture system with device management
- **Rythmo Synchronization**: Real-time text band synchronized with video timeline
- **Export Pipeline**: FFmpeg integration for video/audio merging
- **OpenGL Rendering**: Hardware-accelerated video display system
- **UI Framework**: Modern, responsive interface with custom Qt styling

---