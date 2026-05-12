#!/usr/bin/env bash
set -euo pipefail

cd "${0%/*}/.."

export JAVA_HOME="${JAVA_HOME:-$HOME/android-studio/jbr}"

cd SDL/android-project
./gradlew :app:assembleRelease -x lintVitalAnalyzeRelease
cd ../..

~/Android/Sdk/build-tools/36.1.0/apksigner sign \
  --ks ~/.android/debug.keystore \
  --ks-pass pass:android \
  --key-pass pass:android \
  --out SDL/android-project/app/build/outputs/apk/release/app-release-arm64-v8a-signed.apk \
  SDL/android-project/app/build/outputs/apk/release/app-arm64-v8a-release-unsigned.apk

adb install -r SDL/android-project/app/build/outputs/apk/release/app-release-arm64-v8a-signed.apk
