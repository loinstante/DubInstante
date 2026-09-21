#ifndef FFMPEGFRAMEEXTRACTOR_H
#define FFMPEGFRAMEEXTRACTOR_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

/**
 * @brief Background worker that handles actual FFmpeg C APIs.
 */
class FFmpegWorker : public QObject {
    Q_OBJECT
public:
    explicit FFmpegWorker(QObject *parent = nullptr);
    ~FFmpegWorker() override;

    void setFilePath(const QString &filePath);
    void requestFrame(qint64 timestampMs);
    void stop();

signals:
    void frameExtracted(const QImage &image);
    void errorOccurred(const QString &errorMsg);

public slots:
    void process();

private:
    void closeFile();
    bool openFile(const QString &filePath);

    // Sync
    QMutex m_mutex;
    QWaitCondition m_condition;
    std::atomic<bool> m_abort;

    // Request State
    QString m_requestedFilePath;
    bool m_fileChanged; // Always accessed under m_mutex
    std::atomic<qint64> m_requestedTimestampMs;
    std::atomic<bool> m_hasNewRequest;

    // FFmpeg state
    AVFormatContext *m_formatContext;
    AVCodecContext *m_codecContext;
    AVFrame *m_frame;
    AVFrame *m_rgbFrame;
    AVPacket *m_packet;
    SwsContext *m_swsContext;
    int m_videoStreamIndex;
    
    // Buffers
    uint8_t *m_rgbBuffer;
};

/**
 * @brief High-level wrapper that manages the worker thread.
 */
class FFmpegFrameExtractor : public QObject {
    Q_OBJECT
public:
    explicit FFmpegFrameExtractor(QObject *parent = nullptr);
    ~FFmpegFrameExtractor() override;

    void openFile(const QString &filePath);
    void requestFrame(qint64 timestampMs);

signals:
    void frameExtracted(const QImage &image);
    void errorOccurred(const QString &errorMsg);

private:
    QThread *m_workerThread;
    FFmpegWorker *m_worker;
};

#endif // FFMPEGFRAMEEXTRACTOR_H
