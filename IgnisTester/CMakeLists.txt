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

# Being strict
target_compile_options(Ignis PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
)

target_include_directories(IgnisTester PRIVATE
    ${CMAKE_SOURCE_DIR}/Dependencies
)

target_link_libraries(IgnisTester PRIVATE
    Ignis
)

