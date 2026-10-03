package com.dubinstante.app

import android.content.Context
import android.net.Uri
import android.media.MediaExtractor
import android.media.MediaFormat
import android.os.StatFs
import android.util.Log
import io.microshow.rxffmpeg.RxFFmpegInvoke
import io.microshow.rxffmpeg.RxFFmpegSubscriber
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class AndroidExportService(private val context: Context) {

    private fun detectHasAudioTrack(videoUri: Uri): Boolean {
        val extractor = MediaExtractor()
        return try {
            extractor.setDataSource(context, videoUri, null)
            for (i in 0 until extractor.trackCount) {
                val format = extractor.getTrackFormat(i)
                val mime = format.getString(MediaFormat.KEY_MIME) ?: ""
                if (mime.startsWith("audio/")) {
                    return true
                }
            }
            false
        } catch (e: Exception) {
            Log.w("AndroidExportService", "Failed to inspect audio tracks via MediaExtractor", e)
            true // default to true if probing fails
        } finally {
            extractor.release()
        }
    }

    suspend fun exportVideo(
            sourceVideoUri: Uri,
            recordedAudioPath: String,
            targetUri: Uri,
            mediaVolume: Float,
            micVolume: Float,
            recordDurationMs: Long = 0L,
            exportFullVideo: Boolean = true,
            onProgress: (Int) -> Unit,
            onComplete: (Boolean, String, String?) -> Unit
    ) {
        val outputFile =
                File(context.cacheDir, "dubinstante_export_temp_${System.currentTimeMillis()}.mp4")

        // 0. Pre-check cache space: source copy + encoded output ~= 2x source size
        val sourceSize =
                withContext(Dispatchers.IO) {
                    try {
                        context.contentResolver.openAssetFileDescriptor(sourceVideoUri, "r")?.use {
                            it.length
                        }
                                ?: -1L
                    } catch (e: Exception) {
                        -1L
                    }
                }
        if (sourceSize > 0 && StatFs(context.cacheDir.path).availableBytes < 2 * sourceSize) {
            onComplete(false, "Espace de stockage insuffisant pour préparer l'export", null)
            return
        }

        // 1. Copy Content URI to a temp file
        val tempSourceFile = File(context.cacheDir, "temp_source_video_${System.currentTimeMillis()}.mp4")
        val sourceCopied =
                withContext(Dispatchers.IO) {
                    try {
                        val inputStream: InputStream? =
                                context.contentResolver.openInputStream(sourceVideoUri)
                        if (inputStream == null) {
                            false
                        } else {
                            inputStream.use { input ->
                                FileOutputStream(tempSourceFile).use { output ->
                                    input.copyTo(output)
                                }
                            }
                            true
                        }
                    } catch (e: Exception) {
                        Log.e("AndroidExportService", "Failed to copy source video", e)
                        false
                    }
                }
        if (!sourceCopied) {
            tempSourceFile.delete()
            onComplete(false, "Impossible de lire la vidéo source", null)
            return
        }

        val sourceVideoPath = tempSourceFile.absolutePath

        // 2. Check if source video contains an audio stream
        val sourceHasAudio = withContext(Dispatchers.IO) {
            detectHasAudioTrack(sourceVideoUri)
        }
        Log.i("AndroidExportService", "Source video has audio stream: $sourceHasAudio")

        // 3. Build filter graph adapted to stream presence
        val filterGraph = when {
            sourceHasAudio && mediaVolume > 0.01f -> {
                "[0:a]volume=${mediaVolume}[a0];[1:a]volume=${micVolume}[a1];[a0][a1]amix=inputs=2:duration=longest:dropout_transition=0,apad[aout]"
            }
            sourceHasAudio && mediaVolume <= 0.01f -> {
                "[1:a]volume=${micVolume},apad[aout]"
            }
            else -> {
                // Video has no audio track: mix mic only with apad to match video duration
                "[1:a]volume=${micVolume},apad[aout]"
            }
        }

        val commandArgs =
                mutableListOf(
                        "ffmpeg",
                        "-y",
                        "-threads",
                        "0",
                        "-i",
                        sourceVideoPath,
                        "-i",
                        recordedAudioPath,
                        "-filter_complex",
                        filterGraph,
                        "-map",
                        "0:v:0",
                        "-map",
                        "[aout]",
                        "-c:v",
                        "copy",
                        "-c:a",
                        "aac",
                        "-b:a",
                        "192k",
                        "-shortest"
                )

        if (!exportFullVideo && recordDurationMs > 0) {
            val durationSeconds = recordDurationMs / 1000.0
            commandArgs.add("-t")
            commandArgs.add(String.format(java.util.Locale.US, "%.3f", durationSeconds))
        }

        commandArgs.add(outputFile.absolutePath)

        val argsArray = commandArgs.toTypedArray()
        Log.i("AndroidExportService", "Executing FFmpeg: ${argsArray.joinToString(" ")}")

        RxFFmpegInvoke.getInstance()
                .runCommandAsync(
                        argsArray,
                        object : RxFFmpegSubscriber() {
                            override fun onFinish() {
                                tempSourceFile.delete()
                                try {
                                    val outputStream =
                                            context.contentResolver.openOutputStream(targetUri)
                                    if (outputStream != null) {
                                        outputFile.inputStream().use { input ->
                                            outputStream.use { output -> input.copyTo(output) }
                                        }
                                        outputFile.delete() // Cleanup temp file
                                        Log.i(
                                                "AndroidExportService",
                                                "Export copied successfully to user Uri"
                                        )
                                        onComplete(true, "Export completed successfully", null)
                                    } else {
                                        outputFile.delete()
                                        onComplete(
                                                false,
                                                "Failed to open output stream for selected location",
                                                null
                                        )
                                    }
                                } catch (e: Exception) {
                                    outputFile.delete()
                                    Log.e(
                                            "AndroidExportService",
                                            "Failed to copy export to target Uri",
                                            e
                                    )
                                    onComplete(false, "Failed to copy export: ${e.message}", null)
                                }
                            }

                            override fun onProgress(progress: Int, progressTime: Long) {
                                onProgress(progress)
                            }

                            override fun onCancel() {
                                Log.w("AndroidExportService", "Export cancelled")
                                tempSourceFile.delete()
                                outputFile.delete()
                                onComplete(false, "Export cancelled", null)
                            }

                            override fun onError(message: String?) {
                                Log.e("AndroidExportService", "Export failed. Message: $message")
                                tempSourceFile.delete()
                                outputFile.delete()
                                onComplete(false, "Export failed ($message)", null)
                            }
                        }
                )
    }
}
