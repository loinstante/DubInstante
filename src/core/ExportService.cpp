/**
 * @file ExportService.cpp
 * @brief Implementation of the ExportService class.
 */

#include "ExportService.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>

ExportService::ExportService(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_totalDurationMs(0)
    , m_exportFinishedEmitted(false)
{
    connect(m_process, &QProcess::finished,
            this, &ExportService::handleProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &ExportService::handleProcessError);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &ExportService::parseProgressOutput);
}

// =============================================================================
// Public Methods
// =============================================================================

bool ExportService::isFFmpegAvailable() const
{
    QProcess check;
    check.start("ffmpeg", QStringList() << "-version");
    if (!check.waitForStarted(3000) || !check.waitForFinished(3000)) {
        return false;
    }
    return check.exitStatus() == QProcess::NormalExit && check.exitCode() == 0;
}

bool ExportService::isExporting() const
{
    return m_process->state() != QProcess::NotRunning;
}

void ExportService::startExport(const ExportConfig &config)
{
    // Check if already running
    if (isExporting()) {
        emit exportFinished(false, "Un export est déjà en cours.");
        return;
    }
    
    // Validate configuration
    QString errorMessage;
    if (!validateConfig(config, errorMessage)) {
        emit exportFinished(false, errorMessage);
        return;
    }
    
    m_totalDurationMs = config.durationMs;
    m_exportFinishedEmitted = false;
    m_errorAccumulator.clear();
    emit progressChanged(0);
    
    QStringList args = buildFFmpegArgs(config);
    
    qDebug() << "[ExportService] Starting FFmpeg with args:" << args;
    m_process->setStandardOutputFile(QProcess::nullDevice());
    m_process->start("ffmpeg", args);
}

void ExportService::cancelExport()
{
    if (isExporting()) {
        m_exportFinishedEmitted = true;
        m_process->kill();
        emit exportFinished(false, "Export annulé par l'utilisateur.");
    }
}

// =============================================================================
// Private Slots
// =============================================================================

void ExportService::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_exportFinishedEmitted) {
        return;
    }
    m_exportFinishedEmitted = true;

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        emit progressChanged(100);
        emit exportFinished(true, "Export réussi !");
    } else {
        QString remaining = m_process->readAllStandardError();
        m_errorAccumulator.append(remaining);

        QString detailedError;
        if (m_errorAccumulator.contains("No space left on device", Qt::CaseInsensitive) ||
            m_errorAccumulator.contains("disk full", Qt::CaseInsensitive) ||
            m_errorAccumulator.contains("no space", Qt::CaseInsensitive)) {
            detailedError = "Espace disque insuffisant sur le périphérique de destination.";
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
                detailedError = "Erreur inconnue de FFmpeg.";
            }
        }
        emit exportFinished(false, "Échec de l'export: " + detailedError);
    }
}

void ExportService::handleProcessError(QProcess::ProcessError error)
{
    if (m_exportFinishedEmitted) {
        return;
    }
    m_exportFinishedEmitted = true;

    if (error == QProcess::FailedToStart) {
        emit exportFinished(false, "FFmpeg n'a pas pu démarrer. Est-il installé ?");
    } else {
        emit exportFinished(false, "Erreur lors de l'exécution de FFmpeg.");
    }
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
        errorMessage = "Erreur: Le fichier vidéo source est introuvable.";
        return false;
    }
    
    if (!QFile::exists(config.audioPath)) {
        errorMessage = "Erreur: L'enregistrement de la Piste 1 est introuvable.";
        return false;
    }
    
    for (int i = 0; i < config.extraAudioPaths.size(); ++i) {
        if (!config.extraAudioPaths[i].isEmpty() && !QFile::exists(config.extraAudioPaths[i])) {
            errorMessage = QString("Erreur: L'enregistrement de la Piste %1 est introuvable.").arg(i + 2);
            return false;
        }
    }
    
    if (config.outputPath.isEmpty()) {
        errorMessage = "Erreur: Chemin de sortie non spécifié.";
        return false;
    }

    // Pre-check disk space
    QStorageInfo storage(QFileInfo(config.outputPath).absolutePath());
    if (storage.isValid() && storage.isReady()) {
        qint64 freeBytes = storage.bytesAvailable();
        if (freeBytes < 100LL * 1024 * 1024) { // Less than 100 MB
            errorMessage = "Erreur: Espace disque critique (moins de 100 Mo disponibles) sur le périphérique de destination.";
            return false;
        }
    }
    
    return true;
}

QStringList ExportService::buildFFmpegArgs(const ExportConfig &config) const
{
    QStringList args;
    
    // Overwrite output, use all threads
    args << "-y";
    args << "-threads" << "0";
    
    // Input seeking (fast seek)
    // NOTE: -ss is no longer used for start time because it trims the video.
    // Instead, we use -itsoffset for audio sync below.
    
    // Video Input
    args << "-i" << config.videoPath;   // [0]
    
    // Primary Audio Input
    if (config.trackOffsetsMs.size() > 0 && config.trackOffsetsMs[0] > 0) {
        args << "-itsoffset" << QString::number(config.trackOffsetsMs[0] / 1000.0, 'f', 3);
    }
    args << "-i" << config.audioPath;   // [1]
    
    // Add extra audio tracks: [2], [3], ...
    for (int i = 0; i < config.extraAudioPaths.size(); ++i) {
        const QString &extraPath = config.extraAudioPaths[i];
        if (!extraPath.isEmpty()) {
            if (config.trackOffsetsMs.size() > i + 1 && config.trackOffsetsMs[i + 1] > 0) {
                args << "-itsoffset" << QString::number(config.trackOffsetsMs[i + 1] / 1000.0, 'f', 3);
            }
            args << "-i" << extraPath;
        }
    }
    
    // Count total extra tracks actually added
    int extraCount = 0;
    for (const QString &extraPath : config.extraAudioPaths) {
        if (!extraPath.isEmpty()) extraCount++;
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
    
    // Scale resolution if specified
    if (!config.scaleResolution.isEmpty()) {
        args << "-vf" << QString("scale=%1").arg(config.scaleResolution);
    }
    
    // Build audio filter complex
    QString filterComplex;
    bool includeOriginal = (config.originalVolume >= 0.01f);
    
    if (includeOriginal) {
        filterComplex += QString("[0:a]volume=%1[a0];").arg(config.originalVolume);
    }
    
    // Primary track
    float primaryVol = (config.trackVolumes.size() > 0) ? config.trackVolumes[0] : 1.0f;
    filterComplex += QString("[1:a]volume=%1[a1];").arg(primaryVol);
    
    // Extra tracks: input indices start at 2
    for (int i = 0; i < extraCount; ++i) {
        float extraVol = (config.trackVolumes.size() > i + 1) ? config.trackVolumes[i + 1] : 1.0f;
        filterComplex += QString("[%1:a]volume=%2[a%3];").arg(i + 2).arg(extraVol).arg(i + 2);
    }
    
    // AMIX: combine all audio streams
    QString inputsStr;
    if (includeOriginal) inputsStr += "[a0]";
    inputsStr += "[a1]";
    for (int i = 0; i < extraCount; ++i) {
        inputsStr += QString("[a%1]").arg(i + 2);
    }
    
    int amixInputs = (includeOriginal ? 1 : 0) + 1 + extraCount;
    filterComplex += inputsStr + QString("amix=inputs=%1:duration=longest[aout]").arg(amixInputs);
    
    args << "-filter_complex" << filterComplex;
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
