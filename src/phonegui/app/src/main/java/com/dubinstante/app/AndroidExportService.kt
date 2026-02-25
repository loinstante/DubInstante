package com.dubinstante.app

import android.content.Context
import android.net.Uri
import android.util.Log
import io.microshow.rxffmpeg.RxFFmpegInvoke
import io.microshow.rxffmpeg.RxFFmpegSubscriber
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class AndroidExportService(private val context: Context) {

    suspend fun exportVideo(
            sourceVideoUri: Uri,
            recordedAudioPath: String,
            targetUri: Uri,
            mediaVolume: Float,
            micVolume: Float,
            recordDurationMs: Long,
            onProgress: (Int) -> Unit,
            onComplete: (Boolean, String, String?) -> Unit
    ) {
        val outputFile =
                File(context.cacheDir, "dubinstante_export_temp_${System.currentTimeMillis()}.mp4")

        // 1. Copy Content URI to a temp file
        val tempSourceFile = File(context.cacheDir, "temp_source_video.mp4")
        withContext(Dispatchers.IO) {
            val inputStream: InputStream? = context.contentResolver.openInputStream(sourceVideoUri)
            val outputStream = FileOutputStream(tempSourceFile)
            inputStream?.copyTo(outputStream)
            inputStream?.close()
            outputStream.close()
        }

        val sourceVideoPath = tempSourceFile.absolutePath

        val durationSeconds = recordDurationMs / 1000.0

        // 2. Construct identical ffmpeg command logic to desktop as an exact array of arguments
        // We use a filter_complex with amix to mix the original video's audio [0:a] and the mic
        // [1:a]
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
                        "[0:a]volume=${mediaVolume}[a0];[1:a]volume=${micVolume}[a1];[a0][a1]amix=inputs=2:duration=longest[aout]",
                        "-map",
                        "0:v:0",
                        "-map",
                        "[aout]",
                        "-c:v",
                        "copy",
                        "-c:a",
                        "aac",
                        "-b:a",
                        "192k"
                )

        if (durationSeconds > 0) {
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
                                        onComplete(
                                                false,
                                                "Failed to open output stream for selected location",
                                                null
                                        )
                                    }
                                } catch (e: Exception) {
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
                                onComplete(false, "Export cancelled", null)
                            }

                            override fun onError(message: String?) {
                                Log.e("AndroidExportService", "Export failed. Message: $message")
                                onComplete(false, "Export failed ($message)", null)
                            }
                        }
                )
    }
}
