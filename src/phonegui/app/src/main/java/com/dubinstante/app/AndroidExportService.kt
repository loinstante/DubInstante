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
        onProgress: (Int) -> Unit,
        onComplete: (Boolean, String, String?) -> Unit
    ) {
        val outputDir = File(context.getExternalFilesDir(null), "Exports")
        if (!outputDir.exists()) {
            outputDir.mkdirs()
        }
        val outputFile = File(outputDir, "dubinstante_export_${System.currentTimeMillis()}.mp4")

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

        // 2. Construct identical ffmpeg command logic to desktop
        val command = "ffmpeg -y -i \"$sourceVideoPath\" -i \"$recordedAudioPath\" -c:v copy -c:a aac -map 0:v:0 -map 1:a:0 -shortest \"${outputFile.absolutePath}\""
        val commandArgs = command.split(" ").toTypedArray()

        Log.i("AndroidExportService", "Executing FFmpeg: $command")
        
        RxFFmpegInvoke.getInstance().runCommandAsync(commandArgs, object : RxFFmpegSubscriber() {
            override fun onFinish() {
                Log.i("AndroidExportService", "Export successful: ${outputFile.absolutePath}")
                onComplete(true, "Export completed successfully", outputFile.absolutePath)
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
