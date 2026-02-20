#ifdef __ANDROID__
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <android/window.h>

ANativeWindow* g_second_window = nullptr;

extern "C" {

    JNIEXPORT void JNICALL Java_org_godot_plugins_aynthor_AynThorPlugin_nativeSetSurface(JNIEnv* env, jobject clazz, jobject surface) {
        if (g_second_window) {
            ANativeWindow_release(g_second_window);
        }

        g_second_window = ANativeWindow_fromSurface(env, surface);

        if (g_second_window) {
            ANativeWindow_setBuffersGeometry(g_second_window, 0, 0, WINDOW_FORMAT_RGBA_8888);
        }
    }

    JNIEXPORT void JNICALL Java_org_godot_plugins_aynthor_AynThorPlugin_nativeRemoveSurface(JNIEnv* env, jobject clazz) {
        if (g_second_window) {
            ANativeWindow_release(g_second_window);
            g_second_window = nullptr;
        }
    }
}
#else
void* g_second_window = nullptr;
#endif