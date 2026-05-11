#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
src_dir="${repo_root}/build/web"
dst_dir="${repo_root}/../klappt-web"

required_files=(
  "klappt.html"
  "klappt.js"
  "klappt.wasm"
  "klappt.data"
)

for file in "${required_files[@]}"; do
  if [[ ! -f "${src_dir}/${file}" ]]; then
    echo "missing build artifact: ${src_dir}/${file}" >&2
    exit 1
  fi
done

mkdir -p "${dst_dir}"

cp "${src_dir}/klappt.html" "${dst_dir}/index.html"
cp "${src_dir}/klappt.js" "${dst_dir}/klappt.js"
cp "${src_dir}/klappt.wasm" "${dst_dir}/klappt.wasm"
cp "${src_dir}/klappt.data" "${dst_dir}/klappt.data"

echo "copied web build artifacts to ${dst_dir}"
