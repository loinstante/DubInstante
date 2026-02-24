package com.dubinstante.app

import android.content.Context
import android.net.Uri
import android.util.Log
import io.microshow.rxffmpeg.RxFFmpegInvoke
import io.microshow.rxffmpeg.RxFFmpegSubscriber
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream

class AndroidExportService(private val context: Context) {

    suspend fun exportVideo(
        sourceVideoUri: Uri,
        recordedAudioPath: String,
        targetUri: Uri,
        onProgress: (Int) -> Unit,
        onComplete: (Boolean, String, String?) -> Unit
    ) {
        val outputFile = File(context.cacheDir, "dubinstante_export_temp_${System.currentTimeMillis()}.mp4")

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

        // 2. Construct identical ffmpeg command logic to desktop as an exact array of arguments
        // Strip out existing audio, add new audio, map shortest
        val commandArgs = arrayOf(
            "ffmpeg",
            "-y",
            "-i", sourceVideoPath,
            "-i", recordedAudioPath,
            "-c:v", "copy",
            "-c:a", "aac",
            "-map", "0:v:0",
            "-map", "1:a:0",
            "-shortest",
            outputFile.absolutePath
        )

        Log.i("AndroidExportService", "Executing FFmpeg: ${commandArgs.joinToString(" ")}")
        
        RxFFmpegInvoke.getInstance().runCommandAsync(commandArgs, object : RxFFmpegSubscriber() {
            override fun onFinish() {
                try {
                    val outputStream = context.contentResolver.openOutputStream(targetUri)
                    if (outputStream != null) {
                        outputFile.inputStream().use { input ->
                            outputStream.use { output ->
                                input.copyTo(output)
                            }
                        }
                        outputFile.delete() // Cleanup temp file
                        Log.i("AndroidExportService", "Export copied successfully to user Uri")
                        onComplete(true, "Export completed successfully", null)
                    } else {
                        onComplete(false, "Failed to open output stream for selected location", null)
                    }
                } catch (e: Exception) {
                    Log.e("AndroidExportService", "Failed to copy export to target Uri", e)
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
        })
    }
}
