# How to use these scripts

For the most part, just run the scripts in this directory from this directory. 

## Notes:
- visionOS
	- Requires CMake 3.28 or newer
- Web
 	1. Install emsdk somewhere.
  	2. On Unix hosts, first run `source emsdk_env.sh` (found in the emsdk root directory), then run `config-web-unix.sh`
	3. After the build completes, use `python3 -m http.server` in the build directory to make the page accessible.
- Android
	- There is no easy CMake config for this platform. Instead, follow the steps as outlined below. See the android GitHub action for more details. 
	1. Install Android Studio + NDK from https://developer.android.com/studio
 	2. Copy `build.gradle` to `SDL/android-project/app/`. If you look inside `build.gradle`, you'll see `"../../../CMakeLists.txt"` as the path to the CMakeLists file. This points to the CMakeLists in this repo's root directory after it is copied. 
 	3. `cd SDL/android-project/`
  	4. Create `local.properties` in this directory so Gradle can find your Android SDK (and NDK if needed), for example:
		```
		sdk.dir=/path/to/Android/Sdk
		ndk.dir=/path/to/Android/Sdk/ndk/<version>
		```
  	5. Optional: limit ABIs in `app/build.gradle` with `abiFilters` to skip targets you do not need, for example:
		```
		abiFilters 'arm64-v8a'
		```
  	6. Build and install debug on a connected device/emulator:
		- `./gradlew assembleDebug installDebug`
	- For build + sign + install of the arm64 release APK from the repo root:
		- `./config/android-build-sign-install-arm64.sh`
	- For hotreload updates without reinstalling APK:
	1. Keep the app running on device.
	2. Run `./config/android-push-hotreload.sh --abi arm64-v8a`
	3. This rebuilds `libapp_hotreload.so` and copies it to `/data/user/0/org.viktorfilinkov.klappt/files/libapp_hotreload.so` via `adb` + `run-as`.
