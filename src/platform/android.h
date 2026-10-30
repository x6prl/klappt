void android_set_status_bar_icons_dark(bool dark);

#ifdef ANDROID
#include <SDL3/SDL.h>
#include <SDL3/SDL_system.h>
#include <jni.h>

inline void android_set_status_bar_icons_dark(bool dark) {
	auto *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
	auto activity = static_cast<jobject>(SDL_GetAndroidActivity());

	if (!env || !activity)
		return;

	jclass cls = env->GetObjectClass(activity);

	if (!cls) {
		env->DeleteLocalRef(activity);
		return;
	}

	jmethodID method = env->GetMethodID(cls, "setSystemBarIconsDark", "(Z)V");

	if (method) {
		env->CallVoidMethod(activity, method, static_cast<jboolean>(dark));
	}

	if (env->ExceptionCheck()) {
		env->ExceptionDescribe();
		env->ExceptionClear();
	}

	env->DeleteLocalRef(cls);
	env->DeleteLocalRef(activity);
}
#else

inline void android_set_status_bar_icons_dark(bool dark) {}

#endif // ANDROID
