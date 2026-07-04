extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include "FFmpegFrameExtractor.h"
#include <QDebug>

// ============================================================================
// FFmpegWorker Implementation
// ============================================================================

FFmpegWorker::FFmpegWorker(QObject *parent)
    : QObject(parent)
    , m_abort(false)
    , m_fileChanged(false)
    , m_requestedTimestampMs(-1)
    , m_hasNewRequest(false)
    , m_formatContext(nullptr)
    , m_codecContext(nullptr)
    , m_frame(nullptr)
    , m_rgbFrame(nullptr)
    , m_packet(nullptr)
    , m_swsContext(nullptr)
    , m_videoStreamIndex(-1)
    , m_rgbBuffer(nullptr)
{
    m_frame = av_frame_alloc();
    m_rgbFrame = av_frame_alloc();
    m_packet = av_packet_alloc();
}

FFmpegWorker::~FFmpegWorker() {
    stop();
    closeFile();
    
    if (m_frame) av_frame_free(&m_frame);
    if (m_rgbFrame) av_frame_free(&m_rgbFrame);
    if (m_packet) av_packet_free(&m_packet);
}

void FFmpegWorker::setFilePath(const QString &filePath) {
    QMutexLocker locker(&m_mutex);
    m_requestedFilePath = filePath;
    m_fileChanged = true;
    m_condition.wakeOne();
}

void FFmpegWorker::requestFrame(qint64 timestampMs) {
    m_requestedTimestampMs.store(timestampMs);
    QMutexLocker locker(&m_mutex);
    m_hasNewRequest = true;
    m_condition.wakeOne();
}

void FFmpegWorker::stop() {
    QMutexLocker locker(&m_mutex);
    m_abort = true;
    m_condition.wakeOne();
}

void FFmpegWorker::closeFile() {
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
    }
    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
    }
    if (m_rgbBuffer) {
        av_free(m_rgbBuffer);
        m_rgbBuffer = nullptr;
    }
    m_videoStreamIndex = -1;
}

bool FFmpegWorker::openFile(const QString &filePath) {
    QByteArray filePathUtf8 = filePath.toUtf8();
    
    if (avformat_open_input(&m_formatContext, filePathUtf8.constData(), nullptr, nullptr) != 0) {
        emit errorOccurred("Could not open file with FFmpeg: " + filePath);
        return false;
    }

    if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
        emit errorOccurred("Could not find stream info");
        return false;
    }

    m_videoStreamIndex = -1;
    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++) {
        if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            break;
        }
    }

    if (m_videoStreamIndex == -1) {
        emit errorOccurred("Could not find video stream");
        return false;
    }

    AVCodecParameters *codecParams = m_formatContext->streams[m_videoStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) {
        emit errorOccurred("Unsupported codec");
        return false;
    }

    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) {
        emit errorOccurred("Could not allocate codec context");
        return false;
    }

    if (avcodec_parameters_to_context(m_codecContext, codecParams) < 0) {
        emit errorOccurred("Could not copy codec params");
        return false;
    }

    if (avcodec_open2(m_codecContext, codec, nullptr) < 0) {
        emit errorOccurred("Could not open codec");
        return false;
    }

    // Allocate RGB buffer
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, m_codecContext->width, m_codecContext->height, 1);
    if (numBytes <= 0) {
        emit errorOccurred("Invalid video dimensions");
        return false;
    }
    m_rgbBuffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    if (!m_rgbBuffer) {
        emit errorOccurred("Could not allocate frame buffer");
        return false;
    }

    av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize, m_rgbBuffer,
                         AV_PIX_FMT_BGRA, m_codecContext->width, m_codecContext->height, 1);

    m_swsContext = sws_getContext(m_codecContext->width, m_codecContext->height, m_codecContext->pix_fmt,
                                  m_codecContext->width, m_codecContext->height, AV_PIX_FMT_BGRA,
                                  SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsContext) {
        emit errorOccurred("Could not initialize video scaler");
        return false;
    }

    return true;
}

void FFmpegWorker::process() {
    while (!m_abort) {
        qint64 targetMs = -1;
        QString targetFile;
        bool needOpenFile = false;

        {
            QMutexLocker locker(&m_mutex);
            while (!m_abort && !m_hasNewRequest && !m_fileChanged) {
                m_condition.wait(&m_mutex);
            }
            if (m_abort) break;

            if (m_fileChanged) {
                targetFile = m_requestedFilePath;
                needOpenFile = true;
                m_fileChanged = false;
            }

            if (m_hasNewRequest) {
                targetMs = m_requestedTimestampMs.load();
                m_hasNewRequest = false;
            }
        }

        if (needOpenFile) {
            closeFile();
            if (!openFile(targetFile)) {
                closeFile(); // Purge partially initialized state
            }
        }

        if (!m_formatContext || !m_codecContext || m_videoStreamIndex < 0 ||
            !m_swsContext || !m_rgbBuffer || targetMs < 0) {
            continue;
        }

        // Convert target ms to pts
        AVStream *stream = m_formatContext->streams[m_videoStreamIndex];
        int64_t targetPts = (int64_t)(targetMs * ((double)stream->time_base.den / (1000.0 * stream->time_base.num)));

        // Seek to nearest I-Frame before target
        av_seek_frame(m_formatContext, m_videoStreamIndex, targetPts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(m_codecContext);

        bool frameFound = false;
        
        while (av_read_frame(m_formatContext, m_packet) >= 0) {
            if (m_abort || m_hasNewRequest) {
                av_packet_unref(m_packet);
                break; // Debounce / cancel if a new seek arrived
            }

            if (m_packet->stream_index == m_videoStreamIndex) {
                if (avcodec_send_packet(m_codecContext, m_packet) >= 0) {
                    while (avcodec_receive_frame(m_codecContext, m_frame) >= 0) {
                        if (m_abort || m_hasNewRequest) break;

                        int64_t pts = m_frame->best_effort_timestamp;
                        if (pts == AV_NOPTS_VALUE) pts = m_frame->pts;
                        
                        // We reached the requested frame or just slightly passed it
                        if (pts >= targetPts) {
                            frameFound = true;
                            break;
                        }
                    }
                }
            }
            av_packet_unref(m_packet);
            if (frameFound || m_abort || m_hasNewRequest) break;
        }

        // Emit if found and not superseded by another request
        if (frameFound && !m_abort && !m_hasNewRequest) {
            sws_scale(m_swsContext, m_frame->data, m_frame->linesize, 0, m_codecContext->height,
                      m_rgbFrame->data, m_rgbFrame->linesize);

            QImage img(m_rgbBuffer, m_codecContext->width, m_codecContext->height,
                       m_rgbFrame->linesize[0], QImage::Format_RGB32);
            
            // Deep copy is essential because we reuse m_rgbBuffer
            emit frameExtracted(img.copy());
        }
    }
}

// ============================================================================
// FFmpegFrameExtractor Implementation
// ============================================================================

FFmpegFrameExtractor::FFmpegFrameExtractor(QObject *parent)
    : QObject(parent)
{
    m_workerThread = new QThread(this);
    m_worker = new FFmpegWorker();
    
    // Move worker to background thread
    m_worker->moveToThread(m_workerThread);

    // Connect signals safely across threads
    connect(m_workerThread, &QThread::started, m_worker, &FFmpegWorker::process);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    
    // Forward the extracted frame and error signals
    connect(m_worker, &FFmpegWorker::frameExtracted, this, &FFmpegFrameExtractor::frameExtracted, Qt::QueuedConnection);
    connect(m_worker, &FFmpegWorker::errorOccurred, this, &FFmpegFrameExtractor::errorOccurred, Qt::QueuedConnection);

    m_workerThread->start();
}

FFmpegFrameExtractor::~FFmpegFrameExtractor() {
    m_worker->stop();
    m_workerThread->quit();
    m_workerThread->wait();
}

void FFmpegFrameExtractor::openFile(const QString &filePath) {
    m_worker->setFilePath(filePath);
}

void FFmpegFrameExtractor::requestFrame(qint64 timestampMs) {
    m_worker->requestFrame(timestampMs);
}
