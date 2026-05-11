#!/bin/bash
cd "${0%/*}"

emcmake cmake -S .. -B ../build/web \
  -DEMSCRIPTEN_ALLOW_MEMORY_GROWTH=ON \
  -DEMSCRIPTEN_INITIAL_MEMORY_MB=128
