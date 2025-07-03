@echo off
echo Building OpenGL Stereo Image Generator...

REM Clean and recreate build directory
if exist "build" (
    echo Cleaning build directory...
    rmdir /s /q build
)
mkdir build

REM Enter build directory
cd build

REM Configure CMake project
echo Configuring CMake project...

REM Check for vcpkg toolchain file
set VCPKG_TOOLCHAIN_FILE=
if exist "..\vcpkg_installed\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set VCPKG_TOOLCHAIN_FILE=-DCMAKE_TOOLCHAIN_FILE=../vcpkg_installed/vcpkg/scripts/buildsystems/vcpkg.cmake
    echo Using project-private vcpkg toolchain file
) else (
    REM Try to find global vcpkg toolchain
    where vcpkg >nul 2>&1
    if %ERRORLEVEL% equ 0 (
        for /f "tokens=*" %%i in ('where vcpkg') do (
            set VCPKG_PATH=%%i
            goto :found_vcpkg
        )
    )
    :found_vcpkg
    if defined VCPKG_PATH (
        for %%i in ("%VCPKG_PATH%") do set VCPKG_ROOT=%%~dpi
        set VCPKG_TOOLCHAIN_FILE=-DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
        echo Using global vcpkg toolchain file: %VCPKG_TOOLCHAIN_FILE%
    ) else (
        echo Error: Could not find vcpkg toolchain file
        echo Please ensure vcpkg is installed and in PATH
        pause
        exit /b 1
    )
)

cmake .. %VCPKG_TOOLCHAIN_FILE%

REM Build project
echo Building project...
cmake --build . --config Release

REM Check if build was successful
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build successful!
echo Executable location: build\Release\OpenGLStereoGenerator.exe

REM Copy input files to output directory (if they exist)
if exist "..\image.png" copy "..\image.png" "Release\"
if exist "..\depth.exr" copy "..\depth.exr" "Release\"

cd ..
pause 