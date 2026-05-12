# Klappt

Klappt is a small vocabulary learning app built with C++23, SDL3, SDL_ttf,
Clay UI, Xapian, and LMDB. You add words to a learning list and review them
with spaced repetition. Learning modes progress from recognizing the whole
answer to filling gaps, rebuilding chunks, and composing the answer.

The base platforms are Android, iOS, and Web. Linux is used for development.

## Requirements

- Git submodules initialized:
  ```sh
  git submodule update --init --recursive
  ```
- Android Studio, Android SDK, and NDK for Android.
- Xcode and CMake for iOS.
- Emscripten SDK for Web.
- CMake 3.16+ and a C++23 compiler for Linux development.

## Android

The Android project lives under SDL's Android template. Copy this repository's
Gradle file into the app module:

```sh
cp build.gradle SDL/android-project/app/build.gradle
cd SDL/android-project
```

Create `SDL/android-project/local.properties` so Gradle can find your SDK and
NDK:

```properties
sdk.dir=/path/to/Android/Sdk
ndk.dir=/path/to/Android/Sdk/ndk/<version>
```

Build and install on a connected device or emulator:

```sh
./gradlew assembleDebug installDebug
```

The APK is produced under `SDL/android-project/app/build/outputs/apk/`.

To build, sign, and install the `arm64-v8a` release APK from the repository
root:

```sh
./config/android-build-sign-install-arm64.sh
```

For Android Debug hot reload, keep the app running and run from the repository
root:

```sh
./config/android-push-hotreload.sh --abi arm64-v8a
```

## iOS

Generate the Xcode project from the repository root:

```sh
./config/config-ios-xcode.sh
```

Then open `build/ios/klappt.xcodeproj` in Xcode, select the `klappt` scheme,
choose an iOS device or simulator, and build/run.

You can also build from the command line:

```sh
cmake --build build/ios --config Debug
```

## Web

```sh
source /path/to/emsdk/emsdk_env.sh
./config/config-web-unix.sh
cmake --build build/web
cd build/web
python3 -m http.server
```

Open `http://localhost:8000/klappt.html` in a browser.

## Linux Development

```sh
cmake -S . -B build \
                  -DTRACY_ENABLED=OFF \
                  -DENABLE_HOTRELOAD=ON \
                  -DCMAKE_BUILD_TYPE=Debug \
                  -DSDL_X11_XCURSOR=OFF \
                  -DSDL_X11_XINPUT=OFF \
                  -DSDL_X11_XFIXES=OFF \
                  -DSDL_X11_XRANDR=OFF \
                  -DSDL_X11_XTEST=OFF
cmake --build build
./build/Debug/klappt
```

Debug builds support hot reload. To rebuild only the reloadable module:

```sh
cmake --build build --target app_hotreload
```

To enable Tracy profiling on native builds, configure with `TRACY_ENABLED`:

```sh
cmake -S . -B build-prof \                                                                                                           master ✚ ✱
                       -DTRACY_ENABLED=ON \
                       -DENABLE_HOTRELOAD=OFF \
                       -DCMAKE_BUILD_TYPE=Debug \
                       -DSDL_X11_XCURSOR=OFF \
                       -DSDL_X11_XINPUT=OFF \
                       -DSDL_X11_XFIXES=OFF \
                       -DSDL_X11_XRANDR=OFF \
                       -DSDL_X11_XTEST=OFF
cmake --build build-prof -j4 --target klappt
./build-prof/Debug/klappt
```

Then open the Tracy UI (`tracy-profiler`) and connect to the running app.
`TRACY_ENABLED` is not supported for Web/Emscripten builds.
