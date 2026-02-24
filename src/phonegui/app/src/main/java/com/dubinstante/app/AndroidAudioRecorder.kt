package com.dubinstante.app

import android.content.Context
import android.media.MediaRecorder
import android.os.Build
import android.util.Log
import java.io.File
import java.io.IOException

class AndroidAudioRecorder(private val context: Context) {
    private var mediaRecorder: MediaRecorder? = null
    var outputFile: File? = null
        private set

    fun startRecording() {
        try {
            outputFile = File(context.cacheDir, "dubinstante_voice_${System.currentTimeMillis()}.m4a")

            mediaRecorder = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                MediaRecorder(context)
            } else {
                @Suppress("DEPRECATION")
                MediaRecorder()
            }.apply {
                setAudioSource(MediaRecorder.AudioSource.MIC)
                setOutputFormat(MediaRecorder.OutputFormat.MPEG_4)
                setAudioEncoder(MediaRecorder.AudioEncoder.AAC)
                setAudioEncodingBitRate(128000)
                setAudioSamplingRate(44100)
                setOutputFile(outputFile?.absolutePath)
                prepare()
                start()
            }
            Log.i("AndroidAudioRecorder", "Recording started to ${outputFile?.absolutePath}")
        } catch (e: IOException) {
            Log.e("AndroidAudioRecorder", "MediaRecorder prepare() failed", e)
        } catch (e: Exception) {
            Log.e("AndroidAudioRecorder", "Failed to start recording", e)
        }
    }

    fun stopRecording() {
        try {
            mediaRecorder?.apply {
                stop()
                release()
            }
            mediaRecorder = null
            Log.i("AndroidAudioRecorder", "Recording stopped")
        } catch (e: Exception) {
            Log.e("AndroidAudioRecorder", "Failed to stop recording", e)
        }
    }
}
