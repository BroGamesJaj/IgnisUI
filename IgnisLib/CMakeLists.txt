cmake_minimum_required(VERSION 3.20)
project(Ignis CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")
add_library(Ignis STATIC ${SOURCES})

set(outputDir ${CMAKE_SOURCE_DIR}/Output)
set_target_properties(Ignis PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${outputDir}
    LIBRARY_OUTPUT_DIRECTORY ${outputDir}
    RUNTIME_OUTPUT_DIRECTORY ${outputDir}
)

if (WIN32)
    target_include_directories(Ignis PRIVATE
        $ENV{VULKAN_SDK}/Include
    )
endif()

target_include_directories(Ignis PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies
)
