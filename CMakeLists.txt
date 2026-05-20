cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(DanteVersion)

project(DanteCLI
    VERSION ${DANTE_VERSION}
    DESCRIPTION "Dante CLI — Terminal nativo Windows com superpoderes"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(DanteCompileOptions)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Sql
    Multimedia
    Concurrent
    LinguistTools
)

qt_standard_project_setup()

option(DANTE_BUILD_TESTS "Build unit tests" OFF)
option(DANTE_USE_VENDORED_SQLITE "Use vendored sqlite3 instead of Qt Sql" OFF)

add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/persistence)
add_subdirectory(src/terminal)
add_subdirectory(src/ui)
add_subdirectory(src/app)

if(DANTE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "Dante CLI ${DANTE_VERSION} (${CMAKE_BUILD_TYPE})")
message(STATUS "  Qt:        ${Qt6_VERSION}")
message(STATUS "  Compiler:  ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Platform:  ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
