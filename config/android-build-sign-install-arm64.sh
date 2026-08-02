#!/usr/bin/env bash
set -euo pipefail

cd "${0%/*}/.."

export JAVA_HOME="${JAVA_HOME:-$HOME/android-studio/jbr}"

cd android
./gradlew :app:assembleRelease -x lintVitalAnalyzeRelease
cd -

~/Android/Sdk/build-tools/36.1.0/apksigner sign \
  --ks ~/.android/debug.keystore \
  --ks-pass pass:android \
  --key-pass pass:android \
  --out android/app/build/outputs/apk/release/app-release-arm64-v8a-signed.apk \
  android/app/build/outputs/apk/release/app-arm64-v8a-release-unsigned.apk

adb install -r android/app/build/outputs/apk/release/app-release-arm64-v8a-signed.apk
