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

private:
  std::string currentUri;
  float currentVolume = 1.0f;

  // Rythmo State (Pure C++ adaptation of RythmoManager logic)
  std::string rythmoText =
      "Ceci est une bande rythmo de test pour le doublage Android...";
  int rythmoSpeed = 100;
};
