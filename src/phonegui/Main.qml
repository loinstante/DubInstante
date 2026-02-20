import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia

ApplicationWindow {
    visible: true
    width: 360
    height: 640
    title: "DubInstante Phone"

    CaptureSession {
        id: captureSession
        audioInput: AudioInput {}
        recorder: MediaRecorder {
            id: recorder
            outputLocation: "file:///storage/emulated/0/Download/dub_recording.m4a"
            onRecorderStateChanged: {
                if (recorderState === MediaRecorder.StoppedState) {
                    console.log("Recording saved to", outputLocation)
                }
            }
        }
    }

    MediaPlayer {
        id: player
        videoOutput: videoOutput
        audioOutput: AudioOutput {}
    }

    FileDialog {
        id: fileDialog
        title: "Please choose a video file"
        onAccepted: {
            player.source = fileDialog.selectedFile
            player.play()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        // Video Output
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: width * 9 / 16 // 16:9 aspect ratio
            color: "black"

            VideoOutput {
                id: videoOutput
                anchors.fill: parent
            }
        }

        // Controls
        RowLayout {
            Layout.fillWidth: true
            
            Button {
                text: "Load Video"
                Layout.fillWidth: true
                onClicked: fileDialog.open()
            }

            Button {
                text: recorder.recorderState === MediaRecorder.RecordingState ? "Stop" : "Record"
                Layout.fillWidth: true
                font.bold: true
                palette.buttonText: recorder.recorderState === MediaRecorder.RecordingState ? "white" : "black"
                palette.button: recorder.recorderState === MediaRecorder.RecordingState ? "red" : "#e0e0e0"

                onClicked: {
                    if (recorder.recorderState === MediaRecorder.RecordingState) {
                        recorder.stop()
                    } else {
                        recorder.record()
                    }
                }
            }
        }

        // Rythmo Band (Editable)
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            border.color: "gray"
            border.width: 1
            color: "#f5f5f5"

            ScrollView {
                anchors.fill: parent
                anchors.margins: 5
                
                TextArea {
                    id: rythmoText
                    width: parent.width
                    text: "Write your rythmo text here...\nScroll down as the video plays."
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
