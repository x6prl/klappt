#!/usr/bin/env bash
set -euo pipefail

cd "${0%/*}/.."
ROOT_DIR="$(pwd)"
ANDROID_DIR="$ROOT_DIR/SDL/android-project"

ABI="arm64-v8a"
PKG="org.viktorfilinkov.klappt"
DEVICE_SERIAL=""

usage() {
  cat <<'EOF'
Usage: config/android-push-hotreload.sh [options]

Builds libapp_hotreload.so for one Android ABI and pushes it to:
  /data/user/0/<package>/files/libapp_hotreload.so

Options:
  --abi <abi>         ABI to build (default: arm64-v8a)
  --package <pkg>     Android package (default: org.viktorfilinkov.klappt)
  --serial <serial>   ADB device serial (optional)
  -h, --help          Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --abi)
      ABI="$2"
      shift 2
      ;;
    --package)
      PKG="$2"
      shift 2
      ;;
    --serial)
      DEVICE_SERIAL="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if ! command -v adb >/dev/null 2>&1; then
  echo "adb not found in PATH" >&2
  exit 1
fi

ADB=(adb)
if [[ -n "$DEVICE_SERIAL" ]]; then
  ADB+=( -s "$DEVICE_SERIAL" )
fi

echo "Checking device connection..."
"${ADB[@]}" get-state >/dev/null

echo "Building app_hotreload for ABI: $ABI"
(
  cd "$ANDROID_DIR"
  ./gradlew ":app:buildCMakeDebug[$ABI]"
)

SO_PATH="$(find "$ANDROID_DIR/app/.cxx/Debug" -path "*/$ABI/Debug/libapp_hotreload.so" | head -n 1 || true)"
if [[ -z "$SO_PATH" ]]; then
  echo "Could not find libapp_hotreload.so for ABI $ABI" >&2
  exit 1
fi

REMOTE_TMP="/data/local/tmp/libapp_hotreload.so"
REMOTE_DST="files/libapp_hotreload.so"
REMOTE_NEW="${REMOTE_DST}.new"

echo "Pushing $SO_PATH"
"${ADB[@]}" push "$SO_PATH" "$REMOTE_TMP" >/dev/null

echo "Copying into app private storage via run-as ($PKG)"
"${ADB[@]}" shell run-as "$PKG" cp "$REMOTE_TMP" "$REMOTE_NEW"
"${ADB[@]}" shell run-as "$PKG" chmod 700 "$REMOTE_NEW"
"${ADB[@]}" shell run-as "$PKG" mv -f "$REMOTE_NEW" "$REMOTE_DST"
"${ADB[@]}" shell run-as "$PKG" ls -l "$REMOTE_DST"
"${ADB[@]}" shell rm -f "$REMOTE_TMP"

echo "Updated hotreload module at /data/user/0/$PKG/$REMOTE_DST"
