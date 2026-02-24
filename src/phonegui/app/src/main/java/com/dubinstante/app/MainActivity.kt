package com.dubinstante.app

import android.Manifest
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.ContextCompat
import androidx.media3.common.MediaItem
import androidx.media3.common.Player
import androidx.media3.exoplayer.ExoPlayer
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    private val nativeBridge = NativeBridge()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Initialize JNI C++ Core integration
        nativeBridge.initialize()

        setContent {
            MaterialTheme(
                colorScheme = darkColorScheme()
            ) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    var selectedVideoUri by remember { mutableStateOf<String?>(null) }
                    var volume by remember { mutableStateOf(1.0f) }
                    var currentPositionMs by remember { mutableStateOf(0L) }
                    
                    // State linked to C++ via NativeBridge
                    var rythmoText by remember { mutableStateOf("") }
                    var rythmoSpeed by remember { mutableStateOf(100f) }

                    val context = LocalContext.current

                    // Recording & Export State
                    val audioRecorder = remember { AndroidAudioRecorder(context) }
                    val exportService = remember { AndroidExportService(context) }
                    var isRecording by remember { mutableStateOf(false) }
                    var isExporting by remember { mutableStateOf(false) }
                    var exportProgress by remember { mutableStateOf(0) }
                    var pendingExportAudioPath by remember { mutableStateOf<String?>(null) }
                    val coroutineScope = rememberCoroutineScope()

                    val saveVideoLauncher = rememberLauncherForActivityResult(
                        contract = ActivityResultContracts.CreateDocument("video/mp4")
                    ) { uri: Uri? ->
                        if (uri != null && selectedVideoUri != null && pendingExportAudioPath != null) {
                            isExporting = true
                            exportProgress = 0
                            coroutineScope.launch {
                                exportService.exportVideo(
                                    Uri.parse(selectedVideoUri),
                                    pendingExportAudioPath!!,
                                    uri,
                                    onProgress = { exportProgress = it },
                                    onComplete = { success, msg, _ ->
                                        isExporting = false
                                        pendingExportAudioPath = null
                                        coroutineScope.launch(Dispatchers.Main) {
                                            if (success) {
                                                Toast.makeText(context, "Exported successfully", Toast.LENGTH_LONG).show()
                                            } else {
                                                Toast.makeText(context, "Export Failed: $msg", Toast.LENGTH_LONG).show()
                                            }
                                        }
                                    }
                                )
                            }
                        } else {
                            pendingExportAudioPath = null
                        }
                    }

                    // Initialize state from JNI on first load
                    LaunchedEffect(Unit) {
                        rythmoText = nativeBridge.getRythmoText()
                        rythmoSpeed = nativeBridge.getRythmoSpeed().toFloat()
                    }
                    
                    val exoPlayer = remember {
                        ExoPlayer.Builder(context).build().apply {
                            playWhenReady = true
                        }
                    }

                    val handleStopRecording = {
                        if (isRecording) {
                            exoPlayer.pause()
                            audioRecorder.stopRecording()
                            isRecording = false
                            
                            val audioPath = audioRecorder.outputFile?.absolutePath
                            if (selectedVideoUri != null && audioPath != null) {
                                pendingExportAudioPath = audioPath
                                saveVideoLauncher.launch("dubinstante_export_${System.currentTimeMillis()}.mp4")
                            }
                        }
                    }
                    val handleStopRecordingState by rememberUpdatedState(handleStopRecording)

                    DisposableEffect(exoPlayer) {
                        val listener = object : Player.Listener {
                            override fun onPlaybackStateChanged(playbackState: Int) {
                                if (playbackState == Player.STATE_ENDED) {
                                    handleStopRecordingState()
                                }
                            }
                        }
                        exoPlayer.addListener(listener)
                        onDispose {
                            exoPlayer.removeListener(listener)
                            exoPlayer.release()
                        }
                    }

                    LaunchedEffect(exoPlayer) {
                        while (true) {
                            if (exoPlayer.isPlaying) {
                                currentPositionMs = exoPlayer.currentPosition
                            }
                            kotlinx.coroutines.delay(16) // ~60fps sync
                        }
                    }

                    val micPermissionLauncher = rememberLauncherForActivityResult(
                        contract = ActivityResultContracts.RequestPermission()
                    ) { isGranted ->
                        if (isGranted) {
                            exoPlayer.seekTo(0)
                            currentPositionMs = 0
                            audioRecorder.startRecording()
                            exoPlayer.play()
                            isRecording = true
                        } else {
                            Toast.makeText(context, "Microphone permission required to record", Toast.LENGTH_SHORT).show()
                        }
                    }

                    val videoPickerLauncher = rememberLauncherForActivityResult(
                        contract = ActivityResultContracts.OpenDocument()
                    ) { uri: Uri? ->
                        uri?.let {
                            selectedVideoUri = it.toString()
                            // Pass the path to the C++ Core via JNI Bridge
                            nativeBridge.openVideo(it.toString())
                            
                            val mediaItem = MediaItem.fromUri(uri)
                            exoPlayer.setMediaItem(mediaItem)
                            exoPlayer.prepare()
                            exoPlayer.play()
                        }
                    }

                    Column(
                        modifier = Modifier.fillMaxSize(),
                        horizontalAlignment = Alignment.CenterHorizontally
                    ) {
                        Spacer(modifier = Modifier.height(16.dp))
                        
                        // Header with Logo
                        Row(
                            modifier = Modifier.padding(16.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Image(
                                painter = painterResource(id = R.drawable.logo),
                                contentDescription = "App Logo",
                                modifier = Modifier.size(48.dp)
                            )
                            Spacer(modifier = Modifier.width(16.dp))
                            Text(
                                text = "DubInstante Mobile",
                                fontSize = 24.sp,
                                fontWeight = FontWeight.Bold,
                                color = MaterialTheme.colorScheme.primary
                            )
                        }

                        Spacer(modifier = Modifier.height(16.dp))

                        // Video Player (98% width, 16:9 aspect ratio standard constraint)
                        Box(
                            modifier = Modifier
                                .fillMaxWidth(0.98f)
                                .aspectRatio(16f / 9f)
                        ) {
                            if (selectedVideoUri != null) {
                                VideoPlayer(
                                    exoPlayer = exoPlayer,
                                    modifier = Modifier.fillMaxSize()
                                )
                            }
                        }

                        Spacer(modifier = Modifier.height(8.dp))

                        // Rythmo Band (98% width, placed directly under the video player)
                        RythmoBand(
                            text = rythmoText,
                            onTextChanged = { newText ->
                                rythmoText = newText
                                nativeBridge.setRythmoText(newText)
                            },
                            currentPositionMs = currentPositionMs,
                            speedPixelsPerSecond = rythmoSpeed.toInt(),
                            onSeekRequested = { newMs ->
                                exoPlayer.seekTo(newMs)
                                currentPositionMs = newMs
                            },
                            modifier = Modifier
                                .fillMaxWidth(0.98f)
                                .height(120.dp)
                        )
                        
                        Spacer(modifier = Modifier.height(24.dp))
                        
                        // Volume Control Slider
                        Row(
                            modifier = Modifier.fillMaxWidth(0.9f),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text("🔈", fontSize = 20.sp)
                            Slider(
                                value = volume,
                                onValueChange = { 
                                    volume = it
                                    exoPlayer.volume = volume
                                    nativeBridge.setVolume(it)
                                },
                                modifier = Modifier
                                    .weight(1f)
                                    .padding(horizontal = 8.dp)
                            )
                            Text("🔊", fontSize = 20.sp)
                        }

                        Spacer(modifier = Modifier.height(16.dp))

                        // Speed Control Slider
                        Row(
                            modifier = Modifier.fillMaxWidth(0.9f),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text("Speed:", fontSize = 16.sp, modifier = Modifier.width(60.dp))
                            Slider(
                                value = rythmoSpeed,
                                onValueChange = { 
                                    rythmoSpeed = it
                                    nativeBridge.setRythmoSpeed(it.toInt())
                                },
                                valueRange = 100f..999f,
                                modifier = Modifier.weight(1f)
                            )
                            Text("${rythmoSpeed.toInt()} px/s", fontSize = 14.sp, modifier = Modifier.width(60.dp))
                        }

                        Spacer(modifier = Modifier.height(24.dp))

                        // Actions (Open Video + Record Buttons)
                        Row(
                            modifier = Modifier
                                .fillMaxWidth(0.9f)
                                .height(56.dp),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.SpaceEvenly
                        ) {
                            // Open Video Button
                            Button(
                                onClick = { videoPickerLauncher.launch(arrayOf("video/*")) },
                                modifier = Modifier
                                    .weight(1f)
                                    .fillMaxHeight()
                                    .padding(end = 8.dp)
                            ) {
                                Text("Open Video", fontSize = 18.sp)
                            }

                            // Record Button
                            Button(
                                onClick = { 
                                    if (isRecording) {
                                        handleStopRecording()
                                    } else {
                                        if (ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
                                            exoPlayer.seekTo(0)
                                            currentPositionMs = 0
                                            audioRecorder.startRecording()
                                            exoPlayer.play()
                                            isRecording = true
                                        } else {
                                            micPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
                                        }
                                    }
                                },
                                modifier = Modifier
                                    .weight(1f)
                                    .fillMaxHeight()
                                    .padding(start = 8.dp),
                                colors = ButtonDefaults.buttonColors(containerColor = if (isRecording) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.primary)
                            ) {
                                Text(if (isRecording) "Stop" else "Record", fontSize = 18.sp)
                            }
                        }
                    }

                    if (isExporting) {
                        Box(
                            modifier = Modifier
                                .fillMaxSize()
                                .background(Color.Black.copy(alpha = 0.7f)),
                            contentAlignment = Alignment.Center
                        ) {
                            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                CircularProgressIndicator()
                                Spacer(modifier = Modifier.height(16.dp))
                                Text("Exporting... $exportProgress%", color = Color.White)
                            }
                        }
                    }
                }
            }
        }
    }
}
