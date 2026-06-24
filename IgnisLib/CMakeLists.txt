cmake_minimum_required(VERSION 3.20)
project(Ignis CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Make clangd happy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Render/*.cpp"
)

add_library(Ignis STATIC ${SOURCES})

set(outputDir ${CMAKE_SOURCE_DIR}/Output)
set_target_properties(Ignis PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${outputDir}
    LIBRARY_OUTPUT_DIRECTORY ${outputDir}
    RUNTIME_OUTPUT_DIRECTORY ${outputDir}
)

# Being strict
target_compile_options(Ignis PRIVATE
    # try these out see if you like them:
    # /W4 /w14242 /w14254 /w14263 /w14265 /w14287 /w14296 /w14311 /w14545 /w14546 /w14547 /w14549 /wd4100 /wd5030
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-reorder>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-reorder>
)

# Freetype
add_subdirectory(Dependencies/freetype)

# HarfBuzz
# This is for making smaller font files, currently useless for use so its turned off
set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)
set(FREETYPE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
set(HB_HAVE_GLIB OFF CACHE BOOL "" FORCE)
add_subdirectory(Dependencies/harfbuzz)

# GLFW options
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# GLFW
add_subdirectory(Dependencies/GLFW)
target_link_libraries(Ignis PRIVATE glfw)
target_include_directories(Ignis PRIVATE Dependencies/GLFW/include)
target_compile_definitions(Ignis PRIVATE GLFW_STATIC GLFW_VULKAN_STATIC)

# SPIRV-Reflect
add_subdirectory(Dependencies/SPIRV-Reflect-lean)

# Vulkan
if(WIN32)
    target_include_directories(Ignis PRIVATE $ENV{VULKAN_SDK}/Include)
    target_link_libraries(Ignis PRIVATE $ENV{VULKAN_SDK}/Lib/vulkan-1.lib)
else()
    find_package(Vulkan REQUIRED)
    target_include_directories(Ignis PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(Ignis PRIVATE ${Vulkan_LIBRARIES})
endif()

target_link_libraries(Ignis PRIVATE 
    harfbuzz 
    freetype
    spirv-reflect-static
)

target_include_directories(Ignis PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/glm
)

