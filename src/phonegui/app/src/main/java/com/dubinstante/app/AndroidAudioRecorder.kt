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

    fun startRecording() {
        try {
            audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            requestAudioFocus()

            outputFile =
                    File(context.cacheDir, "dubinstante_voice_${System.currentTimeMillis()}.m4a")

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
            abandonAudioFocus()
            Log.i("AndroidAudioRecorder", "Recording stopped")
        } catch (e: Exception) {
            Log.e("AndroidAudioRecorder", "Failed to stop recording", e)
        }
    }

    private fun requestAudioFocus() {
        audioManager?.let { am ->
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                audioFocusRequest =
                        AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE)
                                .setAudioAttributes(
                                        AudioAttributes.Builder()
                                                .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                                                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                                                .build()
                                )
                                .setAcceptsDelayedFocusGain(false)
                                .setOnAudioFocusChangeListener { /* Handle focus changes if necessary */
                                }
                                .build()
                audioFocusRequest?.let { request -> am.requestAudioFocus(request) }
            } else {
                @Suppress("DEPRECATION")
                am.requestAudioFocus(
                        {},
                        AudioManager.STREAM_VOICE_CALL,
                        AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE
                )
            }
            Log.i("AndroidAudioRecorder", "Audio focus requested for recording")
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
