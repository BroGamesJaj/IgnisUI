cmake_minimum_required(VERSION 3.20)
project(Ignis C)

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_STANDARD_REQUIRED ON)

file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/Source/*.c")

if(MSVC)
    set(outputDir ${CMAKE_SOURCE_DIR}/Output)
else()
#if(CMAKE_BUILD_TYPE STREQUAL "Release")
#    set(outputDir ${CMAKE_SOURCE_DIR}/Output)
#elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
#    set(outputDir ${CMAKE_SOURCE_DIR}/Output)
#else()
#    set(outputDir ${CMAKE_SOURCE_DIR}/Output)
#endif()
endif()


set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${outputDir})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${outputDir})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${outputDir})

add_library(Ignis STATIC ${SOURCES})

set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})

target_include_directories(Ignis PRIVATE ${CMAKE_SOURCE_DIR}/Source)