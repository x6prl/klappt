#!/usr/bin/env bash
set -euo pipefail

cd "${0%/*}/.."

export JAVA_HOME="${JAVA_HOME:-$HOME/android-studio/jbr}"
export ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"

# Auto-detect latest build-tools
BUILD_TOOLS_DIR=$(ls -d "${ANDROID_HOME}/build-tools/"* | sort -V | tail -n 1)

echo "==> Building Release APK for arm64-v8a"
cd android
./gradlew :app:assembleRelease -x lintVitalAnalyzeRelease
cd -

APK_DIR="android/app/build/outputs/apk/release"

# Auto-detect the generated unsigned APK
UNSIGNED_APK=$(find "${APK_DIR}" -name "*unsigned.apk" | head -n 1)
if [[ -z "${UNSIGNED_APK}" ]]; then
    echo "ERROR: Could not find generated unsigned APK in ${APK_DIR}"
    exit 1
fi

ALIGNED_APK="${APK_DIR}/app-aligned.apk"
SIGNED_APK="${APK_DIR}/app-release-arm64-v8a-signed.apk"

echo "==> Zipaligning: $(basename "${UNSIGNED_APK}")"
rm -f "${ALIGNED_APK}"
"${BUILD_TOOLS_DIR}/zipalign" -v -p -f 4 "${UNSIGNED_APK}" "${ALIGNED_APK}"

echo "==> Signing with debug key for local device testing"
"${BUILD_TOOLS_DIR}/apksigner" sign \
  --ks ~/.android/debug.keystore \
  --ks-pass pass:android \
  --key-pass pass:android \
  --out "${SIGNED_APK}" \
  "${ALIGNED_APK}"

rm -f "${ALIGNED_APK}"

echo "==> Installing to connected device"
adb install -r "${SIGNED_APK}"

echo "==> Done!"
