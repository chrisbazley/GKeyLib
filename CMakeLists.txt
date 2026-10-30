cmake_minimum_required(VERSION 3.14)
project(GKeyLib C)
include(FetchContent)

option(USE_OPTIONAL "Enable the _Optional qualifier" OFF)
option(USE_FORTIFY "Enable Fortify memory-allocation checking" OFF)
option(ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

FetchContent_Declare(CBUtil GIT_REPOSITORY https://github.com/chrisbazley/CBUtilLib.git GIT_TAG main)

include(CTest)

if(CMAKE_C_COMPILER_ID STREQUAL "SDCC")
    # The dependency's own CI covers its tests; this build tests GKeyLib.
    set(GKEY_BUILD_TESTING "${BUILD_TESTING}")
    set(BUILD_TESTING OFF)
    FetchContent_MakeAvailable(CBUtil)
    set(BUILD_TESTING "${GKEY_BUILD_TESTING}")
else()
    FetchContent_MakeAvailable(CBUtil)
endif()

if(USE_OPTIONAL)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang|SDCC")
        message(FATAL_ERROR
            "USE_OPTIONAL requires SDCC or Christopher Bazley's Clang fork")
    endif()

    add_compile_definitions(USE_OPTIONAL)

    if(CMAKE_GENERATOR STREQUAL "Xcode")
        set(CMAKE_XCODE_ATTRIBUTE_RUN_CLANG_STATIC_ANALYZER "YES")
        set(CMAKE_XCODE_ATTRIBUTE_CLANG_ANALYZER_OTHER_FLAGS
            "-analyzer-checker optionality,core -analyzer-max-loop 32")
    endif()
endif()

if(USE_FORTIFY)
    FetchContent_Declare(Fortify
        GIT_REPOSITORY https://github.com/chrisbazley/Fortify.git
        GIT_TAG main
    )

    FetchContent_MakeAvailable(Fortify)
    link_libraries(Fortify)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "SDCC")
    add_compile_options(--std-c99 --max-allocs-per-node 100
        "SHELL:--disable-warning 110" "SHELL:--disable-warning 126")
    if(BUILD_TESTING)
        # Use the dependency's compact assertions and simulator runtime.
        add_compile_options("-Wp-include,${cbutil_SOURCE_DIR}/tests/sdcc/compat.h")
        target_compile_options(CBUtil PRIVATE
            "-Wp-include,${cbutil_SOURCE_DIR}/tests/sdcc/compat.h")
    endif()
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_XCODE_ATTRIBUTE_RUN_CLANG_STATIC_ANALYZER "YES")

if(MSVC)
    # MSVC equivalent of -Wall -Wextra is /W4
    add_compile_options(/W4 /wd4200 /wd4996 /wd4130)
elseif(NOT CMAKE_C_COMPILER_ID STREQUAL "SDCC")
    # For GCC and Clang
    add_compile_options(-Wall -Wextra -Wsign-compare -pedantic)
endif()

if(WARNINGS_AS_ERRORS AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    if(MSVC)
        add_compile_options(/WX)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "SDCC")
        add_compile_options(--Werror)
        if(USE_OPTIONAL)
            find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
            set(CMAKE_C_COMPILER_LAUNCHER
                "${Python3_EXECUTABLE}"
                "${cbutil_SOURCE_DIR}/cmake/sdcc-diagnostics.py")
        endif()
    else()
        add_compile_options(-Werror)
        if(USE_OPTIONAL)
            add_compile_options(-Werror=null-to-nonoptional)
        endif()
    endif()
endif()

if(ENABLE_CLANG_TIDY AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
        message(FATAL_ERROR "ENABLE_CLANG_TIDY requires Clang")
    endif()

    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy
        HINTS "/opt/homebrew/opt/llvm/bin"
    )

    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR
            "ENABLE_CLANG_TIDY is ON, but clang-tidy was not found")
    endif()

    set(CMAKE_C_CLANG_TIDY
        "${CLANG_TIDY_EXE}"
        "--checks=-*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.*"
        "--warnings-as-errors=clang-analyzer-*"
    )

    message(STATUS "clang-tidy: ${CLANG_TIDY_EXE}")
    message(STATUS "_Optional static analysis enabled")
endif()

file(GLOB SOURCES "*.c")
file(GLOB PUBLIC_HEADERS "*.h")
file(GLOB PRIVATE_HEADERS "Internal/*.h")

add_library(GKey STATIC ${SOURCES} ${PUBLIC_HEADERS} ${PRIVATE_HEADERS})

set_target_properties(GKey PROPERTIES
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
    DEBUG_POSTFIX "dbg"
)

target_link_libraries(GKey PRIVATE CBUtil)

target_include_directories(GKey PUBLIC .)

target_compile_definitions(GKey PRIVATE
    $<$<CONFIG:Debug>:DEBUG_OUTPUT>
)

install(TARGETS GKey
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

# Dependency builds use the library without building its test suite.
if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR
   AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  add_subdirectory(tests)
endif()
