package com.dubinstante.app

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.media.MediaRecorder
import android.os.Build
import android.util.Log
import java.io.File
import java.io.IOException

class AndroidAudioRecorder(private val context: Context) {
    private var mediaRecorder: MediaRecorder? = null
    private var audioManager: AudioManager? = null
    private var audioFocusRequest: AudioFocusRequest? = null
    var outputFile: File? = null
        private set
    var recordingStartEpochMs: Long = 0L
        private set
    var lastRecordedDurationMs: Long = 0L
        private set

    fun startRecording(): Boolean {
        return try {
            audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            requestAudioFocus()

            outputFile =
                    File(context.cacheDir, "dubinstante_voice_${System.currentTimeMillis()}.m4a")

            recordingStartEpochMs = System.currentTimeMillis()

            mediaRecorder =
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                                MediaRecorder(context)
                            } else {
                                @Suppress("DEPRECATION") MediaRecorder()
                            }
                            .apply {
                                setAudioSource(MediaRecorder.AudioSource.MIC)
                                setOutputFormat(MediaRecorder.OutputFormat.MPEG_4)
                                setAudioEncoder(MediaRecorder.AudioEncoder.AAC)
                                setAudioEncodingBitRate(192000)
                                setAudioSamplingRate(48000)
                                setOutputFile(outputFile?.absolutePath)
                                prepare()
                                start()
                            }
            Log.i("AndroidAudioRecorder", "Recording started to ${outputFile?.absolutePath}")
            true
        } catch (e: IOException) {
            Log.e("AndroidAudioRecorder", "MediaRecorder prepare() failed", e)
            abandonAudioFocus()
            false
        } catch (e: Exception) {
            Log.e("AndroidAudioRecorder", "Failed to start recording", e)
            abandonAudioFocus()
            false
        }
    }

    fun stopRecording(): Long {
        lastRecordedDurationMs = if (recordingStartEpochMs > 0L) {
            System.currentTimeMillis() - recordingStartEpochMs
        } else {
            0L
        }

        try {
            mediaRecorder?.apply {
                try {
                    stop()
                } catch (e: RuntimeException) {
                    Log.w("AndroidAudioRecorder", "MediaRecorder stop failed (possibly too short)", e)
                }
                release()
            }
            mediaRecorder = null
            abandonAudioFocus()
            Log.i("AndroidAudioRecorder", "Recording stopped. Duration: $lastRecordedDurationMs ms")
        } catch (e: Exception) {
            Log.e("AndroidAudioRecorder", "Failed to properly release recorder", e)
        }
        return lastRecordedDurationMs
    }

    private fun requestAudioFocus() {
        audioManager?.let { am ->
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                audioFocusRequest =
                        AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT)
                                .setAudioAttributes(
                                        AudioAttributes.Builder()
                                                .setUsage(AudioAttributes.USAGE_MEDIA)
                                                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                                                .build()
                                )
                                .setAcceptsDelayedFocusGain(false)
                                .setOnAudioFocusChangeListener { /* Handle focus changes if necessary */ }
                                .build()
                audioFocusRequest?.let { request -> am.requestAudioFocus(request) }
            } else {
                @Suppress("DEPRECATION")
                am.requestAudioFocus(
                        {},
                        AudioManager.STREAM_MUSIC,
                        AudioManager.AUDIOFOCUS_GAIN_TRANSIENT
                )
            }
            Log.i("AndroidAudioRecorder", "Media audio focus requested for recording")
        }
    }

    private fun abandonAudioFocus() {
        audioManager?.let { am ->
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                audioFocusRequest?.let { request ->
                    am.abandonAudioFocusRequest(request)
                    audioFocusRequest = null
                }
            } else {
                @Suppress("DEPRECATION") am.abandonAudioFocus {}
            }
            Log.i("AndroidAudioRecorder", "Audio focus abandoned")
        }
    }
}
