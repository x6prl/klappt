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
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fwrapv -fsanitize=address,undefined ")
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
option(TRACY_ENABLED "Enable Tracy profiling instrumentation" OFF)
set(HOTRELOAD_SUPPORTED_PLATFORM OFF)
if ((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID) AND NOT EMSCRIPTEN)
    set(HOTRELOAD_SUPPORTED_PLATFORM ON)
endif()
set(USE_HOTRELOAD OFF)
if (ENABLE_HOTRELOAD AND HOTRELOAD_SUPPORTED_PLATFORM)
    set(USE_HOTRELOAD ON)
endif()

if(TRACY_ENABLED AND EMSCRIPTEN)
    message(FATAL_ERROR "TRACY_ENABLED is not supported for Emscripten builds")
endif()

if(MSVC)
    if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
        add_definitions(/MP)				# parallelize each target, unless Ninja is the generator
    endif()
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
    src/domain/engine.cpp
    src/domain/exercises.cpp
    src/domain/wparser.cpp
    src/domain/word_store.cpp
    src/domain/tokenizer.cpp
    src/base/str_view.cpp
    src/base/hash.cpp
    src/base/shuffle.cpp
    src/platform/web_persist.cpp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_include_directories(${EXECUTABLE_NAME} SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor"
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(${EXECUTABLE_NAME} PRIVATE
        -Wall
        -Wextra
        -Wno-missing-field-initializers
    )
endif()

target_compile_definitions(${EXECUTABLE_NAME}
PRIVATE
    $<$<AND:$<BOOL:${USE_HOTRELOAD}>,$<CONFIG:Debug>>:HOTRELOAD=1>
)
if(ANDROID)
    target_compile_definitions(${EXECUTABLE_NAME}
    PRIVATE
        $<$<AND:$<BOOL:${USE_HOTRELOAD}>,$<CONFIG:Debug>>:HOTRELOAD_MODULE_PATH=\"/data/user/0/org.viktorfilinkov.klappt/files/libapp_hotreload.so\">
    )
endif()

if(USE_HOTRELOAD)
    add_library(app_hotreload SHARED
        ${APP_CORE_FILES}
        ${APP_UI_IMPL_FILES}
        ${APP_HOTRELOAD_FILES}
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
    )
    target_include_directories(app_hotreload PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_include_directories(app_hotreload SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor")
    target_include_directories(app_hotreload SYSTEM PRIVATE xxHash)
    target_include_directories(app_hotreload SYSTEM PRIVATE SDL/include)
    # target_include_directories(app_hotreload PRIVATE SDL_mixer/include)
    # target_include_directories(app_hotreload PRIVATE SDL_image/include)
    target_include_directories(app_hotreload SYSTEM PRIVATE SDL_ttf/include)
    # target_link_libraries(${EXECUTABLE_NAME} PUBLIC
    #     app_hotreload
    # )
    add_dependencies(${EXECUTABLE_NAME} app_hotreload)

    # Use static UI path outside Debug builds even on hotreload-capable platforms.
    target_sources(${EXECUTABLE_NAME}
    PRIVATE
        ${APP_CORE_FILES}
        ${APP_UI_IMPL_FILES}
        $<$<NOT:$<CONFIG:Debug>>:${APP_HOTRELOAD_FILES}>
    )
else()
    # No hotreload support on this platform: always compile UI directly in main target.
    target_sources(${EXECUTABLE_NAME}
    PRIVATE
        ${APP_CORE_FILES}
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
target_compile_features(${EXECUTABLE_NAME} PUBLIC cxx_std_23)
set (CMAKE_CXX_STANDARD 23)

# on Web targets, we need CMake to generate a HTML webpage.
if(EMSCRIPTEN)
    option(EMSCRIPTEN_ALLOW_MEMORY_GROWTH "Allow the WebAssembly heap to grow at runtime" ON)
    set(EMSCRIPTEN_INITIAL_MEMORY_MB "32" CACHE STRING "Initial WebAssembly heap size in MiB")
    set(EMSCRIPTEN_MAXIMUM_MEMORY_MB "512" CACHE STRING "Maximum WebAssembly heap size in MiB when growth is enabled")
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
if(TRACY_ENABLED)
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

# Link SDL to our executable. This also makes its include directory available to us.
target_link_libraries(${EXECUTABLE_NAME} PUBLIC
 xapian::xapian
 SDL3_ttf::SDL3_ttf      # remove if you are not using SDL_ttf
 # SDL3_mixer::SDL3_mixer  # re-enable when SDL_mixer is used again
 # SDL3_image::SDL3_image	# re-enable when SDL_image is used again
 SDL3::SDL3              # If using satelite libraries, SDL must be the last item in the list.
)
if(NOT EMSCRIPTEN)
    target_link_libraries(${EXECUTABLE_NAME} PUBLIC lmdb)
endif()
if(TRACY_ENABLED)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE Tracy::TracyClient)
endif()
if(USE_HOTRELOAD)
    target_link_libraries(app_hotreload PUBLIC
        lmdb
        xapian::xapian
        SDL3_ttf::SDL3_ttf
        # SDL3_mixer::SDL3_mixer
        # SDL3_image::SDL3_image
        SDL3::SDL3
    )
endif()

# Dealing with assets
# We have some non-code resources that our application needs in order to work. How we deal with those differs per platform.
if (APPLE)
    # on Apple targets, the application bundle has a "resources" subfolder where we can place our assets.
    # SDL_GetBasePath() gives us easy access to that location.
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
    add_resource("${CMAKE_CURRENT_LIST_DIR}/word_data/ar.txt")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/word_data/en.txt")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/word_data/ru.txt")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/word_data/tr.txt")
elseif(EMSCRIPTEN)
    # on the web, we have to put the files inside of the webassembly
    # somewhat unintuitively, this is done via a linker argument.
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/Inter-VariableFont.ttf@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/ReadexPro-Regular.ttf@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/Font-Awesome-7-Free-Solid-900.otf@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/JetBrainsMono-Regular.ttf@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/JetBrainsMono-Bold.ttf@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/word_data/ar.txt@/word_data/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/word_data/en.txt@/word_data/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/word_data/ru.txt@/word_data/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/word_data/tr.txt@/word_data/\""
    )
else()
    if (ANDROID)
        if (NOT MOBILE_ASSETS_DIR)
            message(FATAL_ERROR "Building for Android, but MOBILE_ASSETS_DIR is not set")
        endif()
        file(MAKE_DIRECTORY "${MOBILE_ASSETS_DIR}")    # we need to create the project Assets dir otherwise CMake won't copy our assets.
    endif()


    macro(copy_helper filename)
        if (ANDROID)
            # MOBILE_ASSETS_DIR is set in the gradle file via the cmake command line and points to the Android Studio Assets folder.
            # when we copy assets there, the Android build pipeline knows to add them to the apk.
            set(outname "${MOBILE_ASSETS_DIR}/${filename}")
        else()
            # for platforms that do not have a good packaging format, all we can do is copy the assets to the process working directory.
            set(outname "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${filename}")
        endif()
        get_filename_component(outdir "${outname}" DIRECTORY)
        get_filename_component(filename_dir "${filename}" DIRECTORY)
        if (filename_dir STREQUAL "word_data")
            set(input_name "${CMAKE_CURRENT_LIST_DIR}/${filename}")
        else()
            set(input_name "${CMAKE_CURRENT_LIST_DIR}/src/assets/fonts/${filename}")
        endif()
        add_custom_command(POST_BUILD
            TARGET "${EXECUTABLE_NAME}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${outdir}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${input_name}" "${outname}"
     )
    endmacro()
    copy_helper("Inter-VariableFont.ttf")
    copy_helper("ReadexPro-Regular.ttf")
    copy_helper("Font-Awesome-7-Free-Solid-900.otf")
    copy_helper("JetBrainsMono-Regular.ttf")
    copy_helper("JetBrainsMono-Bold.ttf")
    copy_helper("word_data/ar.txt")
    copy_helper("word_data/en.txt")
    copy_helper("word_data/ru.txt")
    copy_helper("word_data/tr.txt")
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

if(BUILD_TESTING AND NOT ANDROID)
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
