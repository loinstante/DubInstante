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

void AndroidPlaybackEngine::setRythmoStyle(uint32_t textCol, uint32_t bgCol,
                                          int sizeSp, uint32_t playheadCol,
                                          int fontIdx) {
  textColor = textCol;
  backgroundColor = bgCol;
  textSize = sizeSp;
  playheadColor = playheadCol;
  fontIndex = fontIdx;
  LOGI("AndroidPlaybackEngine: Rythmo style updated (text: 0x%08X, bg: 0x%08X, size: %d, playhead: 0x%08X, font: %d)",
       textCol, bgCol, sizeSp, playheadCol, fontIdx);
}

uint32_t AndroidPlaybackEngine::getTextColor() const { return textColor; }
uint32_t AndroidPlaybackEngine::getBackgroundColor() const { return backgroundColor; }
int AndroidPlaybackEngine::getTextSize() const { return textSize; }
uint32_t AndroidPlaybackEngine::getPlayheadColor() const { return playheadColor; }
int AndroidPlaybackEngine::getFontIndex() const { return fontIndex; }

void AndroidPlaybackEngine::setTakeActive(bool active) {
  hasTake = active;
  LOGI("AndroidPlaybackEngine: Take active state set to %d", active ? 1 : 0);
}

bool AndroidPlaybackEngine::isTakeActive() const { return hasTake; }

void AndroidPlaybackEngine::setTakeDurationMs(int64_t durationMs) {
  takeDurationMs = durationMs;
  LOGI("AndroidPlaybackEngine: Take duration set to %lld ms", static_cast<long long>(durationMs));
}

int64_t AndroidPlaybackEngine::getTakeDurationMs() const { return takeDurationMs; }
