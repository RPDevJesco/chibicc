cmake_minimum_required(VERSION 3.10)
project(chibicc C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wno-switch -fno-common)
endif()

# Source files for chibicc
set(CHIBICC_SOURCES
    main.c
    tokenize.c
    preprocess.c
    parse.c
    type.c
    codegen.c
    unicode.c
    hashmap.c
    strings.c
    eventchain_compiler.c
    src/eventchain/eventchains.c
    events/tokenize_event.c
    events/preprocess_event.c
    events/parse_event.c
    events/codegen_event.c
)

# Create chibicc executable
add_executable(chibicc ${CHIBICC_SOURCES})

# Link math library on Unix-like systems
if(UNIX AND NOT APPLE)
    target_link_libraries(chibicc m)
endif()

# Installation
install(TARGETS chibicc DESTINATION bin)

# Testing (optional - requires test infrastructure)
enable_testing()
add_test(NAME chibicc_tests COMMAND ${CMAKE_COMMAND} -E echo "Run 'make test' for full test suite")

# Build instructions:
# Windows (Visual Studio):
#   mkdir build && cd build
#   cmake ..
#   cmake --build .
#
# Windows (MinGW):
#   mkdir build && cd build
#   cmake -G "MinGW Makefiles" ..
#   cmake --build .
#
# Linux/macOS:
#   mkdir build && cd build
#   cmake ..
#   make
