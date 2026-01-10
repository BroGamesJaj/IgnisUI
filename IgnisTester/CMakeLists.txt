cmake_minimum_required(VERSION 3.20)
project(IgnisTester CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Make clangd happy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/Source/*.cpp")
add_subdirectory(../IgnisLib IgnisLibBuild)
add_executable(IgnisTester ${SOURCES} )

set(outputDir ${CMAKE_SOURCE_DIR}/Output)
set_target_properties(IgnisTester PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${outputDir}
    LIBRARY_OUTPUT_DIRECTORY ${outputDir}
    RUNTIME_OUTPUT_DIRECTORY ${outputDir}
)

if (WIN32)
    set(GLFW_LIB ${CMAKE_SOURCE_DIR}/Dependencies/glfw3.lib)
    set(VULKAN_LIB $ENV{VULKAN_SDK}/Lib/vulkan-1.lib)
    target_include_directories(IgnisTester PRIVATE
        $ENV{VULKAN_SDK}/Include
    )
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
    find_package(Vulkan REQUIRED)
    set(VULKAN_LIB Vulkan::Vulkan)
endif()

target_include_directories(IgnisTester PRIVATE
    ${CMAKE_SOURCE_DIR}/Dependencies
)

target_link_libraries(IgnisTester PRIVATE
    ${VULKAN_LIB}
    ${GLFW_LIB}
    Ignis
)

