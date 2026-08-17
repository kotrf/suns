cmake_minimum_required(VERSION 3.24)
project(Suns VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SUNS_BUILD_APP "Build the Qt desktop application" ON)
option(SUNS_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(src/core)

if(SUNS_BUILD_APP)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
    qt_standard_project_setup()
    add_subdirectory(src/app)
endif()

if(SUNS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
