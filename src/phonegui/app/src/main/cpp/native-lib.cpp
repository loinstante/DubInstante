#include "AndroidPlaybackEngine.h"
#include <jni.h>
#include <string>

extern "C" JNIEXPORT jlong JNICALL
Java_com_dubinstante_app_NativeBridge_initialize(JNIEnv *env,
                                                 jobject /* this */) {
  auto *engine = new AndroidPlaybackEngine();
  return reinterpret_cast<jlong>(engine);
}

extern "C" JNIEXPORT void JNICALL Java_com_dubinstante_app_NativeBridge_release(
    JNIEnv *env, jobject /* this */, jlong handle) {
  if (handle != 0) {
    auto *engine = reinterpret_cast<AndroidPlaybackEngine *>(handle);
    delete engine;
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_dubinstante_app_NativeBridge_openVideo(JNIEnv *env, jobject /* this */,
                                                jlong handle, jstring uri) {
  if (handle != 0) {
    auto *engine = reinterpret_cast<AndroidPlaybackEngine *>(handle);
    const char *uriStr = env->GetStringUTFChars(uri, nullptr);
    engine->openFile(std::string(uriStr));
    env->ReleaseStringUTFChars(uri, uriStr);
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_dubinstante_app_NativeBridge_play(
    JNIEnv *env, jobject /* this */, jlong handle) {
  if (handle != 0) {
    auto *engine = reinterpret_cast<AndroidPlaybackEngine *>(handle);
    engine->play();
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_dubinstante_app_NativeBridge_pause(
    JNIEnv *env, jobject /* this */, jlong handle) {
  if (handle != 0) {
    auto *engine = reinterpret_cast<AndroidPlaybackEngine *>(handle);
    engine->pause();
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_dubinstante_app_NativeBridge_setVolume(JNIEnv *env, jobject /* this */,
                                                jlong handle, jfloat volume) {
  if (handle != 0) {
    auto *engine = reinterpret_cast<AndroidPlaybackEngine *>(handle);
    engine->setVolume(volume);
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_dubinstante_app_NativeBridge_setRythmoText(JNIEnv *env,
                                                    jobject /* this */,
                                                    jlong handle,
                                                    jstring text) {
  if (handle != 0) {
    auto *engine = reinterpret_cast<AndroidPlaybackEngine *>(handle);
    const char *textStr = env->GetStringUTFChars(text, nullptr);
    engine->setRythmoText(std::string(textStr));
    env->ReleaseStringUTFChars(text, textStr);
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dubinstante_app_NativeBridge_getRythmoText(JNIEnv *env,
                                                    jobject /* this */,
                                                    jlong handle) {
  if (handle != 0) {
    auto *engine = reinterpret_cast<AndroidPlaybackEngine *>(handle);
    return env->NewStringUTF(engine->getRythmoText().c_str());
  }
  return env->NewStringUTF("");
}

extern "C" JNIEXPORT void JNICALL
Java_com_dubinstante_app_NativeBridge_setRythmoSpeed(JNIEnv *env,
                                                     jobject /* this */,
                                                     jlong handle, jint speed) {
  if (handle != 0) {
    auto *engine = reinterpret_cast<AndroidPlaybackEngine *>(handle);
    engine->setRythmoSpeed(speed);
  }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_dubinstante_app_NativeBridge_getRythmoSpeed(JNIEnv *env,
                                                     jobject /* this */,
                                                     jlong handle) {
  if (handle != 0) {
    auto *engine = reinterpret_cast<AndroidPlaybackEngine *>(handle);
    return engine->getRythmoSpeed();
  }
  return 100;
}
