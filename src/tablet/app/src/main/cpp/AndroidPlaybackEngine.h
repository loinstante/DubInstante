#pragma once

#include <string>

/**
 * @class AndroidPlaybackEngine
 * @brief Android-specific adaptation of the C++ PlaybackEngine.
 *
 * This class serves as the C++ logic core for the Android app.
 * Since the Android UI completely handles video rendering to ensure perfect
 * stability, this class acts as the bridge/state-manager to keep the C++
 * architecture intact without fighting the Android UI layer.
 */
class AndroidPlaybackEngine {
public:
  AndroidPlaybackEngine();
  ~AndroidPlaybackEngine();

  void openFile(const std::string &uri);
  void play();
  void pause();
  void setVolume(float volume);

  // Rythmo Band Management
  void setRythmoText(const std::string &text);
  std::string getRythmoText() const;
  void setRythmoSpeed(int speedPixelsPerSecond);
  int getRythmoSpeed() const;

  // Rythmo Style Management
  void setRythmoStyle(uint32_t textColor, uint32_t bgColor, int textSizeSp,
                      uint32_t playheadColor, int fontIndex);
  uint32_t getTextColor() const;
  uint32_t getBackgroundColor() const;
  int getTextSize() const;
  uint32_t getPlayheadColor() const;
  int getFontIndex() const;

  // Track & Take Management (Single Track Studio)
  void setTakeActive(bool active);
  bool isTakeActive() const;
  void setTakeDurationMs(int64_t durationMs);
  int64_t getTakeDurationMs() const;

private:
  std::string currentUri;
  float currentVolume = 1.0f;

  // Rythmo State (Pure C++ adaptation of RythmoManager logic)
  std::string rythmoText =
      "Ceci est une bande rythmo de test pour le doublage Android...";
  int rythmoSpeed = 100;

  // Style State
  uint32_t textColor = 0xFFFFFFFF;        // Default white
  uint32_t backgroundColor = 0xFF1E1E1E;  // Default dark surface
  int textSize = 40;                      // Default 40sp
  uint32_t playheadColor = 0xFFFF0000;    // Default red
  int fontIndex = 0;                      // 0: Monospace, 1: Sans-Serif, 2: Serif

  // Single Track Take State
  bool hasTake = false;
  int64_t takeDurationMs = 0;
};
