@echo off

REM Set variables for directories and build configuration
set BUILD_DIR=build

REM Check if the build directory exists; if not, create it
if not exist %BUILD_DIR% (
    mkdir %BUILD_DIR%
)

REM Navigate to the build directory
cd %BUILD_DIR%

REM Run CMake to configure the project
echo Configuring the project...
cmake -G "Visual Studio 17 2022" -A x64 .. || (
    echo CMake configuration failed!
    cd ..
    PAUSE
    exit /b 1
)

REM Build the project
echo Building Debug
cmake --build . --config Debug || (
    echo Build failed!
    cd ..
    PAUSE
    exit /b 1
)

echo Building Release
cmake --build . --config Release || (
    echo Build failed!
    cd ..
    PAUSE
    exit /b 1
)