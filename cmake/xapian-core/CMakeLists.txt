cmake_minimum_required(VERSION 3.16)

include(CheckIncludeFileCXX)
include(CheckSymbolExists)
include(CheckCXXSourceCompiles)
include(TestBigEndian)

get_filename_component(XAPIAN_CORE_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../xapian/xapian-core"
    ABSOLUTE
)
set(XAPIAN_CORE_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

check_include_file_cxx("poll.h" XAPIAN_HAVE_POLL_H)
check_include_file_cxx("sys/resource.h" XAPIAN_HAVE_SYS_RESOURCE_H)
check_include_file_cxx("sys/uio.h" XAPIAN_HAVE_SYS_UIO_H)
check_include_file_cxx("uuid/uuid.h" XAPIAN_HAVE_UUID_UUID_H)

check_symbol_exists(clock_gettime "time.h" XAPIAN_HAVE_CLOCK_GETTIME)
check_symbol_exists(closefrom "unistd.h" XAPIAN_HAVE_CLOSEFROM)
check_symbol_exists(close_range "unistd.h" XAPIAN_HAVE_CLOSE_RANGE)
check_symbol_exists(fdatasync "unistd.h" XAPIAN_HAVE_FDATASYNC)
check_symbol_exists(fsync "unistd.h" XAPIAN_HAVE_FSYNC)
check_symbol_exists(ftime "sys/timeb.h" XAPIAN_HAVE_FTIME)
check_symbol_exists(ftruncate "unistd.h" XAPIAN_HAVE_FTRUNCATE)
check_symbol_exists(getdirentries "dirent.h;unistd.h" XAPIAN_HAVE_GETDIRENTRIES)
check_symbol_exists(gethostname "unistd.h" XAPIAN_HAVE_GETHOSTNAME)
check_symbol_exists(getrlimit "sys/resource.h" XAPIAN_HAVE_GETRLIMIT)
check_symbol_exists(gettimeofday "sys/time.h" XAPIAN_HAVE_GETTIMEOFDAY)
check_symbol_exists(link "unistd.h" XAPIAN_HAVE_LINK)
check_symbol_exists(nanosleep "time.h" XAPIAN_HAVE_NANOSLEEP)
check_symbol_exists(poll "poll.h" XAPIAN_HAVE_POLL)
check_symbol_exists(posix_fadvise "fcntl.h" XAPIAN_HAVE_POSIX_FADVISE)
check_symbol_exists(pread "unistd.h" XAPIAN_HAVE_PREAD)
check_symbol_exists(pwrite "unistd.h" XAPIAN_HAVE_PWRITE)
check_symbol_exists(setenv "stdlib.h" XAPIAN_HAVE_SETENV)
check_symbol_exists(sigaction "signal.h" XAPIAN_HAVE_SIGACTION)
check_symbol_exists(sleep "unistd.h" XAPIAN_HAVE_SLEEP)
check_symbol_exists(socketpair "sys/socket.h" XAPIAN_HAVE_SOCKETPAIR)
check_symbol_exists(timer_create "time.h" XAPIAN_HAVE_TIMER_CREATE)
check_symbol_exists(writev "sys/uio.h" XAPIAN_HAVE_WRITEV)

check_symbol_exists(strerror_r "string.h" XAPIAN_HAVE_DECL_STRERROR_R)
check_symbol_exists(strerrordesc_np "string.h" XAPIAN_HAVE_STRERRORDESC_NP)

set(_xapian_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=gnu++23")
check_cxx_source_compiles([=[
#include <charconv>
#include <system_error>
int main() {
    double value = 0.0;
    auto result = std::from_chars("1.25", "1.25" + 4, value);
    return result.ec == std::errc{} ? 0 : 1;
}
]=] XAPIAN_HAVE_STD_FROM_CHARS_DOUBLE)

check_cxx_source_compiles([=[
#include <string.h>
int main() {
    char buf[32];
    char* p = strerror_r(1, buf, sizeof(buf));
    return p ? 0 : 1;
}
]=] XAPIAN_STRERROR_R_CHAR_P)
set(CMAKE_REQUIRED_FLAGS "${_xapian_saved_required_flags}")
unset(_xapian_saved_required_flags)

test_big_endian(XAPIAN_WORDS_BIGENDIAN)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set(XAPIAN_HAVE_DECL___BUILTIN_ADD_OVERFLOW 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_BSWAP16 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_BSWAP32 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_BSWAP64 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_CLZ 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_CLZL 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_CLZLL 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_CTZ 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_CTZL 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_CTZLL 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_EXPECT 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_FFS 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_MUL_OVERFLOW 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_POPCOUNT 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_POPCOUNTL 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_POPCOUNTLL 1)
    set(XAPIAN_HAVE_DECL___BUILTIN_SUB_OVERFLOW 1)
else()
    set(XAPIAN_HAVE_DECL___BUILTIN_ADD_OVERFLOW 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_BSWAP16 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_BSWAP32 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_BSWAP64 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_CLZ 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_CLZL 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_CLZLL 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_CTZ 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_CTZL 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_CTZLL 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_EXPECT 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_FFS 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_MUL_OVERFLOW 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_POPCOUNT 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_POPCOUNTL 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_POPCOUNTLL 0)
    set(XAPIAN_HAVE_DECL___BUILTIN_SUB_OVERFLOW 0)
endif()

if(ANDROID)
    set(XAPIAN_HAVE_CLOCK_GETTIME 1)
    set(XAPIAN_HAVE_FDATASYNC 1)
    set(XAPIAN_HAVE_FSYNC 1)
    set(XAPIAN_HAVE_FTRUNCATE 1)
    set(XAPIAN_HAVE_GETHOSTNAME 1)
    set(XAPIAN_HAVE_GETRLIMIT 1)
    set(XAPIAN_HAVE_GETTIMEOFDAY 1)
    set(XAPIAN_HAVE_LINK 1)
    set(XAPIAN_HAVE_NANOSLEEP 1)
    set(XAPIAN_HAVE_POLL 1)
    set(XAPIAN_HAVE_PREAD 1)
    set(XAPIAN_HAVE_PWRITE 1)
    set(XAPIAN_HAVE_SETENV 1)
    set(XAPIAN_HAVE_SIGACTION 1)
    set(XAPIAN_HAVE_SLEEP 1)
    set(XAPIAN_HAVE_SOCKETPAIR 1)
    set(XAPIAN_HAVE_WRITEV 1)
endif()

if(EMSCRIPTEN)
    # Emscripten may expose socketpair() at compile time but not support the
    # syscall at runtime in browser builds. Prefer the flock-based lock path
    # documented by Xapian's own emscripten notes.
    set(XAPIAN_HAVE_SOCKETPAIR 0)
    set(XAPIAN_HAVE_UUID_UUID_H 1)
endif()

set(XAPIAN_PACKAGE "xapian-core")
set(XAPIAN_PACKAGE_NAME "xapian-core")
set(XAPIAN_USE_ICU 0)
set(XAPIAN_VERSION "2.0.0")
set(XAPIAN_MAJOR_VERSION 2)
set(XAPIAN_MINOR_VERSION 0)
set(XAPIAN_REVISION 0)
set(XAPIAN_DOCID_BASE_TYPE "int")
set(XAPIAN_TERMCOUNT_BASE_TYPE "int")
set(XAPIAN_TERMPOS_BASE_TYPE "int")
set(XAPIAN_TOTALLENGTH_TYPE "unsigned long long")
set(XAPIAN_REVISION_TYPE "unsigned long long")

if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(XAPIAN_USE_PROC_FOR_UUID 1)
else()
    set(XAPIAN_USE_PROC_FOR_UUID 0)
endif()

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/config.h.in"
    "${XAPIAN_CORE_BINARY_DIR}/config.h"
    @ONLY
)
file(MAKE_DIRECTORY "${XAPIAN_CORE_BINARY_DIR}/include/xapian")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/version.h.in"
    "${XAPIAN_CORE_BINARY_DIR}/include/xapian/version.h"
    @ONLY
)

file(GLOB XAPIAN_API_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/api/*.cc")
list(REMOVE_ITEM XAPIAN_API_SOURCES
    "${XAPIAN_CORE_SOURCE_DIR}/api/replication.cc"
)

file(GLOB XAPIAN_BACKEND_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/backends/*.cc")
list(REMOVE_ITEM XAPIAN_BACKEND_SOURCES
    "${XAPIAN_CORE_SOURCE_DIR}/backends/dbfactory_remote.cc"
)

file(GLOB XAPIAN_GLASS_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/backends/glass/*.cc")
list(REMOVE_ITEM XAPIAN_GLASS_SOURCES
    "${XAPIAN_CORE_SOURCE_DIR}/backends/glass/glass_databasereplicator.cc"
)

file(GLOB XAPIAN_INMEMORY_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/backends/inmemory/*.cc")
file(GLOB XAPIAN_MULTI_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/backends/multi/*.cc")
file(GLOB XAPIAN_COMMON_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/common/*.cc")
list(REMOVE_ITEM XAPIAN_COMMON_SOURCES
    "${XAPIAN_CORE_SOURCE_DIR}/common/getopt.cc"
    "${XAPIAN_CORE_SOURCE_DIR}/common/socket_utils.cc"
)
file(GLOB XAPIAN_EXPAND_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/expand/*.cc")
file(GLOB XAPIAN_GEOSPATIAL_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/geospatial/*.cc")
file(GLOB XAPIAN_LANGUAGE_SOURCES CONFIGURE_DEPENDS
    "${XAPIAN_CORE_SOURCE_DIR}/languages/*.cc"
)
file(GLOB XAPIAN_MATCHER_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/matcher/*.cc")
set(XAPIAN_NET_SOURCES "${XAPIAN_CORE_SOURCE_DIR}/net/serialise.cc")
file(GLOB XAPIAN_QUERYPARSER_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/queryparser/*.cc")
file(GLOB XAPIAN_UNICODE_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/unicode/*.cc")
file(GLOB XAPIAN_WEIGHT_SOURCES CONFIGURE_DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/weight/*.cc")

list(APPEND XAPIAN_UNICODE_SOURCES
    "${XAPIAN_CORE_BINARY_DIR}/unicode/unicode-data.cc"
)
list(APPEND XAPIAN_QUERYPARSER_SOURCES
    "${XAPIAN_CORE_BINARY_DIR}/queryparser/queryparser_internal.cc"
)

file(MAKE_DIRECTORY "${XAPIAN_CORE_BINARY_DIR}/weight")

file(MAKE_DIRECTORY "${XAPIAN_CORE_BINARY_DIR}/include/xapian")
add_custom_command(OUTPUT
    "${XAPIAN_CORE_BINARY_DIR}/include/xapian/error.h"
    "${XAPIAN_CORE_BINARY_DIR}/include/xapian/errordispatch.h"
    COMMAND perl "-I${XAPIAN_CORE_SOURCE_DIR}" "${XAPIAN_CORE_SOURCE_DIR}/generate-exceptions"
    WORKING_DIRECTORY "${XAPIAN_CORE_BINARY_DIR}"
    DEPENDS
        "${XAPIAN_CORE_SOURCE_DIR}/exception_data.pm"
        "${XAPIAN_CORE_SOURCE_DIR}/generate-exceptions"
    COMMENT "Generating Xapian exception headers"
)

file(GLOB SNOWBALL_ALGORITHM_DEPS CONFIGURE_DEPENDS
    "${XAPIAN_CORE_SOURCE_DIR}/languages/algorithms/*.sbl"
)
set(SNOWBALL_ALGORITHMS)
set(SNOWBALL_GENERATED_SOURCES)
set(SNOWBALL_GENERATED_HEADERS)
foreach(alg ${SNOWBALL_ALGORITHM_DEPS})
    file(RELATIVE_PATH rel_alg "${XAPIAN_CORE_SOURCE_DIR}" "${alg}")
    get_filename_component(alg_name "${alg}" NAME_WE)
    list(APPEND SNOWBALL_ALGORITHMS "${rel_alg}")
    list(APPEND SNOWBALL_GENERATED_SOURCES
        "${XAPIAN_CORE_BINARY_DIR}/languages/algorithms/${alg_name}.cc"
    )
    list(APPEND SNOWBALL_GENERATED_HEADERS
        "${XAPIAN_CORE_BINARY_DIR}/languages/algorithms/${alg_name}.h"
    )
endforeach()

set(SNOWBALL_COMPILER_SOURCES
    "${XAPIAN_CORE_SOURCE_DIR}/languages/compiler/space.c"
    "${XAPIAN_CORE_SOURCE_DIR}/languages/compiler/tokeniser.c"
    "${XAPIAN_CORE_SOURCE_DIR}/languages/compiler/analyser.c"
    "${XAPIAN_CORE_SOURCE_DIR}/languages/compiler/generator.c"
    "${XAPIAN_CORE_SOURCE_DIR}/languages/compiler/driver.c"
)
find_program(XAPIAN_CC_FOR_BUILD NAMES cc gcc clang)
if(NOT XAPIAN_CC_FOR_BUILD)
    message(FATAL_ERROR "A host C compiler is required to build Xapian's Snowball compiler")
endif()
add_custom_command(OUTPUT "${XAPIAN_CORE_BINARY_DIR}/languages/snowball"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${XAPIAN_CORE_BINARY_DIR}/languages"
    COMMAND "${XAPIAN_CC_FOR_BUILD}" -g -DTARGET_C_ONLY
        ${SNOWBALL_COMPILER_SOURCES}
        -o "${XAPIAN_CORE_BINARY_DIR}/languages/snowball"
    DEPENDS ${SNOWBALL_COMPILER_SOURCES}
    COMMENT "Building Xapian Snowball compiler"
)

file(MAKE_DIRECTORY
    "${XAPIAN_CORE_BINARY_DIR}/languages"
    "${XAPIAN_CORE_BINARY_DIR}/languages/algorithms"
)
foreach(alg ${SNOWBALL_ALGORITHM_DEPS})
    get_filename_component(alg_name "${alg}" NAME_WE)
    add_custom_command(OUTPUT
        "${XAPIAN_CORE_BINARY_DIR}/languages/algorithms/${alg_name}.cc"
        "${XAPIAN_CORE_BINARY_DIR}/languages/algorithms/${alg_name}.h"
        COMMAND "${XAPIAN_CORE_BINARY_DIR}/languages/snowball"
            "${alg}"
            -o "${XAPIAN_CORE_BINARY_DIR}/languages/algorithms/${alg_name}.cc"
            -c++
            -u
            -p "Xapian::StemImplementation"
            -P "Xapian::Internal::Snowball"
            -r "languages/"
            -cheader "config.h"
            -hheader "xapian/stem.h"
        COMMAND "${CMAKE_COMMAND}" -E touch
            "${XAPIAN_CORE_BINARY_DIR}/languages/algorithms/${alg_name}.h"
        DEPENDS "${XAPIAN_CORE_BINARY_DIR}/languages/snowball" "${alg}"
        COMMENT "Generating Xapian ${alg_name} stemmer"
    )
endforeach()

add_custom_command(OUTPUT
    "${XAPIAN_CORE_BINARY_DIR}/languages/sbl-dispatch.h"
    "${XAPIAN_CORE_BINARY_DIR}/languages/allsnowballheaders.h"
    COMMAND perl "-I${XAPIAN_CORE_SOURCE_DIR}/common"
        "${XAPIAN_CORE_SOURCE_DIR}/languages/collate-sbl"
        "${XAPIAN_CORE_SOURCE_DIR}"
        ${SNOWBALL_ALGORITHMS}
    WORKING_DIRECTORY "${XAPIAN_CORE_BINARY_DIR}"
    DEPENDS
        "${XAPIAN_CORE_SOURCE_DIR}/languages/collate-sbl"
        "${XAPIAN_CORE_SOURCE_DIR}/common/Tokeniseise.pm"
        ${SNOWBALL_ALGORITHM_DEPS}
        ${SNOWBALL_GENERATED_HEADERS}
    COMMENT "Generating Snowball language dispatch headers"
)

file(MAKE_DIRECTORY "${XAPIAN_CORE_BINARY_DIR}/unicode")
add_custom_command(OUTPUT "${XAPIAN_CORE_BINARY_DIR}/unicode/unicode-data.cc"
    COMMAND perl
        "${XAPIAN_CORE_SOURCE_DIR}/unicode/gen-unicode-data"
        "${XAPIAN_CORE_SOURCE_DIR}/unicode/UnicodeData.txt"
        "17.0.0"
        "${XAPIAN_CORE_BINARY_DIR}/unicode/unicode-data.cc"
    DEPENDS
        "${XAPIAN_CORE_SOURCE_DIR}/unicode/gen-unicode-data"
        "${XAPIAN_CORE_SOURCE_DIR}/unicode/UnicodeData.txt"
    COMMENT "Generating Xapian Unicode data"
)
add_custom_command(OUTPUT "${XAPIAN_CORE_BINARY_DIR}/unicode/c_istab.h"
    COMMAND perl
        "${XAPIAN_CORE_SOURCE_DIR}/unicode/gen_c_istab"
        "${XAPIAN_CORE_BINARY_DIR}/unicode/c_istab.h"
    DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/unicode/gen_c_istab"
    COMMENT "Generating Xapian character table"
)

file(MAKE_DIRECTORY "${XAPIAN_CORE_BINARY_DIR}/queryparser")
add_custom_command(OUTPUT "${XAPIAN_CORE_BINARY_DIR}/queryparser/lemon"
    COMMAND "${XAPIAN_CC_FOR_BUILD}"
        "${XAPIAN_CORE_SOURCE_DIR}/queryparser/lemon.c"
        -o "${XAPIAN_CORE_BINARY_DIR}/queryparser/lemon"
    DEPENDS "${XAPIAN_CORE_SOURCE_DIR}/queryparser/lemon.c"
    COMMENT "Building Xapian Lemon parser generator"
)
add_custom_command(OUTPUT
    "${XAPIAN_CORE_BINARY_DIR}/queryparser/queryparser_internal.cc"
    "${XAPIAN_CORE_BINARY_DIR}/queryparser/queryparser_token.h"
    COMMAND "${XAPIAN_CORE_BINARY_DIR}/queryparser/lemon"
        -q
        "-o${XAPIAN_CORE_BINARY_DIR}/queryparser/queryparser_internal.cc"
        "-h${XAPIAN_CORE_BINARY_DIR}/queryparser/queryparser_token.h"
        "${XAPIAN_CORE_SOURCE_DIR}/queryparser/queryparser.lemony"
    COMMAND "${CMAKE_COMMAND}" -E touch
        "${XAPIAN_CORE_BINARY_DIR}/queryparser/queryparser_token.h"
    DEPENDS
        "${XAPIAN_CORE_BINARY_DIR}/queryparser/lemon"
        "${XAPIAN_CORE_SOURCE_DIR}/queryparser/queryparser.lemony"
        "${XAPIAN_CORE_SOURCE_DIR}/queryparser/queryparser.lt"
    COMMENT "Generating Xapian query parser"
)

add_custom_command(OUTPUT "${XAPIAN_CORE_BINARY_DIR}/weight/idf-norm-dispatch.h"
    COMMAND perl "-I${XAPIAN_CORE_SOURCE_DIR}/common"
        "${XAPIAN_CORE_SOURCE_DIR}/weight/collate-idf-norm"
        "${XAPIAN_CORE_SOURCE_DIR}"
    WORKING_DIRECTORY "${XAPIAN_CORE_BINARY_DIR}"
    DEPENDS
        "${XAPIAN_CORE_SOURCE_DIR}/weight/collate-idf-norm"
        "${XAPIAN_CORE_SOURCE_DIR}/weight/Makefile.mk"
        "${XAPIAN_CORE_SOURCE_DIR}/common/Tokeniseise.pm"
    COMMENT "Generating Xapian idf normalization dispatch header"
)

add_custom_command(OUTPUT "${XAPIAN_CORE_BINARY_DIR}/weight/wdf-norm-dispatch.h"
    COMMAND perl "-I${XAPIAN_CORE_SOURCE_DIR}/common"
        "${XAPIAN_CORE_SOURCE_DIR}/weight/collate-wdf-norm"
        "${XAPIAN_CORE_SOURCE_DIR}"
    WORKING_DIRECTORY "${XAPIAN_CORE_BINARY_DIR}"
    DEPENDS
        "${XAPIAN_CORE_SOURCE_DIR}/weight/collate-wdf-norm"
        "${XAPIAN_CORE_SOURCE_DIR}/weight/Makefile.mk"
        "${XAPIAN_CORE_SOURCE_DIR}/common/Tokeniseise.pm"
    COMMENT "Generating Xapian wdf normalization dispatch header"
)

add_library(xapian STATIC
    ${XAPIAN_API_SOURCES}
    ${XAPIAN_BACKEND_SOURCES}
    ${XAPIAN_GLASS_SOURCES}
    ${XAPIAN_INMEMORY_SOURCES}
    ${XAPIAN_MULTI_SOURCES}
    ${XAPIAN_COMMON_SOURCES}
    ${XAPIAN_EXPAND_SOURCES}
    ${XAPIAN_GEOSPATIAL_SOURCES}
    ${XAPIAN_LANGUAGE_SOURCES}
    ${SNOWBALL_GENERATED_SOURCES}
    ${XAPIAN_MATCHER_SOURCES}
    ${XAPIAN_NET_SOURCES}
    ${XAPIAN_QUERYPARSER_SOURCES}
    ${XAPIAN_UNICODE_SOURCES}
    ${XAPIAN_WEIGHT_SOURCES}
    "${XAPIAN_CORE_BINARY_DIR}/include/xapian/error.h"
    "${XAPIAN_CORE_BINARY_DIR}/include/xapian/errordispatch.h"
    "${XAPIAN_CORE_BINARY_DIR}/languages/sbl-dispatch.h"
    "${XAPIAN_CORE_BINARY_DIR}/languages/allsnowballheaders.h"
    "${XAPIAN_CORE_BINARY_DIR}/unicode/c_istab.h"
    "${XAPIAN_CORE_BINARY_DIR}/weight/idf-norm-dispatch.h"
    "${XAPIAN_CORE_BINARY_DIR}/weight/wdf-norm-dispatch.h"
)

add_library(xapian::xapian ALIAS xapian)

set_target_properties(xapian PROPERTIES
    OUTPUT_NAME xapian
    POSITION_INDEPENDENT_CODE ON
)

target_compile_features(xapian PUBLIC cxx_std_23)
target_compile_definitions(xapian PRIVATE XAPIAN_LIB_BUILD ZLIB_CONST)
if(EMSCRIPTEN)
    target_compile_definitions(xapian PRIVATE FLINTLOCK_USE_FLOCK=1)
endif()
target_include_directories(xapian
    PUBLIC
        "${XAPIAN_CORE_SOURCE_DIR}/include"
        "${XAPIAN_CORE_BINARY_DIR}/include"
    PRIVATE
        "${XAPIAN_CORE_SOURCE_DIR}"
        "${XAPIAN_CORE_BINARY_DIR}"
        "${XAPIAN_CORE_SOURCE_DIR}/common"
        "${XAPIAN_CORE_BINARY_DIR}/languages"
        "${XAPIAN_CORE_SOURCE_DIR}/languages"
        "${XAPIAN_CORE_BINARY_DIR}/unicode"
        "${XAPIAN_CORE_SOURCE_DIR}/queryparser"
        "${XAPIAN_CORE_BINARY_DIR}/queryparser"
)

if(EMSCRIPTEN)
    target_link_options(xapian PUBLIC "-sUSE_ZLIB=1")
else()
    find_package(ZLIB REQUIRED)
    target_link_libraries(xapian PUBLIC ZLIB::ZLIB)
endif()
