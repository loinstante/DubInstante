# Changelog

All notable changes to **DubInstante** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.6.0] - 2026-02-25

### Added
- **Native Android App**: Transitioned from QML prototype to a high-performance native Android application (Kotlin/Jetpack Compose).
- **JNI Integration**: Integrated the core C++ engine via Java Native Interface (JNI) for high-precision Rythmo synchronization.
- **Enhanced Mobile UI**: Streamlined interface with compact header, integrated video player, and optimized Rythmo band for mobile screens.
- **Audio & Export**: Native microphone capture and FFmpeg-powered video/audio mixing for Android.
- **CI/CD Build**: Automated APK builds for the native app on GitHub Actions.
- **Mobile Feedback**: Added quick-report links for mobile users to report bugs easily.

## [0.5.0] - 2026-02-16

### Added
- **Fullscreen Recording Mode**: New "Fullscreen Recording" checkbox — when enabled, the video player and rythmo band go fullscreen during recording for an immersive experience.
- **Stop Recording Shortcut**: `Ctrl+S` immediately stops the active recording session.
- **Shortcuts Popup Menu**: New "⌨" button in the control bar displays all available keyboard shortcuts.

### Fixed
- **edition of the rythmo band**: fixed the rythmo band that could be changed when recording 

## [0.4.2] - 2026-02-13

### Fixed
- **macOS Launch**: Simplified installation with `xattr -c` as the primary Gatekeeper workaround (confirmed working).
- **macOS CI Test**: Added dedicated smoke-test workflow (`test-macos.yml`) with headless launch verification.
- **macOS CI Compatibility**: Fixed `timeout` command not available on macOS runners (replaced with `perl` alarm).

## [0.4.1] - 2026-02-13

### Fixed
- **macOS "App is Damaged"**: Added ad-hoc code signing (`codesign --force --deep --sign -`) in CI to prevent Gatekeeper from flagging the app as damaged.
- **macOS Build**: Repaired bundle generation and deployment. The app is now correctly packaged as a `.app` bundle inside a `.dmg`.
- **macOS CI Pipeline**: Split packaging into 3 steps (macdeployqt → codesign → hdiutil) for reliability.
- **macOS Gatekeeper Docs**: Added `xattr -cr` Terminal workaround for first-launch issues.

## [0.4.0] - 2026-02-11

### Added
- **Project Management System**: New `.dbi` custom binary format with XOR obfuscation and SHA-256 integrity checks.
- **ZIP Archives**: Capability to bundle the project file and video into a single portable `.zip` archive.
- **Async Saving**: Non-blocking save operations with progress indicators for large video archives.
- **Cross-Platform Compression**: Native support for PowerShell (Windows) and `zip` (Unix) for project bundling.
- **Technical Documentation**: Comprehensive onboarding guides in English and French covering the entire architecture.

## [0.3.5] - 2026-02-11

### Added
- **Macos standalone build**: Added standalone build for macos

### Fixed
- **GitHub Actions**: unifying all platform build process


## [0.3.4] - 2026-02-09

### Added
- **Ultra-Smooth 60FPS Rythmo**: Implemented dedicated interpolation loop for decoupled, high-framerate band scrolling.
- **Precision Snap-to-Grid**: Smart nearest-neighbor character alignment when paused for intuitive editing.
- **Unified Cursors**: Perfectly aligned the timestamp line and the playback guide line for zero-parallax editing.

### Fixed
- **Rythmo Navigation**: Backspace now allows moving back through empty space beyond the end of text.
- **Stability**: Fixed a segmentation fault on startup related to uninitialized timers.
- **Video Engine**: Refined media loading sequence (PlaybackEngine) to ensure consistent frame rendering on file open.

## [0.3.3] - 2026-02-09

### Technical
- **Refactoring**: refactoring entire Main class with seperation of Core and UI preparing future platform availability 

## [0.3.2] - 2026-01-27

### Added
- **Track 2 audio recording not mendatory**: add possibility to record with only one mic but with the 
 rythmo band of the second track
### Fixed
- **Export**: Fixed export errors by ensuring proper file paths and permissions
- **Export**: Fixed export errors by ensuring proper audio recording and export

### Technical
- **refactoring mainwindows class**: fixing the mainclass that was godclass for better security, maintainability and bug preventing 

## [0.3.1] - 2026-01-24

### Fixed
- **Windows Build**: Fixed build errors on Windows using github actions

## [0.3.0] - 2026-01-24

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

## [0.2.0] - 2026-01-21

### Added
- **Project Rebranding**: Officially renamed the software from DUBSync to **DubInstante**

## [0.1.0] - 2026-01-21

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

## [0.0.0] - 2026-01-12

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