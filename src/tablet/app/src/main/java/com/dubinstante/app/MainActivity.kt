package com.dubinstante.app

import android.Manifest
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.media3.common.AudioAttributes
import androidx.media3.common.C
import androidx.media3.common.MediaItem
import androidx.media3.common.Player
import androidx.media3.exoplayer.ExoPlayer
import com.dubinstante.app.ui.SingleTrackPanel
import com.dubinstante.app.ui.StyleCustomizationDialog
import java.io.File
import java.util.Locale
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            MaterialTheme(colorScheme = darkColorScheme()) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    TabletStudioScreen()
                }
            }
        }
    }
}

@Composable
fun TabletStudioScreen() {
    val context = LocalContext.current
    val playbackViewModel: PlaybackViewModel = viewModel()
    val coroutineScope = rememberCoroutineScope()

    var selectedVideoUri by remember { mutableStateOf<String?>(null) }
    var selectedVideoName by remember { mutableStateOf<String?>(null) }
    var volume by remember { mutableFloatStateOf(1.0f) }
    var micVolume by remember { mutableFloatStateOf(1.0f) }
    var totalDurationMs by remember { mutableLongStateOf(0L) }
    var isPlaying by remember { mutableStateOf(false) }

    // State linked to C++ NativeBridge & ViewModel
    val rythmoText by playbackViewModel.rythmoText.collectAsState()
    val rythmoSpeed by playbackViewModel.rythmoSpeed.collectAsState()
    val rythmoStyle by playbackViewModel.rythmoStyle.collectAsState()
    val currentPositionMs by playbackViewModel.currentPositionMs.collectAsState()
    val currentTakePath by playbackViewModel.currentTakePath.collectAsState()
    val takeDurationMs by playbackViewModel.takeDurationMs.collectAsState()
    val isAuditionPlaying by playbackViewModel.isAuditionPlaying.collectAsState()

    // Dialog state
    var showStyleDialog by remember { mutableStateOf(false) }

    // Recording & Export Services
    val audioRecorder = remember { AndroidAudioRecorder(context) }
    val exportService = remember { AndroidExportService(context) }
    var isRecording by remember { mutableStateOf(false) }
    var isExporting by remember { mutableStateOf(false) }
    var exportProgress by remember { mutableIntStateOf(0) }

    // ExoPlayer for Video Playback
    val exoPlayer = remember {
        val audioAttributes = AudioAttributes.Builder()
            .setUsage(C.USAGE_MEDIA)
            .setContentType(C.AUDIO_CONTENT_TYPE_MOVIE)
            .build()

        ExoPlayer.Builder(context).build().apply {
            setAudioAttributes(audioAttributes, false)
            playWhenReady = false
        }
    }

    // Secondary Audio Player for Take Auditioning
    val takeAudioPlayer = remember {
        ExoPlayer.Builder(context).build().apply {
            playWhenReady = false
        }
    }

    // Synchronize volume
    LaunchedEffect(volume) {
        exoPlayer.volume = volume
        playbackViewModel.setVolume(volume)
    }
    LaunchedEffect(micVolume) {
        takeAudioPlayer.volume = micVolume.coerceIn(0f, 1f)
    }

    // Export Document Launcher
    val saveVideoLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.CreateDocument("video/mp4")
    ) { uri: Uri? ->
        if (uri != null && selectedVideoUri != null && currentTakePath != null) {
            isExporting = true
            exportProgress = 0
            coroutineScope.launch {
                exportService.exportVideo(
                    sourceVideoUri = Uri.parse(selectedVideoUri),
                    recordedAudioPath = currentTakePath!!,
                    targetUri = uri,
                    mediaVolume = volume,
                    micVolume = micVolume,
                    recordDurationMs = takeDurationMs,
                    exportFullVideo = true,
                    onProgress = { progress -> exportProgress = progress },
                    onComplete = { success, msg, _ ->
                        isExporting = false
                        coroutineScope.launch(Dispatchers.Main) {
                            if (success) {
                                Toast.makeText(context, "Export completed successfully!", Toast.LENGTH_LONG).show()
                            } else {
                                Toast.makeText(context, "Export Failed: $msg", Toast.LENGTH_LONG).show()
                            }
                        }
                    }
                )
            }
        }
    }

    // Stop recording handler
    val handleStopRecording = {
        if (isRecording) {
            exoPlayer.pause()
            val duration = audioRecorder.stopRecording()
            isRecording = false

            val audioPath = audioRecorder.outputFile?.absolutePath
            if (audioPath != null && File(audioPath).exists() && File(audioPath).length() > 0) {
                playbackViewModel.setTake(audioPath, duration)
                Toast.makeText(context, "Take recorded (${duration / 1000}s). Ready to audition or export!", Toast.LENGTH_SHORT).show()
            } else {
                Toast.makeText(context, "Recording was too short or empty.", Toast.LENGTH_SHORT).show()
            }
        }
    }
    val handleStopRecordingState by rememberUpdatedState(handleStopRecording)

    // Player event listeners
    DisposableEffect(exoPlayer) {
        val listener = object : Player.Listener {
            override fun onPlaybackStateChanged(playbackState: Int) {
                if (playbackState == Player.STATE_ENDED) {
                    if (isRecording) {
                        handleStopRecordingState()
                    }
                    if (isAuditionPlaying) {
                        takeAudioPlayer.pause()
                        takeAudioPlayer.seekTo(0)
                        exoPlayer.seekTo(0)
                        playbackViewModel.setAuditionPlaying(false)
                    }
                }
                totalDurationMs = exoPlayer.duration.coerceAtLeast(0L)
            }

            override fun onIsPlayingChanged(playing: Boolean) {
                isPlaying = playing
            }
        }
        exoPlayer.addListener(listener)
        onDispose {
            exoPlayer.removeListener(listener)
            exoPlayer.release()
            takeAudioPlayer.release()
        }
    }

    // High precision sync loop for position updates
    LaunchedEffect(exoPlayer) {
        while (true) {
            if (exoPlayer.isPlaying) {
                playbackViewModel.updatePosition(exoPlayer.currentPosition)
            }
            delay(16) // ~60fps
        }
    }

    // Mic Permission Launcher
    val micPermissionLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestPermission()
    ) { isGranted ->
        if (isGranted) {
            // Stop auditioning if active
            if (isAuditionPlaying) {
                takeAudioPlayer.pause()
                playbackViewModel.setAuditionPlaying(false)
            }
            exoPlayer.seekTo(0)
            playbackViewModel.updatePosition(0L)
            val started = audioRecorder.startRecording()
            if (started) {
                exoPlayer.play()
                isRecording = true
            }
        } else {
            Toast.makeText(context, "Microphone permission is required for dubbing", Toast.LENGTH_SHORT).show()
        }
    }

    // Toggle Audition Take
    val handleAuditionToggle: () -> Unit = {
        if (isAuditionPlaying) {
            exoPlayer.pause()
            takeAudioPlayer.pause()
            playbackViewModel.setAuditionPlaying(false)
        } else {
            currentTakePath?.let { path ->
                takeAudioPlayer.setMediaItem(MediaItem.fromUri(Uri.fromFile(File(path))))
                takeAudioPlayer.prepare()
                exoPlayer.seekTo(0)
                takeAudioPlayer.seekTo(0)
                playbackViewModel.updatePosition(0L)
                exoPlayer.play()
                takeAudioPlayer.play()
                playbackViewModel.setAuditionPlaying(true)
            }
        }
    }

    // Video Picker Launcher (Supports MP4, MKV, WebM)
    val videoPickerLauncher = rememberLauncherForActivityResult(
        contract = object : ActivityResultContracts.OpenDocument() {
            override fun createIntent(context: android.content.Context, input: Array<String>): android.content.Intent {
                return super.createIntent(context, input).apply {
                    type = "video/*"
                    putExtra(
                        android.content.Intent.EXTRA_MIME_TYPES,
                        arrayOf("video/mp4", "video/x-matroska", "video/webm", "video/quicktime")
                    )
                    addCategory(android.content.Intent.CATEGORY_OPENABLE)
                }
            }
        }
    ) { uri: Uri? ->
        uri?.let {
            selectedVideoUri = it.toString()
            selectedVideoName = it.lastPathSegment?.substringAfterLast('/') ?: "Video Loaded"

            playbackViewModel.openVideo(it.toString())
            playbackViewModel.clearTake()
            takeAudioPlayer.stop()
            takeAudioPlayer.clearMediaItems()

            exoPlayer.setMediaItem(MediaItem.fromUri(uri))
            exoPlayer.prepare()
            exoPlayer.seekTo(0)
            playbackViewModel.updatePosition(0L)
        }
    }

    // Studio Layout
    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
    ) {
        // Top App Bar / Studio Header
        Surface(
            modifier = Modifier
                .fillMaxWidth()
                .height(64.dp),
            color = MaterialTheme.colorScheme.surface,
            tonalElevation = 3.dp
        ) {
            Row(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 20.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                // Branding & Title
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Image(
                        painter = painterResource(id = R.drawable.logo),
                        contentDescription = "DubInstante Logo",
                        modifier = Modifier.size(32.dp)
                    )
                    Spacer(modifier = Modifier.width(12.dp))
                    Column {
                        Text(
                            text = "DubInstante Studio",
                            fontSize = 18.sp,
                            fontWeight = FontWeight.Bold,
                            color = MaterialTheme.colorScheme.primary
                        )
                        Text(
                            text = "Tablet Edition • 1 Mic Studio",
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                    if (selectedVideoName != null) {
                        Spacer(modifier = Modifier.width(16.dp))
                        Surface(
                            shape = RoundedCornerShape(8.dp),
                            color = MaterialTheme.colorScheme.primaryContainer,
                            contentColor = MaterialTheme.colorScheme.onPrimaryContainer
                        ) {
                            Text(
                                text = "🎬 $selectedVideoName",
                                fontSize = 12.sp,
                                fontWeight = FontWeight.Medium,
                                modifier = Modifier.padding(horizontal = 10.dp, vertical = 4.dp)
                            )
                        }
                    }
                }

                // Action Buttons
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    OutlinedButton(
                        onClick = { videoPickerLauncher.launch(arrayOf("video/*")) },
                        shape = RoundedCornerShape(8.dp),
                        contentPadding = PaddingValues(horizontal = 14.dp, vertical = 8.dp)
                    ) {
                        Text("📁 Open Video", fontSize = 13.sp)
                    }

                    OutlinedButton(
                        onClick = { showStyleDialog = true },
                        shape = RoundedCornerShape(8.dp),
                        contentPadding = PaddingValues(horizontal = 14.dp, vertical = 8.dp)
                    ) {
                        Text("🎨 Band Style", fontSize = 13.sp)
                    }

                    Button(
                        onClick = {
                            saveVideoLauncher.launch("dubinstante_export_${System.currentTimeMillis()}.mp4")
                        },
                        enabled = currentTakePath != null && !isRecording && !isExporting,
                        shape = RoundedCornerShape(8.dp),
                        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 8.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF1B5E20))
                    ) {
                        Text("💾 Export", fontSize = 13.sp, fontWeight = FontWeight.Bold)
                    }
                }
            }
        }

        // Main Studio Console (Two-Pane Landscape Layout)
        Row(
            modifier = Modifier
                .fillMaxSize()
                .padding(12.dp)
        ) {
            // Left Column: Primary Stage (Video + Rythmo Band + Transport Bar)
            Column(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxHeight(),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                // 16:9 Video Canvas
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .weight(1f)
                        .clip(RoundedCornerShape(12.dp))
                        .background(Color.Black)
                        .border(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.4f), RoundedCornerShape(12.dp)),
                    contentAlignment = Alignment.Center
                ) {
                    if (selectedVideoUri != null) {
                        VideoPlayer(
                            exoPlayer = exoPlayer,
                            modifier = Modifier.fillMaxSize()
                        )
                    } else {
                        Column(
                            horizontalAlignment = Alignment.CenterHorizontally,
                            verticalArrangement = Arrangement.Center
                        ) {
                            Text(
                                text = "No Video Loaded",
                                fontSize = 18.sp,
                                fontWeight = FontWeight.Medium,
                                color = Color.Gray
                            )
                            Spacer(modifier = Modifier.height(8.dp))
                            Button(
                                onClick = { videoPickerLauncher.launch(arrayOf("video/*")) },
                                shape = RoundedCornerShape(8.dp)
                            ) {
                                Text("Open Video to Begin")
                            }
                        }
                    }
                }

                // Rythmo Band (Dedicated Tablet Height 110dp)
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(110.dp),
                    shape = RoundedCornerShape(10.dp),
                    elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)
                ) {
                    RythmoBand(
                        text = rythmoText,
                        onTextChanged = { newText ->
                            playbackViewModel.setRythmoText(newText)
                        },
                        currentPositionMs = currentPositionMs,
                        speedPixelsPerSecond = rythmoSpeed.toInt(),
                        onSeekRequested = { newMs ->
                            exoPlayer.seekTo(newMs)
                            playbackViewModel.updatePosition(newMs)
                            if (isAuditionPlaying) {
                                takeAudioPlayer.seekTo(newMs)
                            }
                        },
                        style = rythmoStyle,
                        modifier = Modifier.fillMaxSize()
                    )
                }

                // Transport & Scrubbing Bar
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(60.dp),
                    shape = RoundedCornerShape(10.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f))
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(horizontal = 16.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        // Jump to Start
                        IconButton(
                            onClick = {
                                exoPlayer.seekTo(0)
                                playbackViewModel.updatePosition(0L)
                                if (isAuditionPlaying) {
                                    takeAudioPlayer.seekTo(0)
                                }
                            },
                            enabled = selectedVideoUri != null
                        ) {
                            Text("⏮", fontSize = 18.sp)
                        }

                        // Step back 5s
                        IconButton(
                            onClick = {
                                val target = (currentPositionMs - 5000L).coerceAtLeast(0L)
                                exoPlayer.seekTo(target)
                                playbackViewModel.updatePosition(target)
                                if (isAuditionPlaying) {
                                    takeAudioPlayer.seekTo(target)
                                }
                            },
                            enabled = selectedVideoUri != null
                        ) {
                            Text("⏪", fontSize = 16.sp)
                        }

                        // Play / Pause Toggle
                        FilledIconButton(
                            onClick = {
                                if (isPlaying) {
                                    exoPlayer.pause()
                                    if (isAuditionPlaying) takeAudioPlayer.pause()
                                } else {
                                    exoPlayer.play()
                                    if (isAuditionPlaying) takeAudioPlayer.play()
                                }
                            },
                            enabled = selectedVideoUri != null && !isRecording,
                            modifier = Modifier.size(42.dp)
                        ) {
                            Text(if (isPlaying) "⏸" else "▶", fontSize = 18.sp)
                        }

                        // Step forward 5s
                        IconButton(
                            onClick = {
                                val target = (currentPositionMs + 5000L).coerceAtMost(totalDurationMs)
                                exoPlayer.seekTo(target)
                                playbackViewModel.updatePosition(target)
                                if (isAuditionPlaying) {
                                    takeAudioPlayer.seekTo(target)
                                }
                            },
                            enabled = selectedVideoUri != null
                        ) {
                            Text("⏩", fontSize = 16.sp)
                        }

                        Spacer(modifier = Modifier.width(12.dp))

                        // Timecode Display
                        Text(
                            text = "${formatTimecode(currentPositionMs)} / ${formatTimecode(totalDurationMs)}",
                            fontSize = 13.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = MaterialTheme.colorScheme.onSurface
                        )

                        Spacer(modifier = Modifier.width(12.dp))

                        // Scrubbing Slider
                        Slider(
                            value = if (totalDurationMs > 0) currentPositionMs.toFloat() / totalDurationMs else 0f,
                            onValueChange = { ratio ->
                                val target = (ratio * totalDurationMs).toLong()
                                exoPlayer.seekTo(target)
                                playbackViewModel.updatePosition(target)
                                if (isAuditionPlaying) {
                                    takeAudioPlayer.seekTo(target)
                                }
                            },
                            enabled = selectedVideoUri != null && !isRecording,
                            modifier = Modifier.weight(1f)
                        )
                    }
                }
            }

            // Right Column: Single Track Console (approx 340dp width)
            SingleTrackPanel(
                isRecording = isRecording,
                onRecordToggle = {
                    if (isRecording) {
                        handleStopRecording()
                    } else {
                        if (ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
                            if (isAuditionPlaying) {
                                takeAudioPlayer.pause()
                                playbackViewModel.setAuditionPlaying(false)
                            }
                            exoPlayer.seekTo(0)
                            playbackViewModel.updatePosition(0L)
                            val started = audioRecorder.startRecording()
                            if (started) {
                                exoPlayer.play()
                                isRecording = true
                            }
                        } else {
                            micPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
                        }
                    }
                },
                hasTake = currentTakePath != null,
                takeDurationMs = takeDurationMs,
                isAuditionPlaying = isAuditionPlaying,
                onAuditionToggle = handleAuditionToggle,
                onClearTake = {
                    takeAudioPlayer.stop()
                    playbackViewModel.clearTake()
                    Toast.makeText(context, "Take discarded.", Toast.LENGTH_SHORT).show()
                },
                onExportClick = {
                    saveVideoLauncher.launch("dubinstante_export_${System.currentTimeMillis()}.mp4")
                },
                volume = volume,
                onVolumeChange = { volume = it },
                micVolume = micVolume,
                onMicVolumeChange = { micVolume = it },
                rythmoSpeed = rythmoSpeed,
                onSpeedChange = { playbackViewModel.setRythmoSpeed(it) },
                hasVideoLoaded = selectedVideoUri != null,
                modifier = Modifier.width(360.dp)
            )
        }
    }

    // Style Customization Dialog
    if (showStyleDialog) {
        StyleCustomizationDialog(
            initialStyle = rythmoStyle,
            onDismissRequest = { showStyleDialog = false },
            onSaveStyle = { newStyle ->
                playbackViewModel.setRythmoStyle(newStyle)
                showStyleDialog = false
            }
        )
    }

    // Export Progress Modal
    if (isExporting) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = 0.75f)),
            contentAlignment = Alignment.Center
        ) {
            Card(
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
                modifier = Modifier.padding(24.dp)
            ) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    modifier = Modifier.padding(32.dp)
                ) {
                    CircularProgressIndicator(
                        progress = { exportProgress / 100f },
                        modifier = Modifier.size(64.dp),
                        strokeWidth = 6.dp
                    )
                    Spacer(modifier = Modifier.height(20.dp))
                    Text(
                        text = "Exporting Dubbed Video...",
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onSurface
                    )
                    Spacer(modifier = Modifier.height(6.dp))
                    Text(
                        text = "$exportProgress%",
                        fontSize = 15.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = MaterialTheme.colorScheme.primary
                    )
                    Spacer(modifier = Modifier.height(10.dp))
                    Text(
                        text = "Mixing original video and microphone take with FFmpeg",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        }
    }
}

private fun formatTimecode(ms: Long): String {
    val totalSeconds = ms / 1000
    val minutes = totalSeconds / 60
    val seconds = totalSeconds % 60
    val millis = ms % 1000
    return String.format(Locale.US, "%02d:%02d.%03d", minutes, seconds, millis)
}
