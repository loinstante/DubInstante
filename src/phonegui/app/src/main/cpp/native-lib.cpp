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

extern "C" JNIEXPORT void JNICALL
Java_com_dubinstante_app_NativeBridge_setVolume(JNIEnv *env, jobject /* this */,
                                                jfloat volume) {
  if (g_playbackEngine) {
    g_playbackEngine->setVolume(volume);
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_dubinstante_app_NativeBridge_setRythmoText(JNIEnv *env,
                                                    jobject /* this */,
                                                    jstring text) {
  if (g_playbackEngine) {
    const char *textStr = env->GetStringUTFChars(text, nullptr);
    g_playbackEngine->setRythmoText(std::string(textStr));
    env->ReleaseStringUTFChars(text, textStr);
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dubinstante_app_NativeBridge_getRythmoText(JNIEnv *env,
                                                    jobject /* this */) {
  if (g_playbackEngine) {
    return env->NewStringUTF(g_playbackEngine->getRythmoText().c_str());
  }
  return env->NewStringUTF("");
}

extern "C" JNIEXPORT void JNICALL
Java_com_dubinstante_app_NativeBridge_setRythmoSpeed(JNIEnv *env,
                                                     jobject /* this */,
                                                     jint speed) {
  if (g_playbackEngine) {
    g_playbackEngine->setRythmoSpeed(speed);
  }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_dubinstante_app_NativeBridge_getRythmoSpeed(JNIEnv *env,
                                                     jobject /* this */) {
  if (g_playbackEngine) {
    return g_playbackEngine->getRythmoSpeed();
  }
  return 100;
}
