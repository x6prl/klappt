#!/bin/bash
cp SDL/android-project/app/src/main/java/org/libsdl/app/* android/app/src/main/java/org/libsdl/app/
find android/app/src/main/java/org/libsdl/app/ -type f -name "*.java" -exec sed -i 's/org\.libsdl\.app/org.viktorfilinkov.klappt/g' {} +
