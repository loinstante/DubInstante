import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import Qt.labs.platform as Platform
import QtCore

ApplicationWindow {
    id: root
    visible: true
    width: 360
    height: 640
    title: "DubInstante Phone"

    // ── Recording ──────────────────────────────────────────────
    CaptureSession {
        id: captureSession
        audioInput: AudioInput {}
        recorder: MediaRecorder {
            id: recorder
            outputLocation: StandardPaths.writableLocation(StandardPaths.MusicLocation) + "/dub_recording.m4a"
            onRecorderStateChanged: {
                if (recorderState === MediaRecorder.StoppedState)
                    console.log("Recording saved to", actualLocation)
            }
        }
    }

    // ── Video Playback ─────────────────────────────────────────
    MediaPlayer {
        id: player
        videoOutput: videoOutput
        audioOutput: AudioOutput {}
    }

    // ── Android-compatible File Dialog (Qt.labs.platform) ──────
    Platform.FileDialog {
        id: fileDialog
        title: "Choose a video file"
        nameFilters: ["Video files (*.mp4 *.mkv *.avi *.mov *.webm)", "All files (*)"]
        onAccepted: {
            player.source = file
            player.play()
        }
    }

    // ── UI ─────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        // Video area
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: width * 9 / 16
            color: "black"
            radius: 6

            VideoOutput {
                id: videoOutput
                anchors.fill: parent
            }

            // Tap to play / pause
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (player.playbackState === MediaPlayer.PlayingState)
                        player.pause()
                    else
                        player.play()
                }
            }
        }

        // Buttons row
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                id: loadBtn
                text: "📂  Load Video"
                Layout.fillWidth: true
                onClicked: fileDialog.open()

                background: Rectangle {
                    color: loadBtn.pressed ? "#bbdefb" : "#e3f2fd"
                    radius: 6
                }
            }

            Button {
                id: recBtn
                property bool isRecording: recorder.recorderState === MediaRecorder.RecordingState
                text: isRecording ? "⏹  Stop" : "🔴  Record"
                Layout.fillWidth: true
                font.bold: true
                onClicked: {
                    if (isRecording)
                        recorder.stop()
                    else
                        recorder.record()
                }

                background: Rectangle {
                    color: recBtn.isRecording
                        ? (recBtn.pressed ? "#c62828" : "#ef5350")
                        : (recBtn.pressed ? "#c8e6c9" : "#e8f5e9")
                    radius: 6
                }

                contentItem: Text {
                    text: recBtn.text
                    font: recBtn.font
                    color: recBtn.isRecording ? "white" : "black"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // Rythmo Band (editable)
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            border.color: "#bdbdbd"
            border.width: 1
            radius: 6
            color: "#fafafa"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                Text {
                    text: "Rythmo Band"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#757575"
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    TextArea {
                        id: rythmoText
                        placeholderText: "Type your dubbing text here…"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 16
                    }
                }
            }
        }
    }
}
