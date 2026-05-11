# build debug
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSDL_X11_XCURSOR=OFF -DSDL_X11_XINPUT=OFF -DSDL_X11_XFIXES=OFF -DSDL_X11_XRANDR=OFF -DSDL_X11_XTEST=OFF
cmake --build build

# clear logs
adb logcat -c
# read logs
adb logcat | rg -n "SDL/APP|F libc| SDL| libapp"

# tracy Android USB forwarding
adb forward tcp:8086 tcp:8086

# build, sign, install release
JAVA_HOME=~/android-studio/jbr ./gradlew :app:assembleRelease -x lintVitalAnalyzeRelease
~/Android/Sdk/build-tools/36.1.0/apksigner sign \
                                  --ks ~/.android/debug.keystore \
                                  --ks-pass pass:android \
                                  --key-pass pass:android \
                                  --out app/build/outputs/apk/release/app-release-test-signed.apk \
                                  app/build/outputs/apk/release/app-arm64-v8a-release-unsigned.apk
adb install -r app/build/outputs/apk/release/app-release-test-signed.apk 
