/**
 * @file ExportService.cpp
 * @brief Implementation of the ExportService class.
 */

#include "ExportService.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTemporaryFile>
#include <limits>

ExportService::ExportService(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_totalDurationMs(0)
    , m_exportFinishedEmitted(false)
    , m_outputExistedBefore(false)
{
    connect(m_process, &QProcess::finished,
            this, &ExportService::handleProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &ExportService::handleProcessError);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &ExportService::parseProgressOutput);
}

ExportService::~ExportService()
{
    if (!isExporting()) {
        return;
    }
    // Destroyed mid-export (application closed): the receivers of our signals may already
    // be half-destroyed, so stop listening before killing, then drop the truncated file.
    m_process->disconnect(this);
    m_process->kill();
    m_process->waitForFinished(3000);
    removePartialOutput();
}

// =============================================================================
// Public Methods
// =============================================================================

QString ExportService::toolPath(const QString &name)
{
    // findExecutable() and not exists(): it appends the .exe on Windows and
    // checks the executable bit, which a plain path test would not.
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString bundled = QStandardPaths::findExecutable(
        name, {appDir, appDir + "/../Resources"});
    if (!bundled.isEmpty()) {
        return bundled;
    }
    return QStandardPaths::findExecutable(name);
}

bool ExportService::isFFmpegAvailable(QString *errorMessage)
{
    const bool ffmpegFound = !toolPath("ffmpeg").isEmpty();
    const bool ffprobeFound = !toolPath("ffprobe").isEmpty();

    if (ffmpegFound && ffprobeFound) {
        return true;
    }

    if (errorMessage) {
        const QString toolName = !ffmpegFound ? "FFmpeg" : "FFprobe";
        *errorMessage = ExportService::tr(
            "%1 was not found on this system. It is required to export.\n\n"
            "Debian / Ubuntu: sudo apt install ffmpeg\n"
            "Fedora: sudo dnf install ffmpeg\n"
            "Arch: sudo pacman -S ffmpeg\n"
            "macOS (Homebrew): brew install ffmpeg\n"
            "Windows: https://ffmpeg.org/download.html").arg(toolName);
    }

    return false;
}

bool ExportService::isExporting() const
{
    return m_process->state() != QProcess::NotRunning;
}

void ExportService::startExport(const ExportConfig &config)
{
    // Check if already running (no signal: it would wipe the running export's progress UI)
    if (isExporting()) {
        qWarning() << "[ExportService] startExport ignored: an export is already running";
        return;
    }
    
    // Validate configuration
    QString errorMessage;
    if (!validateConfig(config, errorMessage)) {
        emit exportFinished(false, errorMessage);
        return;
    }
    
    m_totalDurationMs = config.durationMs;
    m_currentOutputPath = config.outputPath;
    m_exportFinishedEmitted = false;
    m_errorAccumulator.clear();

    QFileInfo outputInfo(m_currentOutputPath);
    m_outputExistedBefore = outputInfo.exists();
    m_outputMTimeBefore = m_outputExistedBefore ? outputInfo.lastModified() : QDateTime();

    emit progressChanged(0);

    // Empty when nothing was found: QProcess then fails to start, which is the
    // same path as a missing ffmpeg and is already reported to the user.
    const QString program = toolPath("ffmpeg");

    // Kept until the next export: ffmpeg reads it while the process runs
    m_filterScript = std::make_unique<QTemporaryFile>(
        QDir::temp().filePath("dubinstante_filter_XXXXXX.txt"));
    QStringList unused;
    if (!m_filterScript->open() ||
        m_filterScript->write(buildAudioGraph(config, &unused).toUtf8()) < 0 ||
        !m_filterScript->flush()) {
        emit exportFinished(false, tr("Error: could not write the temporary audio graph."));
        return;
    }
    m_filterScript->close();

    QStringList args = buildFFmpegArgs(config, m_filterScript->fileName(),
                                       ffmpegMajorVersion(program));
    qDebug() << "[ExportService] Starting FFmpeg:" << program << args;
    m_process->setStandardOutputFile(QProcess::nullDevice());
    m_process->start(program, args);
}

void ExportService::cancelExport()
{
    if (isExporting()) {
        m_exportFinishedEmitted = true;
        m_process->kill();
        emit exportFinished(false, tr("Export cancelled by the user."));
    }
}

// =============================================================================
// Private Slots
// =============================================================================

void ExportService::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_exportFinishedEmitted) {
        // Cancelled or already reported: the process is done now, drop the partial file
        removePartialOutput();
        return;
    }
    m_exportFinishedEmitted = true;

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        resetOutputTracking();
        emit progressChanged(100);
        emit exportFinished(true, tr("Export succeeded!"));
    } else {
        QString remaining = m_process->readAllStandardError();
        m_errorAccumulator.append(remaining);

        QString detailedError;
        if (m_errorAccumulator.contains("No space left on device", Qt::CaseInsensitive) ||
            m_errorAccumulator.contains("disk full", Qt::CaseInsensitive) ||
            m_errorAccumulator.contains("no space", Qt::CaseInsensitive)) {
            detailedError = tr("Not enough disk space on the destination drive.");
        } else {
            QStringList lines = m_errorAccumulator.split('\n', Qt::SkipEmptyParts);
            QStringList lastLines;
            int count = 0;
            for (int i = lines.size() - 1; i >= 0 && count < 3; --i) {
                QString line = lines[i].trimmed();
                if (!line.isEmpty() && !line.startsWith("frame=") && !line.startsWith("size=")) {
                    lastLines.prepend(line);
                    count++;
                }
            }
            if (!lastLines.isEmpty()) {
                detailedError = lastLines.join("\n");
            } else {
                detailedError = tr("Unknown FFmpeg error.");
            }
        }
        removePartialOutput();
        emit exportFinished(false, tr("Export failed: %1").arg(detailedError));
    }
}

void ExportService::handleProcessError(QProcess::ProcessError error)
{
    if (m_exportFinishedEmitted) {
        return;
    }
    m_exportFinishedEmitted = true;

    removePartialOutput();
    if (error == QProcess::FailedToStart) {
        emit exportFinished(false, tr("FFmpeg could not start. Is it installed?"));
    } else {
        emit exportFinished(false, tr("Error while running FFmpeg."));
    }
}

void ExportService::removePartialOutput()
{
    // Only remove what this run actually wrote: a file that pre-existed at the output
    // path (e.g. a previous successful export) and that ffmpeg never touched must survive.
    const QFileInfo fi(m_currentOutputPath);
    const bool wasWrittenByThisRun = !m_currentOutputPath.isEmpty() && fi.exists()
        && (!m_outputExistedBefore || fi.lastModified() != m_outputMTimeBefore);
    if (wasWrittenByThisRun) {
        QFile::remove(m_currentOutputPath);
    }

    // A crash emits both errorOccurred and finished, so this runs twice per export:
    // the second call must be a no-op, not treat the kept file as written by this run.
    resetOutputTracking();
}

void ExportService::resetOutputTracking()
{
    m_currentOutputPath.clear();
    m_outputExistedBefore = false;
    m_outputMTimeBefore = QDateTime();
}

void ExportService::parseProgressOutput()
{
    QString output = m_process->readAllStandardError();
    if (output.isEmpty()) {
        return;
    }

    m_errorAccumulator.append(output);
    // Limit memory usage (keep last 20KB)
    if (m_errorAccumulator.size() > 50000) {
        m_errorAccumulator = m_errorAccumulator.right(20000);
    }

    qDebug() << "[FFmpeg]" << output;

    if (m_totalDurationMs <= 0) {
        return;
    }
    
    // Parse time from FFmpeg output
    // Formats: time=00:00:00.00 or time=123.45
    static QRegularExpression reHMS("time=(\\d+):(\\d+):(\\d+)\\.(\\d+)");
    static QRegularExpression reSec("time=(\\d+)\\.(\\d+)");
    
    QRegularExpressionMatch match = reHMS.match(output);
    qint64 currentTimeMs = 0;
    
    if (match.hasMatch()) {
        int hours = match.captured(1).toInt();
        int mins = match.captured(2).toInt();
        int secs = match.captured(3).toInt();
        int centisecs = match.captured(4).toInt();
        currentTimeMs = (hours * 3600 + mins * 60 + secs) * 1000 + centisecs * 10;
    } else {
        match = reSec.match(output);
        if (match.hasMatch()) {
            currentTimeMs = match.captured(1).toLongLong() * 1000 +
                           match.captured(2).toInt() * 10;
        }
    }
    
    if (currentTimeMs > 0) {
        int percentage = static_cast<int>((currentTimeMs * 100) / m_totalDurationMs);
        percentage = qBound(0, percentage, 100);
        emit progressChanged(percentage);
    }
}

// =============================================================================
// Private Methods
// =============================================================================

bool ExportService::validateConfig(const ExportConfig &config, QString &errorMessage) const
{
    if (!QFile::exists(config.videoPath)) {
        errorMessage = tr("Error: the source video file was not found.");
        return false;
    }
    
    for (const ExportSegment &segment : config.segments) {
        if (!QFile::exists(segment.path)) {
            errorMessage = tr("Error: the audio recording was not found: %1")
                               .arg(segment.path);
            return false;
        }
    }
    
    if (config.outputPath.isEmpty()) {
        errorMessage = tr("Error: no output path given.");
        return false;
    }

    // Pre-check disk space
    QStorageInfo storage(QFileInfo(config.outputPath).absolutePath());
    if (storage.isValid() && storage.isReady()) {
        qint64 freeBytes = storage.bytesAvailable();
        if (freeBytes < 100LL * 1024 * 1024) { // Less than 100 MB
            errorMessage = tr("Error: critically low disk space (less than 100 MB free) on the destination drive.");
            return false;
        }
    }
    
    return true;
}

QList<ExportSegment> ExportService::clipSegments(const QList<ExportSegment> &segments,
                                                qint64 rangeStartMs, qint64 durationMs)
{
    const qint64 rangeEnd = durationMs > 0 ? rangeStartMs + durationMs
                                           : std::numeric_limits<qint64>::max();
    QList<ExportSegment> clipped;
    for (ExportSegment segment : segments) {
        const qint64 from = qMax(segment.timelineStartMs, rangeStartMs);
        const qint64 to = qMin(segment.timelineStartMs + segment.durationMs, rangeEnd);
        if (from >= to || segment.volume < 0.01f)
            continue;
        segment.sourceOffsetMs += from - segment.timelineStartMs;
        segment.timelineStartMs = from - rangeStartMs;
        segment.durationMs = to - from;
        clipped.append(segment);
    }
    return clipped;
}

QString ExportService::buildAudioGraph(const ExportConfig &config, QStringList *audioInputs)
{
    const auto seconds = [](qint64 ms) { return QString::number(ms / 1000.0, 'f', 3); };
    const QList<ExportSegment> segments =
        clipSegments(config.segments, config.rangeStartMs, config.durationMs);

    // One input per file: a take heard in several comp regions is split, not reopened
    audioInputs->clear();
    QList<int> inputOfSegment;
    QVector<int> usesPerInput;
    for (const ExportSegment &segment : segments) {
        int input = audioInputs->indexOf(segment.path);
        if (input < 0) {
            audioInputs->append(segment.path);
            usesPerInput.append(0);
            input = audioInputs->size() - 1;
        }
        ++usesPerInput[input];
        inputOfSegment.append(input);
    }

    QStringList chains;
    QStringList mixInputs;
    if (config.originalVolume >= 0.01f) {
        // [0:a] needs no shift: the input-side -ss already aligned it
        chains << QString("[0:a]volume=%1[orig]").arg(config.originalVolume);
        mixInputs << "[orig]";
    }

    QVector<QStringList> pads(audioInputs->size());
    for (int input = 0; input < audioInputs->size(); ++input) {
        const QString label = QString("[%1:a]").arg(input + 1);
        if (usesPerInput[input] == 1) {
            pads[input] << label;
            continue;
        }
        QString split = label + QString("asplit=%1").arg(usesPerInput[input]);
        for (int k = 0; k < usesPerInput[input]; ++k) {
            const QString pad = QString("[in%1_%2]").arg(input + 1).arg(k);
            split += pad;
            pads[input] << pad;
        }
        chains << split;
    }

    // amix discards input timestamps: each piece is placed with atrim + adelay
    for (int i = 0; i < segments.size(); ++i) {
        const ExportSegment &segment = segments[i];
        QString chain = pads[inputOfSegment[i]].takeFirst();
        chain += QString("atrim=start=%1:duration=%2,asetpts=PTS-STARTPTS")
                     .arg(seconds(segment.sourceOffsetMs), seconds(segment.durationMs));
        if (segment.timelineStartMs > 0)
            chain += QString(",adelay=%1:all=1").arg(segment.timelineStartMs);
        chain += QString(",volume=%1[seg%2]").arg(segment.volume).arg(i);
        chains << chain;
        mixInputs << QString("[seg%1]").arg(i);
    }

    if (mixInputs.isEmpty()) {
        // Everything muted: silence, cut by -t / -shortest like the mix would be
        chains << "anullsrc=r=48000:cl=stereo[aout]";
    } else {
        // normalize=0 (FFmpeg >= 4.4): keep user-set volumes, amix otherwise divides each input by N.
        // apad: the mix ends with the last take when the original audio is muted, and -shortest
        // would cut the video there; padding lets the video stream set the end.
        chains << mixInputs.join(QString()) +
                      QString("amix=inputs=%1:duration=longest:normalize=0,apad[aout]")
                          .arg(mixInputs.size());
    }
    return chains.join(";\n");
}

int ExportService::ffmpegMajorVersion(const QString &program)
{
    // Probed once per binary: startExport() runs on the GUI thread
    static QHash<QString, int> cache;
    if (cache.contains(program))
        return cache.value(program);

    QProcess probe;
    probe.start(program, {"-hide_banner", "-version"});
    if (!probe.waitForFinished(5000) || probe.exitStatus() != QProcess::NormalExit) {
        probe.kill();
        probe.waitForFinished(1000);
        return 0;   // not cached: a transient failure gets another chance
    }
    // "ffmpeg version 6.1.1-3ubuntu5", "ffmpeg version n7.1", "ffmpeg version N-118000-g..."
    const QRegularExpressionMatch match =
        QRegularExpression(R"(ffmpeg version (N-|n?(\d+)))")
            .match(QString::fromUtf8(probe.readAllStandardOutput()));
    const int major = !match.hasMatch() ? 0
                      : match.captured(1) == "N-" ? 99 : match.captured(2).toInt();
    cache.insert(program, major);
    return major;
}

QStringList ExportService::buildFFmpegArgs(const ExportConfig &config,
                                           const QString &filterScriptPath, int ffmpegMajor)
{
    QStringList args;

    // Overwrite output, use all threads
    args << "-y";
    args << "-threads" << "0";

    // Input-side seek: frame-accurate when re-encoding and resets output timestamps to zero
    if (config.rangeStartMs > 0) {
        args << "-ss" << QString::number(config.rangeStartMs / 1000.0, 'f', 3);
    }
    args << "-i" << config.videoPath;   // [0]

    QStringList audioInputs;
    buildAudioGraph(config, &audioInputs);
    for (const QString &input : audioInputs) {
        args << "-i" << input;          // [1..N]
    }

    if (config.expertMode) {
        // Video Codec
        args << "-c:v" << config.videoCodec;
        if (config.videoCodec != "copy") {
            if (!config.speedPreset.isEmpty()) {
                args << "-preset" << config.speedPreset;
            }
            if (config.videoBitrateMbps > 0) {
                args << "-b:v" << QString("%1M").arg(config.videoBitrateMbps);
            } else {
                args << "-crf" << QString::number(config.crf >= 0 ? config.crf : 21);
            }
            if (config.videoCodec == "prores") {
                args << "-pix_fmt" << "yuv422p";
            } else {
                args << "-pix_fmt" << "yuv420p";
            }
        }
    } else {
        // Video encoding (Standard)
        args << "-c:v" << "libx264";
        args << "-preset" << (config.speedPreset.isEmpty() ? "medium" : config.speedPreset);
        args << "-crf" << QString::number(config.crf >= 0 ? config.crf : 21);
        args << "-pix_fmt" << "yuv420p";
    }
    
    // Scale resolution if specified (-vf is incompatible with stream copy)
    bool videoCopy = config.expertMode && config.videoCodec == "copy";
    if (!config.scaleResolution.isEmpty() && !videoCopy) {
        args << "-vf" << QString("scale=%1").arg(config.scaleResolution);
    }
    
    // "-filter_complex_script" is deprecated since ffmpeg 7.0, which reads any
    // option value from a file with the "-/" prefix instead.
    args << (ffmpegMajor >= 7 ? "-/filter_complex" : "-filter_complex_script")
         << filterScriptPath;
    args << "-map" << "0:v:0";
    args << "-map" << "[aout]";
    
    if (config.expertMode) {
        // Audio Codec
        args << "-c:a" << config.audioCodec;
        if (config.audioCodec != "copy" && !config.audioCodec.startsWith("pcm")) {
            args << "-b:a" << QString("%1k").arg(config.audioBitrateKbps > 0 ? config.audioBitrateKbps : 192);
        }
        if (config.sampleRateHz > 0) {
            args << "-ar" << QString::number(config.sampleRateHz);
        }
    } else {
        // Select audio encoder based on format (Standard)
        QString fmt = config.format.toLower();
        if (fmt == "avi") {
            args << "-c:a" << "libmp3lame";
        } else {
            args << "-c:a" << "aac";
        }
        args << "-b:a" << QString("%1k").arg(config.audioBitrateKbps > 0 ? config.audioBitrateKbps : 192);
    }
    
    // Duration limit
    if (config.durationMs > 0) {
        args << "-t" << QString::number(config.durationMs / 1000.0, 'f', 3);
    } else {
        args << "-shortest";
    }
    
    // Custom FFmpeg Flags (Expert Mode)
    if (config.expertMode && !config.customFFmpegFlags.trimmed().isEmpty()) {
        QStringList customFlags = config.customFFmpegFlags.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        args << customFlags;
    }
    
    args << config.outputPath;
    
    return args;
}
