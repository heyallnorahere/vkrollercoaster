cmake_minimum_required(VERSION 3.10)

set(VKROLLERCOASTER_INCLUDES "${CMAKE_SOURCE_DIR}/vendor/glm" ${Vulkan_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
set(VKROLLERCOASTER_LIBS stb glfw spdlog spirv-cross-glsl EnTT shaderc ${Vulkan_LIBRARIES} assimp VulkanMemoryAllocator ktx_read)

if(UNIX AND NOT APPLE)
    list(APPEND VKROLLERCOASTER_LIBS pthread stdc++fs)
endif()

file(GLOB_RECURSE VKROLLERCOASTER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE VKROLLERCOASTER_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders/*.hlsl")

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/vendor/imgui")
list(APPEND VKROLLERCOASTER_INCLUDES ${IMGUI_DIR})
set(IMGUI_BACKENDS glfw vulkan)
file(GLOB IMGUI_SOURCE "${IMGUI_DIR}/*.cpp" "${IMGUI_DIR}/*.h" "${IMGUI_DIR}/misc/cpp/imgui_stdlib.*")
foreach(BACKEND ${IMGUI_BACKENDS})
    file(GLOB BACKEND_SOURCE "${IMGUI_DIR}/backends/imgui_impl_${BACKEND}.*")
    list(APPEND IMGUI_SOURCE ${BACKEND_SOURCE})
endforeach()

set(MANIFEST ${VKROLLERCOASTER_SOURCE} ${VKROLLERCOASTER_SHADERS} ${IMGUI_SOURCE})
add_executable(vkrollercoaster ${MANIFEST})
target_include_directories(vkrollercoaster PRIVATE ${VKROLLERCOASTER_INCLUDES})
target_link_libraries(vkrollercoaster PRIVATE ${VKROLLERCOASTER_LIBS})
if(DEFINED VKROLLERCOASTER_DEFINITIONS)
    target_compile_definitions(vkrollercoaster PRIVATE ${VKROLLERCOASTER_DEFINITIONS})    
endif()

set_target_properties(vkrollercoaster PROPERTIES CXX_STANDARD 17)

if(MSVC)
    set_target_properties(vkrollercoaster PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_source_files_properties(${VKROLLERCOASTER_SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")
endif()

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.16)
    target_precompile_headers(vkrollercoaster PRIVATE pch.h)
    set_source_files_properties(${IMGUI_SOURCE} PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)
endif()