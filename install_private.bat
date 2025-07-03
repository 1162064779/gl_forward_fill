@echo off
echo ========================================
echo OpenGL Stereo Image Generator - Private Dependency Installation
echo ========================================

REM Check if vcpkg.json exists
if not exist "vcpkg.json" (
    echo Error: vcpkg.json not found!
    echo Please ensure vcpkg.json exists in the project root.
    pause
    exit /b 1
)

echo.
echo Installing project-private dependencies...

REM Install dependencies using vcpkg.json
vcpkg install --triplet=x64-windows

REM Check if toolchain file exists
if not exist "vcpkg_installed\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    echo Warning: vcpkg toolchain file not found in vcpkg_installed
    echo This might be a vcpkg version issue.
    echo.
)

REM Check if installation was successful
if %ERRORLEVEL% neq 0 (
    echo Dependency installation failed!
    echo Please check your vcpkg installation and internet connection.
    pause
    exit /b 1
)

echo.
echo ========================================
echo All dependencies installed successfully!
echo ========================================
echo.
echo Dependencies installed in vcpkg_installed/:
echo - GLFW3: OpenGL window management
echo - GLAD: OpenGL extension loading  
echo - stb: PNG image reading and writing
echo.
echo Now you can run build.bat to build the project
echo.
pause 