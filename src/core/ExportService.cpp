/**
 * @file ExportService.cpp
 * @brief Implementation of the ExportService class.
 */

#include "ExportService.h"

#include <QDebug>
#include <QFile>
#include <QRegularExpression>

ExportService::ExportService(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_totalDurationMs(0)
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
    check.waitForFinished(3000);
    return (check.exitCode() == 0);
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
    emit progressChanged(0);
    
    QStringList args = buildFFmpegArgs(config);
    
    qDebug() << "[ExportService] Starting FFmpeg with args:" << args;
    m_process->start("ffmpeg", args);
}

void ExportService::cancelExport()
{
    if (isExporting()) {
        m_process->kill();
        emit exportFinished(false, "Export annulé par l'utilisateur.");
    }
}

// =============================================================================
// Private Slots
// =============================================================================

void ExportService::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        emit progressChanged(100);
        emit exportFinished(true, "Export réussi !");
    } else {
        QString error = m_process->readAllStandardError();
        emit exportFinished(false, "Échec de l'export: " + error);
    }
}

void ExportService::handleProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        emit exportFinished(false, "FFmpeg n'a pas pu démarrer. Est-il installé ?");
    } else {
        emit exportFinished(false, "Erreur lors de l'exécution de FFmpeg.");
    }
}

void ExportService::parseProgressOutput()
{
    if (m_totalDurationMs <= 0) {
        return;
    }
    
    QString output = m_process->readAllStandardError();
    qDebug() << "[FFmpeg]" << output;
    
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
    
    if (!config.secondAudioPath.isEmpty() && !QFile::exists(config.secondAudioPath)) {
        errorMessage = "Erreur: L'enregistrement de la Piste 2 est introuvable.";
        return false;
    }
    
    if (config.outputPath.isEmpty()) {
        errorMessage = "Erreur: Chemin de sortie non spécifié.";
        return false;
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
    if (config.startTimeMs > 0) {
        args << "-ss" << QString::number(config.startTimeMs / 1000.0, 'f', 3);
    }
    
    // Input files
    args << "-i" << config.videoPath;   // [0]
    args << "-i" << config.audioPath;   // [1]
    
    bool hasSecondTrack = !config.secondAudioPath.isEmpty();
    if (hasSecondTrack) {
        args << "-i" << config.secondAudioPath;  // [2]
    }
    
    // Video encoding: High quality H.264
    args << "-c:v" << "libx264";
    args << "-preset" << "superfast";
    args << "-crf" << "18";
    args << "-pix_fmt" << "yuv420p";
    
    // Build audio filter complex
    QString filterComplex;
    bool includeOriginal = (config.originalVolume >= 0.01f);
    
    if (includeOriginal) {
        filterComplex += QString("[0:a]volume=%1[a0];").arg(config.originalVolume);
    }
    
    filterComplex += "[1:a]volume=1.0[a1];";
    
    if (hasSecondTrack) {
        filterComplex += "[2:a]volume=1.0[a2];";
    }
    
    // AMIX: combine all audio streams
    QString inputsStr;
    if (includeOriginal) inputsStr += "[a0]";
    inputsStr += "[a1]";
    if (hasSecondTrack) inputsStr += "[a2]";
    
    int amixInputs = (includeOriginal ? 1 : 0) + 1 + (hasSecondTrack ? 1 : 0);
    filterComplex += inputsStr + QString("amix=inputs=%1:duration=longest[aout]").arg(amixInputs);
    
    args << "-filter_complex" << filterComplex;
    args << "-map" << "0:v:0";
    args << "-map" << "[aout]";
    args << "-c:a" << "aac";
    args << "-b:a" << "192k";
    
    // Duration limit
    if (config.durationMs > 0) {
        args << "-t" << QString::number(config.durationMs / 1000.0, 'f', 3);
    } else {
        args << "-shortest";
    }
    
    args << config.outputPath;
    
    return args;
}
