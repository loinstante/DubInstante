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
