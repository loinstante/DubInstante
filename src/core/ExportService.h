/**
 * @file ExportService.h
 * @brief Core service for video/audio export using FFmpeg.
 * 
 * This class handles the post-processing export workflow:
 * merging video with recorded audio tracks using FFmpeg.
 * It provides progress reporting and error handling.
 * 
 * @note Part of the Core layer - no UI dependencies allowed.
 * @note Requires FFmpeg to be installed and available in PATH.
 */

#ifndef EXPORTSERVICE_H
#define EXPORTSERVICE_H

#include <QObject>
#include <QProcess>
#include <QString>

/**
 * @struct ExportConfig
 * @brief Configuration structure for export operations.
 * 
 * Contains all parameters needed to perform a video/audio merge.
 */
struct ExportConfig {
    QString videoPath;          ///< Absolute path to source video
    QString audioPath;          ///< Absolute path to primary recorded audio
    QString secondAudioPath;    ///< Optional: path to second audio track
    QString outputPath;         ///< Absolute path for output file
    qint64 durationMs;          ///< Recording duration in milliseconds (-1 for full)
    qint64 startTimeMs;         ///< Start time offset in milliseconds
    float originalVolume;       ///< Volume of original video audio (0.0 to 1.0)
    
    ExportConfig()
        : durationMs(-1)
        , startTimeMs(0)
        , originalVolume(1.0f)
    {}
};

/**
 * @class ExportService
 * @brief Manages FFmpeg-based video export operations.
 * 
 * Features:
 * - Merges video with one or two audio tracks
 * - Supports audio mixing with volume control
 * - Reports progress via signals
 * - High-quality H.264 encoding (CRF 18)
 * 
 * @example
 * @code
 * auto service = new ExportService(this);
 * 
 * connect(service, &ExportService::progressChanged,
 *         progressBar, &QProgressBar::setValue);
 * connect(service, &ExportService::exportFinished,
 *         this, &MainWindow::onExportComplete);
 * 
 * ExportConfig config;
 * config.videoPath = "/path/to/video.mp4";
 * config.audioPath = "/path/to/audio.wav";
 * config.outputPath = "/path/to/output.mp4";
 * config.durationMs = 30000;
 * 
 * service->startExport(config);
 * @endcode
 */
class ExportService : public QObject {
    Q_OBJECT

public:
    explicit ExportService(QObject *parent = nullptr);
    ~ExportService() override = default;

    // =========================================================================
    // Export Operations
    // =========================================================================
    
    /**
     * @brief Starts an export operation with the given configuration.
     * @param config Export configuration parameters.
     * 
     * Emits progressChanged during processing and exportFinished on completion.
     */
    void startExport(const ExportConfig &config);
    
    /**
     * @brief Cancels a running export operation.
     */
    void cancelExport();
    
    /**
     * @brief Checks if FFmpeg is available on the system.
     * @return true if FFmpeg is installed and accessible.
     */
    bool isFFmpegAvailable() const;
    
    /**
     * @brief Returns whether an export is currently in progress.
     */
    bool isExporting() const;

signals:
    /**
     * @brief Emitted periodically during export with progress percentage.
     * @param percentage Progress from 0 to 100.
     */
    void progressChanged(int percentage);
    
    /**
     * @brief Emitted when export completes (success or failure).
     * @param success true if export succeeded.
     * @param message Human-readable status or error message.
     */
    void exportFinished(bool success, const QString &message);

private slots:
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleProcessError(QProcess::ProcessError error);
    void parseProgressOutput();

private:
    /**
     * @brief Builds the FFmpeg command arguments.
     * @param config Export configuration.
     * @return List of command-line arguments.
     */
    QStringList buildFFmpegArgs(const ExportConfig &config) const;
    
    /**
     * @brief Validates the export configuration.
     * @param config Configuration to validate.
     * @param errorMessage Output: error message if validation fails.
     * @return true if configuration is valid.
     */
    bool validateConfig(const ExportConfig &config, QString &errorMessage) const;

    QProcess *m_process;
    qint64 m_totalDurationMs;
};

#endif // EXPORTSERVICE_H
