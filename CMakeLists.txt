cmake_minimum_required(VERSION 3.10)
project(vkrollercoaster)
if(NOT ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    message(FATAL_ERROR "vkrollercoaster must not be used as a dependency!")
endif()
find_package(Vulkan REQUIRED)
add_subdirectory("vendor")
add_subdirectory("src")