/**
 * @file PlaybackEngine.h
 * @brief Core engine for media playback management.
 * 
 * This class encapsulates QMediaPlayer and QAudioOutput, providing a clean
 * interface for video/audio playback. It inherits from QObject (not QWidget)
 * to ensure complete separation from UI concerns.
 * 
 * @note Part of the Core layer - no UI dependencies allowed.
 */

#ifndef PLAYBACKENGINE_H
#define PLAYBACKENGINE_H

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>
#include <QUrl>

class QVideoSink;

/**
 * @class PlaybackEngine
 * @brief Manages media playback lifecycle and state.
 * 
 * Responsibilities:
 * - Media file loading and source management
 * - Playback control (play, pause, stop, seek)
 * - Volume management
 * - Emitting playback state and position signals
 * 
 * @example
 * @code
 * auto engine = new PlaybackEngine(this);
 * engine->setVideoSink(videoWidget->videoSink());
 * engine->openFile(QUrl::fromLocalFile("/path/to/video.mp4"));
 * engine->play();
 * @endcode
 */
class PlaybackEngine : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a PlaybackEngine instance.
     * @param parent Parent QObject for memory management.
     */
    explicit PlaybackEngine(QObject *parent = nullptr);
    
    ~PlaybackEngine() override = default;

    // =========================================================================
    // Configuration
    // =========================================================================
    
    /**
     * @brief Sets the video sink for frame output.
     * @param sink QVideoSink from a VideoWidget or similar receiver.
     */
    void setVideoSink(QVideoSink *sink);
    
    /**
     * @brief Opens a media file for playback.
     * @param url URL to the media file (local or remote).
     */
    void openFile(const QUrl &url);

    // =========================================================================
    // State Accessors
    // =========================================================================
    
    /** @brief Returns the total duration of the current media in milliseconds. */
    qint64 duration() const;
    
    /** @brief Returns the current playback position in milliseconds. */
    qint64 position() const;
    
    /** @brief Returns the current playback state (Playing, Paused, Stopped). */
    QMediaPlayer::PlaybackState playbackState() const;
    
    /** @brief Returns the current volume level (0.0 to 1.0). */
    float volume() const;
    
    /** @brief Returns the video frame rate in FPS. Defaults to 25.0 if unknown. */
    qreal videoFrameRate() const;

public slots:
    // =========================================================================
    // Playback Control
    // =========================================================================
    
    /** @brief Starts or resumes playback. */
    void play();
    
    /** @brief Pauses playback at current position. */
    void pause();
    
    /** @brief Stops playback and resets position to beginning. */
    void stop();
    
    /**
     * @brief Seeks to a specific position.
     * @param position Target position in milliseconds.
     */
    void seek(qint64 position);
    
    /**
     * @brief Sets the playback volume.
     * @param volume Volume level from 0.0 (mute) to 1.0 (max).
     */
    void setVolume(float volume);

signals:
    // =========================================================================
    // State Change Signals
    // =========================================================================
    
    /** @brief Emitted when playback position changes. */
    void positionChanged(qint64 position);
    
    /** @brief Emitted when media duration becomes known or changes. */
    void durationChanged(qint64 duration);
    
    /** @brief Emitted when playback state changes. */
    void playbackStateChanged(QMediaPlayer::PlaybackState state);
    
    /** @brief Emitted when media status changes (loading, loaded, etc.). */
    void mediaStatusChanged(QMediaPlayer::MediaStatus status);
    
    /** @brief Emitted when media metadata is loaded (contains frame rate, etc.). */
    void metaDataChanged();
    
    /** @brief Emitted when volume changes. */
    void volumeChanged(float volume);
    
    /** @brief Emitted when an error occurs during playback. */
    void errorOccurred(const QString &error);

private:
    QMediaPlayer *m_mediaPlayer;
    QAudioOutput *m_audioOutput;
};

#endif // PLAYBACKENGINE_H
