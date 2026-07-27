cmake_minimum_required(VERSION 3.16)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# prevent installing to system directories.
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

# Declare the project
project(klappt)

include(CTest)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Og")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fwrapv -fsanitize=address,undefined,thread ")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Og -fno-omit-frame-pointer")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

if ((APPLE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin") OR EMSCRIPTEN)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")    # Disable shared builds on platforms where it does not make sense to use them
    set(SDL_SHARED OFF)
else()
    set(SDL_SHARED ON)
endif()

# Hotreload is only supported for Linux and Android builds.
option(ENABLE_HOTRELOAD "Enable Debug hotreload where supported" ON)
option(ENABLE_NEURO "Enable Sherpa onnx for TTS and ASR" ON)
if (EMSCRIPTEN)  # TODO: make it work
    set(ENABLE_NEURO OFF)
endif()
option(ENABLE_TRACY "Enable Tracy profiling instrumentation" OFF)
set(HOTRELOAD_SUPPORTED_PLATFORM OFF)
if ((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID) AND NOT EMSCRIPTEN)
    set(HOTRELOAD_SUPPORTED_PLATFORM ON)
endif()
set(USE_HOTRELOAD OFF)
if (ENABLE_HOTRELOAD AND HOTRELOAD_SUPPORTED_PLATFORM)
    set(USE_HOTRELOAD ON)
endif()

if(ENABLE_TRACY AND EMSCRIPTEN)
    message(FATAL_ERROR "ENABLE_TRACY is not supported for Emscripten builds")
endif()

if(MSVC)
    if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
        add_definitions(/MP)				# parallelize each target, unless Ninja is the generator
    endif()
endif()

if(ENABLE_NEURO)
    # ==========================================
    # 1. Sherpa-ONNX Configuration & Integration
    # ==========================================
    if (ANDROID)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Force static libraries build" FORCE)
        # CRITICAL FIX FOR LINUX & ANDROID HOT RELOAD:
        set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Enable PIC for static libs" FORCE)
    else()
        set(BUILD_SHARED_LIBS ON CACHE BOOL "Force dynamic libraries build" FORCE)
    endif()

    # Enable C api
    set(SHERPA_ONNX_ENABLE_C_API ON CACHE BOOL "Enable C API" FORCE)
    # Disable unnecessary Sherpa-ONNX features
    set(SHERPA_ONNX_ENABLE_PYTHON OFF CACHE BOOL "Disable Python bindings" FORCE)
    set(SHERPA_ONNX_ENABLE_TESTS OFF CACHE BOOL "Disable Tests" FORCE)
    set(SHERPA_ONNX_ENABLE_CHECK OFF CACHE BOOL "Disable Checks" FORCE)
    set(SHERPA_ONNX_ENABLE_PORTAUDIO OFF CACHE BOOL "Disable PortAudio" FORCE)
    set(SHERPA_ONNX_ENABLE_JNI OFF CACHE BOOL "Disable JNI (Not needed for C++ API)" FORCE)
    set(SHERPA_ONNX_ENABLE_WEBSOCKET OFF CACHE BOOL "Disable WebSocket" FORCE)
    set(SHERPA_ONNX_ENABLE_BINARY OFF CACHE BOOL "Disable CLI binaries" FORCE)
    set(SHERPA_ONNX_LINK_LIBSTDCPP_STATICALLY OFF CACHE BOOL "Disable static libstdc++" FORCE)

    # Enable features you actually need
    set(SHERPA_ONNX_ENABLE_TTS ON CACHE BOOL "Enable Text-to-Speech support" FORCE)

    # CRITICAL: Tell Sherpa-ONNX to use our manually extracted pre-installed ORT
    set(SHERPA_ONNX_USE_PRE_INSTALLED_ONNXRUNTIME_IF_AVAILABLE ON CACHE BOOL "Use pre-installed ORT" FORCE)

    # ==========================================
    # 2. Pre-fetch & Extract ONNX Runtime
    # ==========================================
    set(ORT_VERSION "1.26.0")
    # Persistent cache outside the build directory
    set(ORT_CACHE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/ort_zips")
    file(MAKE_DIRECTORY "${ORT_CACHE_DIR}")

    # Helper function to download, cache, and extract ONNX Runtime
    function(fetch_and_extract_ort EXPECTED_ZIP_NAME DOWNLOAD_URL)
        # Isolate extraction to a specific folder per zip to avoid architecture conflicts
        string(REGEX REPLACE "\\.[^.]*$" "" ZIP_BASENAME "${EXPECTED_ZIP_NAME}")
        if(ZIP_BASENAME MATCHES "\\.tar$")
            string(REGEX REPLACE "\\.tar$" "" ZIP_BASENAME "${ZIP_BASENAME}")
        endif()

        set(LOCAL_ZIP_PATH "${ORT_CACHE_DIR}/${EXPECTED_ZIP_NAME}")
        set(ORT_BUILD_DIR "${CMAKE_BINARY_DIR}/onnxruntime_deps/${ZIP_BASENAME}")
        file(MAKE_DIRECTORY "${ORT_BUILD_DIR}")

        set(ORT_LIB_DIR "")
        set(ORT_INC_DIR "")

        # 1. Check if already extracted in the build directory
        set(FOUND_LIBS "")
        if(WIN32)
            file(GLOB_RECURSE FOUND_LIBS "${ORT_BUILD_DIR}/**/onnxruntime.lib" "${ORT_BUILD_DIR}/**/onnxruntime.dll")
        elseif(APPLE)
            file(GLOB_RECURSE FOUND_LIBS "${ORT_BUILD_DIR}/**/libonnxruntime.a" "${ORT_BUILD_DIR}/**/libonnxruntime.dylib")
        else()
            # Linux, Android, Emscripten
            file(GLOB_RECURSE FOUND_LIBS "${ORT_BUILD_DIR}/**/libonnxruntime.a" "${ORT_BUILD_DIR}/**/libonnxruntime.so")
        endif()

        if(FOUND_LIBS)
            list(GET FOUND_LIBS 0 FIRST_LIB)
            get_filename_component(ORT_LIB_DIR "${FIRST_LIB}" DIRECTORY)

            # Include dir is typically 'include' or 'headers' one level up from 'lib'
            if(EXISTS "${ORT_LIB_DIR}/../include")
                get_filename_component(ORT_INC_DIR "${ORT_LIB_DIR}/../include" ABSOLUTE)
            elseif(EXISTS "${ORT_LIB_DIR}/../headers")
                get_filename_component(ORT_INC_DIR "${ORT_LIB_DIR}/../headers" ABSOLUTE)
            endif()
        endif()

        if(NOT ORT_LIB_DIR OR NOT ORT_INC_DIR)
            message(STATUS "ONNX Runtime not found in build dir. Preparing ${EXPECTED_ZIP_NAME}...")

            # 2. Download if missing from persistent cache
            if(NOT EXISTS "${LOCAL_ZIP_PATH}")
                message(STATUS "Downloading ONNX Runtime (cached locally)...")
                file(DOWNLOAD "${DOWNLOAD_URL}" "${LOCAL_ZIP_PATH}"
                    SHOW_PROGRESS
                    STATUS ORT_DOWNLOAD_STATUS
                    HTTPHEADER "User-Agent: Mozilla/5.0"
                )
                list(GET ORT_DOWNLOAD_STATUS 0 ORT_DOWNLOAD_CODE)
                if(ORT_DOWNLOAD_CODE)
                    list(GET ORT_DOWNLOAD_STATUS 1 ORT_DOWNLOAD_MSG)
                    message(FATAL_ERROR "Failed to download ONNX Runtime from ${DOWNLOAD_URL}. Error: ${ORT_DOWNLOAD_MSG}")
                endif()
            else()
                message(STATUS "Using locally cached ONNX Runtime zip: ${LOCAL_ZIP_PATH}")
            endif()

            # 3. Extract to build directory
            message(STATUS "Extracting ONNX Runtime to build directory: ${ORT_BUILD_DIR}")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xf "${LOCAL_ZIP_PATH}"
                WORKING_DIRECTORY "${ORT_BUILD_DIR}"
                RESULT_VARIABLE ORT_EXTRACT_RESULT
            )
            if(ORT_EXTRACT_RESULT)
                message(FATAL_ERROR "Failed to extract ONNX Runtime zip")
            endif()

            # 4. Find the lib and include dirs again after extraction
            set(FOUND_LIBS "")
            if(WIN32)
                file(GLOB_RECURSE FOUND_LIBS "${ORT_BUILD_DIR}/**/onnxruntime.lib" "${ORT_BUILD_DIR}/**/onnxruntime.dll")
            elseif(APPLE)
                file(GLOB_RECURSE FOUND_LIBS "${ORT_BUILD_DIR}/**/libonnxruntime.a" "${ORT_BUILD_DIR}/**/libonnxruntime.dylib")
            else()
                file(GLOB_RECURSE FOUND_LIBS "${ORT_BUILD_DIR}/**/libonnxruntime.a" "${ORT_BUILD_DIR}/**/libonnxruntime.so")
            endif()

            if(FOUND_LIBS)
                list(GET FOUND_LIBS 0 FIRST_LIB)
                get_filename_component(ORT_LIB_DIR "${FIRST_LIB}" DIRECTORY)
                if(EXISTS "${ORT_LIB_DIR}/../include")
                    get_filename_component(ORT_INC_DIR "${ORT_LIB_DIR}/../include" ABSOLUTE)
                elseif(EXISTS "${ORT_LIB_DIR}/../headers")
                    get_filename_component(ORT_INC_DIR "${ORT_LIB_DIR}/../headers" ABSOLUTE)
                endif()
            endif()

            if(NOT ORT_LIB_DIR OR NOT ORT_INC_DIR)
                message(FATAL_ERROR "Extracted ONNX Runtime but could not find lib/include directories inside ${ORT_BUILD_DIR}")
            endif()
        else()
            message(STATUS "Found extracted ONNX Runtime in build dir: ${ORT_LIB_DIR}")
        endif()

        # Expose to parent scope via Environment Variables (Required by sherpa-onnx)
        set(ENV{SHERPA_ONNXRUNTIME_LIB_DIR} "${ORT_LIB_DIR}")
        set(ENV{SHERPA_ONNXRUNTIME_INCLUDE_DIR} "${ORT_INC_DIR}")

        # Also set CMake variables for compatibility
        set(SHERPA_ONNXRUNTIME_LIB_DIR "${ORT_LIB_DIR}" CACHE PATH "" FORCE)
        set(SHERPA_ONNXRUNTIME_INCLUDE_DIR "${ORT_INC_DIR}" CACHE PATH "" FORCE)
        set(SHERPA_ONNXRUNTIME_LIB_DIR "${ORT_LIB_DIR}" PARENT_SCOPE)
        set(SHERPA_ONNXRUNTIME_INCLUDE_DIR "${ORT_INC_DIR}" PARENT_SCOPE)
    endfunction()

    # ==========================================
    # Platform Specific Setup
    # ==========================================
    if(ANDROID)
        set(ORT_ARCH "${CMAKE_ANDROID_ARCH_ABI}")
        set(ORT_ZIP_NAME "onnxruntime-android-${ORT_ARCH}-static_lib-${ORT_VERSION}.zip")
        set(ORT_URL "https://github.com/csukuangfj/onnxruntime-libs/releases/download/v${ORT_VERSION}/${ORT_ZIP_NAME}")
        fetch_and_extract_ort("${ORT_ZIP_NAME}" "${ORT_URL}")
        set(SHERPA_ONNX_ENABLE_ONNXRUNTIME_NDK ON CACHE BOOL "" FORCE)

    elseif(EMSCRIPTEN)
        set(SHERPA_ONNX_ENABLE_WASM ON CACHE BOOL "Enable WASM for Emscripten" FORCE)
        set(ORT_ZIP_NAME "onnxruntime-wasm-static_lib-simd-${ORT_VERSION}.zip")
        set(ORT_URL "https://github.com/csukuangfj/onnxruntime-libs/releases/download/v${ORT_VERSION}/${ORT_ZIP_NAME}")
        fetch_and_extract_ort("${ORT_ZIP_NAME}" "${ORT_URL}")

    else()
        # Desktop environments (Windows, macOS, Linux)
        # Note: Adjust these names if you switch BUILD_SHARED_LIBS to OFF (use static_lib zips)
        if(WIN32)
            set(ORT_ARCH "x64")
            set(ORT_ZIP_NAME "onnxruntime-win-${ORT_ARCH}-MD-Release-${ORT_VERSION}.tar.bz2")
            set(ORT_URL "https://github.com/csukuangfj/onnxruntime-libs/releases/download/v${ORT_VERSION}/${ORT_ZIP_NAME}")
        elseif(APPLE)
            set(ORT_ARCH "universal2")
            set(ORT_ZIP_NAME "onnxruntime-osx-${ORT_ARCH}-${ORT_VERSION}.zip")
            set(ORT_URL "https://github.com/csukuangfj/onnxruntime-libs/releases/download/v${ORT_VERSION}/${ORT_ZIP_NAME}")
        elseif(UNIX)
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
                set(ORT_ARCH "aarch64")
            else()
                set(ORT_ARCH "x64")
            endif()
            set(ORT_ZIP_NAME "onnxruntime-linux-${ORT_ARCH}-glibc2_28-Release-${ORT_VERSION}.zip")
            set(ORT_URL "https://github.com/csukuangfj/onnxruntime-libs/releases/download/v${ORT_VERSION}/${ORT_ZIP_NAME}")
        endif()

        if(ORT_ZIP_NAME)
            fetch_and_extract_ort("${ORT_ZIP_NAME}" "${ORT_URL}")
        endif()
    endif()

    # Add the source code of sherpa-onnx
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sherpa-onnx" ${CMAKE_BINARY_DIR}/sherpa-onnx EXCLUDE_FROM_ALL)
    # ==========================================
    # ================ SHERPA ONNX ========================
endif()

if(NOT EMSCRIPTEN)
    #######
    # cURL
    #######
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CURL_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)

    set(CURL_DISABLE_FTP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_FILE ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_RTSP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_DICT ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_TELNET ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_TFTP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_POP3 ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_IMAP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_SMB ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_SMTP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_GOPHER ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_MQTT ON CACHE BOOL "" FORCE)

    set(CURL_DISABLE_PROXY ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_COOKIES ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_MIME ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_NETRC ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_ALTSVC ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_HSTS ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_DOH ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_WEBSOCKETS ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_ZSTD ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_VERBOSE_STRINGS OFF CACHE BOOL "" FORCE)

    set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
    set(_OLD_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
    if (ANDROID)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        # set(EXECUTABLE_NAME ${PROJECT_NAME})
        set(CURL_USE_MBEDTLS OFF CACHE BOOL "" FORCE)
        set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
        set(CURL_USE_SECTRANSP OFF CACHE BOOL "" FORCE)
        set(CURL_USE_SCHANNEL OFF CACHE BOOL "" FORCE)
        set(CURL_USE_WOLFSSL OFF CACHE BOOL "" FORCE)
        set(CURL_USE_GNUTLS OFF CACHE BOOL "" FORCE)

        # set(MBEDTLS_INCLUDE_DIR ${mbedtls_SOURCE_DIR}/include CACHE STRING "" FORCE)
        # set(MBEDTLS_LIBRARY mbedtls CACHE STRING "" FORCE)
        # set(MBEDX509_LIBRARY mbedx509 CACHE STRING "" FORCE)
        # set(MBEDCRYPTO_LIBRARY mbedcrypto CACHE STRING "" FORCE)
        #
        # set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
        add_subdirectory(src/vendor/boringssl)

        set(BORINGSSL_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/src/vendor/boringssl")
        set(BORINGSSL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/boringssl")

        add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
        set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LOCATION "${BORINGSSL_BIN_DIR}/libssl.a"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/boringssl/include"
        )
        #
        add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LOCATION "${BORINGSSL_BIN_DIR}/libcrypto.a"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/boringssl/include"
        )

        if(NOT TARGET OpenSSL::SSL)
            add_library(OpenSSL::SSL ALIAS ssl)
        endif()
        if(NOT TARGET OpenSSL::Crypto)
            add_library(OpenSSL::Crypto ALIAS crypto)
        endif()
        # 3. Set cache variables so libcurl's internal checks know OpenSSL is "found"
        # set(OPENSSL_FOUND ON CACHE BOOL "" FORCE)
        # set(OpenSSL_FOUND ON CACHE BOOL "" FORCE)
        set(OPENSSL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/boringssl/include" CACHE PATH "" FORCE)

        set(OPENSSL_FOUND ON CACHE BOOL "" FORCE)
        set(OPENSSL_INCLUDE_DIR "${BORINGSSL_SRC_DIR}/include" CACHE PATH "" FORCE)
        set(OPENSSL_SSL_LIBRARY "${BORINGSSL_BIN_DIR}/libssl.a" CACHE FILEPATH "" FORCE)
        set(OPENSSL_CRYPTO_LIBRARY "${BORINGSSL_BIN_DIR}/libcrypto.a" CACHE FILEPATH "" FORCE)
        set(OPENSSL_ROOT_DIR "${BORINGSSL_BIN_DIR}" CACHE PATH "" FORCE)

        set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
    elseif(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
        set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
        set(CURL_USE_SRP OFF CACHE BOOL "" FORCE)
        find_package(OpenSSL REQUIRED)
        if(NOT OPENSSL_FOUND)
            message(FATAL_ERROR "System OpenSSL not found for Linux build. Install libssl-dev or equivalent.")
        endif()

    elseif(APPLE AND NOT EMSCRIPTEN)
        set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
        find_package(OpenSSL QUIET)
        if(NOT OPENSSL_FOUND)
            message(WARNING "No system OpenSSL found for macOS build. Will try using built-in or default SSL.")
        endif()

    elseif(EMSCRIPTEN)
        set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    endif()
    message(STATUS "OPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}")
    message(STATUS "OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}")
    message(STATUS "OPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}")
    message(STATUS "OPENSSL_CRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}")

    add_subdirectory(src/vendor/curl)
    # set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS ${_OLD_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)
    #######
    ####### CURL ###########
endif()

# Set the name of the executable
set(EXECUTABLE_NAME ${PROJECT_NAME})

# Create an executable or a shared library based on the platform and add our sources to it
if (ANDROID)
    # The SDL java code is hardcoded to load libmain.so on android, so we need to change EXECUTABLE_NAME
    set(EXECUTABLE_NAME main)
    add_library(${EXECUTABLE_NAME} SHARED)
else()
    add_executable(${EXECUTABLE_NAME})
endif()

# Hot-reloadable sources
file(GLOB APP_HOTRELOAD_FILES CONFIGURE_DEPENDS
    src/ui/*.cpp
    src/ui/components/*.cpp
)
list(REMOVE_ITEM APP_HOTRELOAD_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/clay_impl.cpp"
)
set(APP_UI_IMPL_FILES
    src/ui/clay_impl.cpp
)
set(APP_CORE_FILES
    src/app/worker.cpp
    src/platform/net_worker.cpp
)
set(APP_PLATFORM_FILES
    src/platform/web_persist.cpp
    src/platform/audio.cpp
    src/platform/zip.cpp
    src/platform/fs.cpp
    $<$<BOOL:${ENABLE_NEURO}>:src/platform/neuro.cpp>
)
set(APP_BASE_FILES
    src/base/str_view.cpp
    src/base/hash.cpp
    src/base/shuffle.cpp
)
set(APP_DOMAIN_FILES
    src/domain/engine.cpp
    src/domain/exercises.cpp
    src/domain/wparser.cpp
    src/domain/word_store.cpp
    src/domain/tokenizer.cpp
)
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_include_directories(${EXECUTABLE_NAME} SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor"
)
target_include_directories(${EXECUTABLE_NAME} SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/xxHash"
)

if (ENABLE_NEURO)
    target_include_directories(${EXECUTABLE_NAME} SYSTEM PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sherpa-onnx"
    )
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE sherpa-onnx-core sherpa-onnx-c-api)
endif()

#
add_library(miniz STATIC
    src/vendor/miniz/miniz.c
)
target_link_libraries(${EXECUTABLE_NAME} PRIVATE miniz)

add_library(simdjson STATIC
    src/vendor/simdjson/simdjson.cpp
)
target_link_libraries(${EXECUTABLE_NAME} PRIVATE simdjson)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(${EXECUTABLE_NAME} PRIVATE
        -Wall
        -Wextra
        # -Wfloat-equal
        -Wno-missing-field-initializers
        -ffast-math # TODO: check the impact
    )
endif()

target_compile_definitions(${EXECUTABLE_NAME}
PRIVATE
    $<$<AND:$<BOOL:${USE_HOTRELOAD}>,$<CONFIG:Debug>>:HOTRELOAD=1>
    $<$<BOOL:${ENABLE_NEURO}>:NEURO=1>
)
if(ANDROID)
    target_compile_definitions(${EXECUTABLE_NAME}
    PRIVATE
        $<$<AND:$<BOOL:${USE_HOTRELOAD}>,$<CONFIG:Debug>>:HOTRELOAD_MODULE_PATH=\"/data/user/0/org.viktorfilinkov.klappt/files/libapp_hotreload.so\">
    )
endif()

if(USE_HOTRELOAD)
    add_library(app_hotreload SHARED
        ${APP_HOTRELOAD_FILES}
        ${APP_UI_IMPL_FILES}
        # ${APP_CORE_FILES}
        # ${APP_UI_IMPL_FILES}
        # ${APP_BASE_FILES}
        # ${APP_DOMAIN_FILES}
        # ${APP_PLATFORM_FILES}
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_options(${EXECUTABLE_NAME} PRIVATE
            -Wall
            -Wextra
            -Wno-missing-field-initializers
            -Og
    )
    endif()
    set_source_files_properties(src/base/hash.cpp PROPERTIES
        COMPILE_OPTIONS "-O2"
    )
    target_compile_features(app_hotreload PUBLIC cxx_std_23)
    target_compile_definitions(app_hotreload PRIVATE
        $<$<CONFIG:Debug>:HOTRELOAD=1>
        $<$<BOOL:${ENABLE_NEURO}>:NEURO=1>
    )
    target_include_directories(app_hotreload PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_include_directories(app_hotreload SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor")
    if (ENABLE_NEURO)
        target_include_directories(app_hotreload SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/sherpa-onnx")
    endif()
    target_include_directories(app_hotreload SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/curl/include")
    target_include_directories(app_hotreload SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/xxHash")
    target_include_directories(app_hotreload SYSTEM PRIVATE SDL/include)
    # target_include_directories(app_hotreload PRIVATE SDL_mixer/include)
    # target_include_directories(app_hotreload PRIVATE SDL_image/include)
    target_include_directories(app_hotreload SYSTEM PRIVATE SDL_ttf/include)

    # it builds automatically btw
    if (!ANDROID)
        add_dependencies(${EXECUTABLE_NAME} app_hotreload)
    endif()

    # Use static UI path outside Debug builds even on hotreload-capable platforms.
    target_sources(${EXECUTABLE_NAME}
    PRIVATE
        ${APP_CORE_FILES}
        ${APP_PLATFORM_FILES}
        ${APP_BASE_FILES}
        ${APP_DOMAIN_FILES}
        $<$<NOT:$<CONFIG:Debug>>:${APP_UI_IMPL_FILES}>
        $<$<NOT:$<CONFIG:Debug>>:${APP_HOTRELOAD_FILES}>
    )
else()
    # No hotreload support on this platform: always compile UI directly in main target.
    target_sources(${EXECUTABLE_NAME}
    PRIVATE
        ${APP_CORE_FILES}
        ${APP_PLATFORM_FILES}
        ${APP_BASE_FILES}
        ${APP_DOMAIN_FILES}
        ${APP_UI_IMPL_FILES}
        ${APP_HOTRELOAD_FILES}
    )
endif()

# Add your sources to the target
target_sources(${EXECUTABLE_NAME}
PRIVATE
    src/app/main.cpp
    src/platform/apple/iosLaunchScreen.storyboard
)

# What is iosLaunchScreen.storyboard? This file describes what Apple's mobile platforms
# should show the user while the application is starting up. If you don't include one,
# then you get placed in a compatibility mode that does not allow HighDPI.
# This file is referenced inside Info.plist.in, where it is marked as the launch screen file.
# It is also ignored on non-Apple platforms.

# To get an app icon on Apple platforms, add it to your executable.
# Afterward, the image file in Info.plist.in.
if(APPLE)
    target_sources("${EXECUTABLE_NAME}" PRIVATE "src/assets/logo.png")
endif()

# Set C++ version
set (CMAKE_CXX_STANDARD 23)

# on Web targets, we need CMake to generate a HTML webpage.
if(EMSCRIPTEN)
    option(EMSCRIPTEN_ALLOW_MEMORY_GROWTH "Allow the WebAssembly heap to grow at runtime" ON)
    set(EMSCRIPTEN_INITIAL_MEMORY_MB "512" CACHE STRING "Initial WebAssembly heap size in MiB")
    set(EMSCRIPTEN_MAXIMUM_MEMORY_MB "1024" CACHE STRING "Maximum WebAssembly heap size in MiB when growth is enabled")
    option(EMSCRIPTEN_USE_WASMFS "Use WasmFS for Emscripten builds." OFF)

    math(EXPR EMSCRIPTEN_INITIAL_MEMORY_BYTES "${EMSCRIPTEN_INITIAL_MEMORY_MB} * 1024 * 1024")
    math(EXPR EMSCRIPTEN_MAXIMUM_MEMORY_BYTES "${EMSCRIPTEN_MAXIMUM_MEMORY_MB} * 1024 * 1024")

    set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")

    target_link_options(${EXECUTABLE_NAME} PRIVATE
        "-sINITIAL_MEMORY=${EMSCRIPTEN_INITIAL_MEMORY_BYTES}"
        "-lidbfs.js"
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web/web_persist.js"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web/web_shell.html"
    )

    if(EMSCRIPTEN_USE_WASMFS)
        target_link_options(${EXECUTABLE_NAME} PRIVATE
            "-sWASMFS=1"
        )
    endif()

    if(EMSCRIPTEN_ALLOW_MEMORY_GROWTH)
        target_link_options(${EXECUTABLE_NAME} PRIVATE
            "-sALLOW_MEMORY_GROWTH=1"
            "-sMAXIMUM_MEMORY=${EMSCRIPTEN_MAXIMUM_MEMORY_BYTES}"
            "-sSTACK_SIZE=1048576"
        )
    endif()

    # Keep C++ exceptions catchable in browser builds so third-party library
    # failures surface through normal error handling instead of hard aborting.
    # target_compile_options(${EXECUTABLE_NAME} PRIVATE
    #     "-fexceptions"
    # )
    # target_link_options(${EXECUTABLE_NAME} PRIVATE
    #     "-fexceptions"
    #     "-sNO_DISABLE_EXCEPTION_CATCHING"
    # )
endif()

# Configure SDL by calling its CMake file.
# we use EXCLUDE_FROM_ALL so that its install targets and configs don't
# pollute upwards into our configuration.
if(ENABLE_TRACY)
    target_link_options(${EXECUTABLE_NAME} PRIVATE -rdynamic)
    set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy profiling" FORCE)
    add_subdirectory(tracy EXCLUDE_FROM_ALL)
endif()

add_subdirectory(SDL EXCLUDE_FROM_ALL)

# If you don't want SDL_ttf, then remove this section.
set(SDLTTF_VENDORED ON) # tell SDL_ttf to build its own dependencies
add_subdirectory(SDL_ttf EXCLUDE_FROM_ALL)

# SDL_mixer (kept commented for future re-enable)
# set(SDLMIXER_MIDI_NATIVE OFF)     # disable formats we don't use to make the build faster and smaller. Also some of these don't work on all platforms so you'll need to do some experimentation.
# set(SDLMIXER_GME OFF)
# set(SDLMIXER_WAVPACK OFF)
# set(SDLMIXER_MOD OFF)
# set(SDLMIXER_MOD_XMP OFF)
# set(SDLMIXER_OPUS OFF)
# set(SDLMIXER_MP3_MPG123 OFF)
# set(SDLMIXER_VORBIS_VORBISFILE OFF)
# set(SDLMIXER_FLAC_LIBFLAC OFF)
# set(SDLMIXER_VENDORED ON)   # tell SDL_mixer to build its own dependencies
# add_subdirectory(SDL_mixer EXCLUDE_FROM_ALL)

# SDL_image (kept commented for future re-enable)
# set(SDLIMAGE_VENDORED ON)
# set(SDLIMAGE_AVIF OFF)	# disable formats we don't use to make the build faster and smaller.
# set(SDLIMAGE_BMP OFF)
# set(SDLIMAGE_JPEG OFF)
# set(SDLIMAGE_WEBP OFF)
# set(SDLIMAGE_PNG_LIBPNG OFF)    # SDL3 includes built-in support for loading PNGs. If you need more than what that supports, enable this option.
# add_subdirectory(SDL_image EXCLUDE_FROM_ALL)

if(NOT EMSCRIPTEN)
    add_library(lmdb STATIC
        lmdb/libraries/liblmdb/mdb.c
        lmdb/libraries/liblmdb/midl.c
    )
    target_include_directories(lmdb PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/lmdb/libraries/liblmdb"
    )
endif()

add_subdirectory(cmake/xapian-core "${CMAKE_BINARY_DIR}/xapian/xapian-core" EXCLUDE_FROM_ALL)
target_include_directories(${EXECUTABLE_NAME} SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/xapian/xapian-core/include"
)

# Link SDL to our executable. This also makes its include directory available to us.
target_link_libraries(${EXECUTABLE_NAME} PUBLIC
    xapian::xapian
    SDL3_ttf::SDL3_ttf      # remove if you are not using SDL_ttf
    # SDL3_mixer::SDL3_mixer  # re-enable when SDL_mixer is used again
    # SDL3_image::SDL3_image	# re-enable when SDL_image is used again
)

# Add linker options to group libraries for proper symbol resolution
# This allows the linker to search through the group multiple times
target_link_options(${EXECUTABLE_NAME} PRIVATE "LINKER:--start-group")
# Allow hotreload module use symbols from main executable
if (USE_HOTRELOAD)
    target_link_options(${EXECUTABLE_NAME} PRIVATE "-rdynamic")
    if(ANDROID)
        # Link app_hotreload library directly to 'main'.
        # This allows the Android linker to find symbols like StrView or job_push
        # inside the libmain.so that is already loaded in memory.
        target_link_libraries(app_hotreload PRIVATE ${EXECUTABLE_NAME})

        # set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        #     CXX_VISIBILITY_PRESET default
        #     VISIBILITY_INLINES_HIDDEN OFF
        # )
    else()
        # on Linux -rdynamic works for that purpose
        target_link_options(${EXECUTABLE_NAME} PRIVATE "-rdynamic")
        # set_target_properties(${EXECUTABLE_NAME} PROPERTIES ENABLE_EXPORTS TRUE)
    endif()
endif()

# Link sherpa dependencies and SDL3 with explicit ordering
# Libraries that depend on others should come BEFORE their dependencies
if (ENABLE_NEURO)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        sherpa-onnx-core
        $<TARGET_NAME_IF_EXISTS:kaldi-native-fbank-core>
        $<TARGET_NAME_IF_EXISTS:kaldi-decoder-core>
        $<TARGET_NAME_IF_EXISTS:kissfft-float>
        $<TARGET_NAME_IF_EXISTS:espeak-ng>
        $<TARGET_NAME_IF_EXISTS:ucd>
        $<TARGET_NAME_IF_EXISTS:piper_phonemize>
        $<TARGET_NAME_IF_EXISTS:sherpa-onnx-kaldifst-core>
        $<TARGET_NAME_IF_EXISTS:ssentencepiece_core>
        $<TARGET_NAME_IF_EXISTS:sherpa-onnx-fstfar>
        $<TARGET_NAME_IF_EXISTS:sherpa-onnx-fst>
        SDL3::SDL3
    )
else()
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        SDL3::SDL3
    )
endif()
# End the linker group
target_link_options(${EXECUTABLE_NAME} PRIVATE "LINKER:--end-group")

if(ANDROID)
    target_link_libraries(${EXECUTABLE_NAME} PUBLIC
      log
      android
      ssl
      crypto
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND OPENSSL_FOUND)
    message(STATUS "Linking against system OpenSSL on Linux")
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(NOT EMSCRIPTEN)
    target_link_libraries(${EXECUTABLE_NAME} PUBLIC lmdb)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE CURL::libcurl )
endif()

if(ENABLE_TRACY)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE Tracy::TracyClient)
endif()

if(USE_HOTRELOAD)
    target_link_libraries(app_hotreload PUBLIC
        lmdb
        xapian::xapian
        SDL3_ttf::SDL3_ttf

        # sherpa-onnx-c-api
        # sherpa-onnx-core
        # CURL::libcurl
        $<TARGET_NAME_IF_EXISTS:espeak-ng>
        $<TARGET_NAME_IF_EXISTS:ucd>
        $<TARGET_NAME_IF_EXISTS:ucd-tools>
        $<TARGET_NAME_IF_EXISTS:piper_phonemize>

        $<TARGET_NAME_IF_EXISTS:kaldi-native-fbank-core>
        $<TARGET_NAME_IF_EXISTS:kissfft-float>
        $<TARGET_NAME_IF_EXISTS:kaldi-decoder-core>
        $<TARGET_NAME_IF_EXISTS:sherpa-onnx-kaldifst-core>
        $<TARGET_NAME_IF_EXISTS:ssentencepiece_core>
        $<TARGET_NAME_IF_EXISTS:sherpa-onnx-fstfar>
        $<TARGET_NAME_IF_EXISTS:sherpa-onnx-fst>

        SDL3::SDL3
    )
endif()

# Dealing with assets
set(ASSETS_ROOT "${CMAKE_CURRENT_LIST_DIR}/src/assets")

# We have some non-code resources that our application needs in order to work. How we deal with those differs per platform.
if (APPLE)
    # on Apple targets, the application bundle has a "resources" subfolder
    set(input_root "${CMAKE_CURRENT_LIST_DIR}/src")
    macro(add_resource FILE)
        file(RELATIVE_PATH relpath "${input_root}" "${FILE}")
        get_filename_component(relpath "${relpath}" DIRECTORY)
        if (relpath STREQUAL "assets/fonts")
            set(relpath "")
        endif()
        target_sources(${EXECUTABLE_NAME} PRIVATE ${FILE})
        set_property(SOURCE ${FILE} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/${relpath}")
    endmacro()
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/Inter-VariableFont.ttf")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/ReadexPro-Regular.ttf")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/Font-Awesome-7-Free-Solid-900.otf")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/JetBrainsMono-Regular.ttf")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/JetBrainsMono-Bold.ttf")
    # add_resource("${CMAKE_CURRENT_LIST_DIR}/word_data/ar.txt")
    # add_resource("${CMAKE_CURRENT_LIST_DIR}/word_data/en.txt")
    # add_resource("${CMAKE_CURRENT_LIST_DIR}/word_data/ru.txt")
    # add_resource("${CMAKE_CURRENT_LIST_DIR}/word_data/tr.txt")

elseif(EMSCRIPTEN)
    # on the web, we pack files into the webassembly virtual filesystem
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/Inter-VariableFont.ttf@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/ReadexPro-Regular.ttf@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/Font-Awesome-7-Free-Solid-900.otf@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/JetBrainsMono-Regular.ttf@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/JetBrainsMono-Bold.ttf@/\""
        # "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/word_data/ar.txt@/word_data/\""
        # "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/word_data/en.txt@/word_data/\""
        # "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/word_data/ru.txt@/word_data/\""
        # "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/word_data/tr.txt@/word_data/\""
    )

else()
    if (ANDROID)
        if (NOT MOBILE_ASSETS_DIR)
            message(FATAL_ERROR "Building for Android, but MOBILE_ASSETS_DIR is not set")
        endif()
        file(MAKE_DIRECTORY "${MOBILE_ASSETS_DIR}")
        set(DEST_ROOT "${MOBILE_ASSETS_DIR}")
    else()
        set(DEST_ROOT "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
    endif()

    macro(copy_helper filename)
        get_filename_component(outdir "${DEST_ROOT}/${filename}" DIRECTORY)
        get_filename_component(filename_dir "${filename}" DIRECTORY)

        if (filename_dir STREQUAL "word_data")
            set(input_name "${CMAKE_CURRENT_LIST_DIR}/${filename}")
        else()
            set(input_name "${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/${filename}")
        endif()

        add_custom_command(TARGET "${EXECUTABLE_NAME}" POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${outdir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${input_name}" "${DEST_ROOT}/${filename}"
        )
    endmacro()

    copy_helper("Inter-VariableFont.ttf")
    copy_helper("ReadexPro-Regular.ttf")
    copy_helper("Font-Awesome-7-Free-Solid-900.otf")
    copy_helper("JetBrainsMono-Regular.ttf")
    copy_helper("JetBrainsMono-Bold.ttf")
    # copy_helper("word_data/ar.txt")
    # copy_helper("word_data/en.txt")
    # copy_helper("word_data/ru.txt")
    # copy_helper("word_data/tr.txt")

endif()

# set some extra configs for each platform
set(RESOURCE_FILES "src/assets/logo.png")
if(IOS)
    set(RESOURCE_FILES "src/platform/apple/iosLaunchScreen.storyboard;src/assets/logo.png") # make the splash screen show up on iOS
endif()

set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    # On macOS, make a proper .app bundle instead of a bare executable
    MACOSX_BUNDLE TRUE
    # Set the Info.plist file for Apple Mobile platforms. Without this file, your app
    # will not launch.
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/apple/Info.plist.in"

    # in Xcode, create a Scheme in the schemes dropdown for the app.
    XCODE_GENERATE_SCHEME TRUE
    # Identification for Xcode
    XCODE_ATTRIBUTE_BUNDLE_IDENTIFIER "org.viktorfilinkov.klappt"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.viktorfilinkov.klappt"
    XCODE_ATTRIBUTE_CURRENTYEAR "${CURRENTYEAR}"
    RESOURCE "${RESOURCE_FILES}"
)

if(NOT ANDROID)
    add_executable(codec_tests
        tests/codec_test.cpp
        src/ui/clay_impl.cpp
        src/base/str_view.cpp
    )
    target_include_directories(codec_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_include_directories(codec_tests  SYSTEM PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor"
    )
    target_link_libraries(codec_tests PRIVATE SDL3::SDL3)
    add_test(NAME codec_tests COMMAND codec_tests)
    add_executable(str_view_tests
        tests/str_view_test.cpp
        src/base/str_view.cpp
    )
    target_compile_features(str_view_tests PUBLIC cxx_std_23)
    target_include_directories(str_view_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_include_directories(str_view_tests SYSTEM PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor"
    )
    target_link_libraries(str_view_tests PRIVATE SDL3::SDL3)
    add_test(NAME str_view_tests COMMAND str_view_tests)

    add_executable(arr_tests
        tests/arr_test.cpp
    )
    target_compile_features(arr_tests PUBLIC cxx_std_23)
    target_include_directories(arr_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(arr_tests PRIVATE SDL3::SDL3)
    add_test(NAME arr_tests COMMAND arr_tests)

    add_executable(tokenizer_tests
        tests/tokenizer_test.cpp
        src/domain/tokenizer.cpp
        src/base/str_view.cpp
    )
    target_compile_features(tokenizer_tests PUBLIC cxx_std_23)
    target_include_directories(tokenizer_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_include_directories(tokenizer_tests SYSTEM PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor"
    )
    target_link_libraries(tokenizer_tests PRIVATE SDL3::SDL3)
    add_test(NAME tokenizer_tests COMMAND tokenizer_tests)

endif()

# on Visual Studio, set our app as the default project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "${EXECUTABLE_NAME}")

# On macOS Platforms, ensure that the bundle is valid for distribution by calling fixup_bundle.
# note that fixup_bundle does not work on iOS, so you will want to use static libraries
# or manually copy dylibs and set rpaths
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # tell Install about the target, otherwise fixup won't know about the transitive dependencies
    install(TARGETS ${EXECUTABLE_NAME}
     BUNDLE DESTINATION ./install COMPONENT Runtime
        RUNTIME DESTINATION ./install/bin COMPONENT Runtime
    )

    set(DEP_DIR "${CMAKE_BINARY_DIR}")  # where to look for dependencies when fixing up
    INSTALL(CODE
        "include(BundleUtilities)
        fixup_bundle(\"$<TARGET_BUNDLE_DIR:${EXECUTABLE_NAME}>\" \"\" \"${DEP_DIR}\")
        "
    )
    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()
