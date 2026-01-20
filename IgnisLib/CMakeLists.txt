cmake_minimum_required(VERSION 3.20)
project(Ignis CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Make clangd happy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")
add_library(Ignis STATIC ${SOURCES})

set(outputDir ${CMAKE_SOURCE_DIR}/Output)
set_target_properties(Ignis PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${outputDir}
    LIBRARY_OUTPUT_DIRECTORY ${outputDir}
    RUNTIME_OUTPUT_DIRECTORY ${outputDir}
)

# HarfBuzz
# This is for making smaller font files, currently useless for use so 
# its turned off
set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
add_subdirectory(Dependencies/harfbuzz)

add_subdirectory(Dependencies/miniaudio)

if (WIN32)
    target_compile_options(Ignis PRIVATE /permissive-)
    target_include_directories(Ignis PRIVATE
        $ENV{VULKAN_SDK}/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies
    )
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
    find_package(Vulkan REQUIRED)
    target_include_directories(Ignis PRIVATE
        ${VULKAN_INCLUDE_DIRS}
        ${GLFW_INCLUDE_DIRS}
        ${HARFBUZZ_INCLUDE_DIRS}
    )
    target_link_libraries(Ignis PRIVATE Vulkan::Vulkan ${GLFW_LIBRARIES})
endif()

target_link_libraries(Ignis PRIVATE harfbuzz miniaudio)

target_include_directories(Ignis PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/glm
)

