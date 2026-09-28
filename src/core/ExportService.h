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

#include <QDateTime>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

class QTemporaryFile;

/**
 * @struct ExportSegment
 * @brief A piece of take heard in the mix: file range placed on the timeline.
 */
struct ExportSegment {
    QString path;                   ///< Recorded WAV
    qint64 timelineStartMs = 0;     ///< Where it plays, in source video time
    qint64 sourceOffsetMs = 0;      ///< Where it starts inside the file
    qint64 durationMs = 0;
    float volume = 1.0f;            ///< Track volume (0 = muted)
};

/**
 * @struct ExportConfig
 * @brief Configuration structure for export operations.
 * 
 * Contains all parameters needed to perform a video/audio merge.
 */
struct ExportConfig {
    QString videoPath;              ///< Absolute path to source video
    QList<ExportSegment> segments;  ///< Every take piece heard, all tracks together
    QString outputPath;             ///< Absolute path for output file
    qint64 durationMs;              ///< Recording duration in milliseconds (-1 for full)
    qint64 rangeStartMs;            ///< Start of the exported range, in source video time (0 = from the beginning)
    float originalVolume;           ///< Volume of original video audio (0.0 to 1.0)
    QString scaleResolution;        ///< Resolution scale: e.g., "1920:-2", "1280:-2", or empty
    QString speedPreset;            ///< FFmpeg speed preset: e.g., "ultrafast", "medium", "slow"
    int crf;                        ///< CRF value: 0 to 51 (default 21)
    int audioBitrateKbps;           ///< Audio bitrate in kbps (default 192)
    QString format;                 ///< Container format: e.g., "mp4", "mkv", "mov", "avi"
    
    // Expert Mode fields
    bool expertMode;                ///< Whether to use manual expert settings
    QString videoCodec;             ///< Video encoder: e.g., "libx264", "libx265", "prores", "libvpx-vp9", "copy"
    QString audioCodec;             ///< Audio encoder: e.g., "aac", "libmp3lame", "ac3", "pcm_s16le", "pcm_s24le", "copy"
    int videoBitrateMbps;           ///< Video target bitrate in Mbps (0 for CRF mode)
    int sampleRateHz;               ///< Audio sample rate in Hz (0 for original)
    QString customFFmpegFlags;      ///< Raw extra FFmpeg arguments
    
    ExportConfig()
        : durationMs(-1)
        , rangeStartMs(0)
        , originalVolume(1.0f)
        , speedPreset("medium")
        , crf(21)
        , audioBitrateKbps(192)
        , format("mp4")
        , expertMode(false)
        , videoCodec("libx264")
        , audioCodec("aac")
        , videoBitrateMbps(0)
        , sampleRateHz(0)
    {}
};

/**
 * @class ExportService
 * @brief Manages FFmpeg-based video export operations.
 * 
 * Features:
 * - Mixes every take segment of every track over the video
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
 * config.segments = {{"/path/to/take.wav", 1500, 0, 30000}};
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
    ~ExportService() override;

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
     * @brief Checks if FFmpeg and FFprobe are available.
     * @param errorMessage Optional pointer to store installation instructions if missing.
     * @return true if both are resolvable through toolPath().
     */
    static bool isFFmpegAvailable(QString *errorMessage = nullptr);

    /**
     * @brief Resolves an FFmpeg tool ("ffmpeg" or "ffprobe") to a full path.
     *
     * The copy shipped with the application wins over the system one: it is the
     * version the export arguments were written against. Looks next to the
     * executable (which covers AppDir/usr/bin in the AppImage and
     * Contents/MacOS in the macOS bundle), then in Contents/Resources, then in
     * the PATH. Returns an empty string if the tool is nowhere to be found.
     */
    static QString toolPath(const QString &name);
    
    /**
     * @brief Returns whether an export is currently in progress.
     */
    bool isExporting() const;

    /**
     * @brief Keeps the parts of @p segments inside the exported range, with
     *        timelineStartMs made relative to the range start.
     * @param durationMs Range length, <= 0 for "until the end".
     */
    static QList<ExportSegment> clipSegments(const QList<ExportSegment> &segments,
                                             qint64 rangeStartMs, qint64 durationMs);

    /**
     * @brief Builds the -filter_complex graph producing [aout].
     * @param audioInputs Output: the distinct take files, inputs 1..N in order.
     */
    static QString buildAudioGraph(const ExportConfig &config, QStringList *audioInputs);

    /**
     * @brief Builds the FFmpeg command arguments.
     * @param filterScriptPath File holding buildAudioGraph(): a comped project
     *        easily exceeds the 32767-character Windows command line.
     * @param ffmpegMajor Major version: 7 and later read the file through
     *        "-/filter_complex", older ones through "-filter_complex_script".
     */
    static QStringList buildFFmpegArgs(const ExportConfig &config,
                                       const QString &filterScriptPath, int ffmpegMajor);

    /**
     * @brief Major version of @p program, 99 for a git build, 0 if unknown.
     */
    static int ffmpegMajorVersion(const QString &program);

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
     * @brief Validates the export configuration.
     * @param config Configuration to validate.
     * @param errorMessage Output: error message if validation fails.
     * @return true if configuration is valid.
     */
    bool validateConfig(const ExportConfig &config, QString &errorMessage) const;

    /**
     * @brief Deletes the partial output file after a failed or cancelled export.
     */
    void removePartialOutput();

    /**
     * @brief Forgets the output tracked for the current run, making removePartialOutput() a no-op.
     */
    void resetOutputTracking();

    QProcess *m_process;
    qint64 m_totalDurationMs;
    QString m_errorAccumulator;
    QString m_currentOutputPath;
    bool m_exportFinishedEmitted;
    bool m_outputExistedBefore;
    QDateTime m_outputMTimeBefore;
    std::unique_ptr<QTemporaryFile> m_filterScript;
};

#endif // EXPORTSERVICE_H
