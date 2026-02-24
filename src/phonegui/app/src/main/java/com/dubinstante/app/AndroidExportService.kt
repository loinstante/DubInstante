package com.dubinstante.app

import android.content.Context
import android.util.Log
import com.arthenica.ffmpegkit.FFmpegKit
import com.arthenica.ffmpegkit.ReturnCode
import java.io.File

import android.net.Uri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
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

        // 1. Copy Content URI to a temp file because FFmpegKit needs a raw file path
        val tempSourceFile = File(context.cacheDir, "temp_source_video.mp4")
        withContext(Dispatchers.IO) {
            val inputStream: InputStream? = context.contentResolver.openInputStream(sourceVideoUri)
            val outputStream = FileOutputStream(tempSourceFile)
            inputStream?.copyTo(outputStream)
            inputStream?.close()
            outputStream.close()
        }
        
        val sourceVideoPath = tempSourceFile.absolutePath

        // Construct identical ffmpeg command logic to desktop:
        // Strip out existing audio, add new audio, map shortest
        // -i source_video -i recorded_audio -c:v copy -c:a aac -map 0:v:0 -map 1:a:0 -shortest
        val command = "-y -i \"$sourceVideoPath\" -i \"$recordedAudioPath\" -c:v copy -c:a aac -map 0:v:0 -map 1:a:0 -shortest \"${outputFile.absolutePath}\""

        Log.i("AndroidExportService", "Executing FFmpeg: $command")
        
        FFmpegKit.executeAsync(command, { session ->
            val returnCode = session.returnCode
            if (ReturnCode.isSuccess(returnCode)) {
                Log.i("AndroidExportService", "Export successful: ${outputFile.absolutePath}")
                onComplete(true, "Export completed successfully", outputFile.absolutePath)
            } else if (ReturnCode.isCancel(returnCode)) {
                Log.w("AndroidExportService", "Export cancelled")
                onComplete(false, "Export cancelled", null)
            } else {
                val errorLog = session.failStackTrace
                Log.e("AndroidExportService", "Export failed with return code $returnCode. Log: $errorLog")
                onComplete(false, "Export failed (Code $returnCode)", null)
            }
        }, { log ->
            // Logging progress would involve parsing FFmpeg logs
            Log.d("AndroidExportService", log.message)
        }, { statistics ->
            // Minimal progress event emulation map
            val timeInMilliseconds = statistics.time
            // Just emitting a heartbeat progress for UI
            if (timeInMilliseconds > 0) {
                onProgress((timeInMilliseconds % 100).toInt()) 
            }
        })
    }
}
