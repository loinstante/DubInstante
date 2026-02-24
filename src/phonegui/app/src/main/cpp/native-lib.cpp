#include "AndroidPlaybackEngine.h"
#include <jni.h>
#include <string>

// In a real application, instead of a global pointer, we would pass the pointer
// back to Kotlin as a 'long' handle so we can have multiple engines if needed.
static AndroidPlaybackEngine *g_playbackEngine = nullptr;

extern "C" JNIEXPORT void JNICALL
Java_com_dubinstante_app_NativeBridge_initialize(JNIEnv *env,
                                                 jobject /* this */) {
  if (!g_playbackEngine) {
    g_playbackEngine = new AndroidPlaybackEngine();
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_dubinstante_app_NativeBridge_openVideo(JNIEnv *env, jobject /* this */,
                                                jstring uri) {
  if (g_playbackEngine) {
    const char *uriStr = env->GetStringUTFChars(uri, nullptr);
    g_playbackEngine->openFile(std::string(uriStr));
    env->ReleaseStringUTFChars(uri, uriStr);
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_dubinstante_app_NativeBridge_play(JNIEnv *env, jobject /* this */) {
  if (g_playbackEngine) {
    g_playbackEngine->play();
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_dubinstante_app_NativeBridge_pause(JNIEnv *env, jobject /* this */) {
  if (g_playbackEngine) {
    g_playbackEngine->pause();
  }
}
