cmake_minimum_required(VERSION 3.16)

project(nmea-gps-logger
    VERSION 1.0.0
    DESCRIPTION "NMEA 0183 parser, track logger and geofence monitor"
    LANGUAGES CXX)

option(NMEA_BUILD_TESTS "Build unit tests" ON)
option(NMEA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(NMEA_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(NMEA_COVERAGE "Instrument for gcov/gcovr coverage" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(nmea_warnings INTERFACE)
target_compile_options(nmea_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wnull-dereference)
if(NMEA_WARNINGS_AS_ERRORS)
    target_compile_options(nmea_warnings INTERFACE -Werror)
endif()
if(NMEA_SANITIZE)
    target_compile_options(nmea_warnings INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer
                                                   -fno-sanitize-recover=all)
    target_link_options(nmea_warnings INTERFACE -fsanitize=address,undefined)
endif()
if(NMEA_COVERAGE)
    target_compile_options(nmea_warnings INTERFACE --coverage -O0 -g)
    target_link_options(nmea_warnings INTERFACE --coverage)
endif()

add_library(nmea
    src/export.cpp
    src/geo.cpp
    src/geofence.cpp
    src/io.cpp
    src/parser.cpp
    src/pipeline.cpp
    src/replay.cpp
    src/sentence.cpp
    src/stats.cpp
    src/time.cpp
    src/track.cpp)
target_include_directories(nmea PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(nmea PRIVATE nmea_warnings)

if(NMEA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
