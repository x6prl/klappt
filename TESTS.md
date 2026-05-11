# Tests

This project currently has CTest-backed unit test targets:

- `str_view_tests`
- `arr_tests`
- `tokenizer_tests`

The test source lives in [tests/str_view_test.cpp](/home/x/src/lexi-sdl/tests/str_view_test.cpp).

## Desktop Build

Configure the project with CMake:

```sh
cmake -S . -B build
```

Build the test target:

```sh
cmake --build build --target str_view_tests -j4
```

## Running Tests

Run the `StrView` unit tests directly with CTest:

```sh
ctest --test-dir build -R str_view_tests --output-on-failure
```

Run all registered CTest tests:

```sh
ctest --test-dir build --output-on-failure
```

## Clean Rebuild

If CMake state is stale, reconfigure before rebuilding:

```sh
cmake -S . -B build
cmake --build build --target str_view_tests -j4
ctest --test-dir build -R str_view_tests --output-on-failure
```

## Android Note

`str_view_tests` is not added to Android builds. The Android Gradle/CMake integration builds the app targets, not the desktop unit test executable.
