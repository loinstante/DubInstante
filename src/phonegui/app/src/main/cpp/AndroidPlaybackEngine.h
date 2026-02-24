#pragma once

#include <string>

/**
 * @class AndroidPlaybackEngine
 * @brief Android-specific adaptation of the C++ PlaybackEngine.
 * 
 * This class serves as the C++ logic core for the Android app. 
 * Since the Android UI completely handles video rendering to ensure perfect stability, 
 * this class acts as the bridge/state-manager to keep the C++ architecture intact 
 * without fighting the Android UI layer.
 */
class AndroidPlaybackEngine {
public:
    AndroidPlaybackEngine();
    ~AndroidPlaybackEngine();

    void openFile(const std::string& uri);
    void play();
    void pause();

private:
    std::string currentUri;
};
