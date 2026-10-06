#include "haptic.h"

#ifdef __ANDROID__
#include <SDL3/SDL_system.h>

#include <jni.h>

namespace {
constexpr int ANDROID_HAPTIC_VIRTUAL_KEY = 1;
constexpr int ANDROID_HAPTIC_FLAG_IGNORE_VIEW_SETTING = 1;
} // namespace

static jobject jni_decor_view = nullptr;
static jmethodID jni_perform_haptic_mid = nullptr;

inline void android_haptics_tap() {
	JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
	if (!env)
		return;

	if (!jni_decor_view) {
		jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
		if (!activity)
			return;

		jclass act_cls = env->GetObjectClass(activity);
		jmethodID get_win_mid =
			  env->GetMethodID(act_cls, "getWindow", "()Landroid/view/Window;");
		jobject window = env->CallObjectMethod(activity, get_win_mid);

		if (window) {
			jclass win_cls = env->GetObjectClass(window);
			jmethodID get_decor_mid = env->GetMethodID(win_cls, "getDecorView",
			                                           "()Landroid/view/View;");
			jobject local_decor = env->CallObjectMethod(window, get_decor_mid);

			if (local_decor) {
				jclass view_cls = env->GetObjectClass(local_decor);
				jni_perform_haptic_mid = env->GetMethodID(
					  view_cls, "performHapticFeedback", "(II)Z");
				jni_decor_view = env->NewGlobalRef(local_decor);

				env->DeleteLocalRef(view_cls);
				env->DeleteLocalRef(local_decor);
			}
			env->DeleteLocalRef(win_cls);
			env->DeleteLocalRef(window);
		}
		env->DeleteLocalRef(act_cls);
	}

	if (jni_decor_view && jni_perform_haptic_mid) {
		env->CallBooleanMethod(jni_decor_view, jni_perform_haptic_mid,
		                       ANDROID_HAPTIC_VIRTUAL_KEY,
		                       ANDROID_HAPTIC_FLAG_IGNORE_VIEW_SETTING);
	}
}
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// Calls navigator.vibrate(ms) directly in JavaScript
EM_JS(void, web_vibrate, (int ms), {
	if (navigator.vibrate) {
		navigator.vibrate(ms);
	}
});
#endif

void haptic_tap() {
#if defined(__ANDROID__)
	android_haptics_tap();
#elif defined(__EMSCRIPTEN__)
	web_vibrate(15);
#endif
}

void haptic_quit() {
#if defined(__ANDROID__)
	JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
	if (env && jni_decor_view) {
		env->DeleteGlobalRef(jni_decor_view);
		jni_decor_view = nullptr;
	}
#endif
}
