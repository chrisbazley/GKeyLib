cmake_minimum_required(VERSION 3.10)
project(GKeyLib C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

if(MSVC)
    # MSVC equivalent of -Wall -Wextra is /W4
    add_compile_options(/W4)
else()
    # For GCC and Clang
    add_compile_options(-Wall -Wextra -pedantic)
endif()

file(GLOB SOURCES "*.c")
file(GLOB PUBLIC_HEADERS "*.h")

add_library(GKey STATIC ${SOURCES} ${PUBLIC_HEADERS})

set_target_properties(GKey PROPERTIES
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
    DEBUG_POSTFIX "dbg"
)

target_include_directories(GKey PUBLIC .)
target_compile_definitions(GKey PRIVATE $<$<CONFIG:Debug>:DEBUG_OUTPUT>)

install(TARGETS GKey
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)
