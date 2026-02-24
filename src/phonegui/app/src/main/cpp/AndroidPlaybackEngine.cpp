#include "AndroidPlaybackEngine.h"
#include <android/log.h>

#define LOG_TAG "DubInstanteCore"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

AndroidPlaybackEngine::AndroidPlaybackEngine() {
  LOGI("AndroidPlaybackEngine created.");
}

AndroidPlaybackEngine::~AndroidPlaybackEngine() {
  LOGI("AndroidPlaybackEngine destroyed.");
}

void AndroidPlaybackEngine::openFile(const std::string &uri) {
  currentUri = uri;
  LOGI("AndroidPlaybackEngine: Instructed to open file %s", uri.c_str());
  // Here we can link back to Core C++ logic such as parsing metadata, audio
  // extraction, etc.
}

void AndroidPlaybackEngine::play() {
  LOGI("AndroidPlaybackEngine: Play called from UI.");
}

void AndroidPlaybackEngine::pause() {
  LOGI("AndroidPlaybackEngine: Pause called from UI.");
}

void AndroidPlaybackEngine::setVolume(float volume) {
  currentVolume = volume;
  LOGI("AndroidPlaybackEngine: Volume set to %f", volume);
}

void AndroidPlaybackEngine::setRythmoText(const std::string &text) {
  rythmoText = text;
  LOGI("AndroidPlaybackEngine: Rythmo text updated, length: %zu",
       text.length());
}

std::string AndroidPlaybackEngine::getRythmoText() const { return rythmoText; }

void AndroidPlaybackEngine::setRythmoSpeed(int speedPixelsPerSecond) {
  rythmoSpeed = speedPixelsPerSecond;
  LOGI("AndroidPlaybackEngine: Rythmo speed set to %d px/s",
       speedPixelsPerSecond);
}

int AndroidPlaybackEngine::getRythmoSpeed() const { return rythmoSpeed; }
