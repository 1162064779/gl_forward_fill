@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Android OpenGL ES Stereo Generator Build
echo ========================================

:: 设置NDK路径 - 请根据你的实际路径修改
set NDK_PATH=D:\ndk\android-ndk-r25c
set ANDROID_TOOLCHAIN=%NDK_PATH%\build\cmake\android.toolchain.cmake

:: 检查NDK路径
if not exist "%NDK_PATH%" (
    echo Error: NDK not found at %NDK_PATH%
    echo Please update the NDK_PATH variable in this script
    pause
    exit /b 1
)

:: 创建构建目录
if not exist "build" mkdir build
cd build

:: 清理之前的构建
if exist "CMakeCache.txt" del /q CMakeCache.txt
if exist "CMakeFiles" rmdir /s /q CMakeFiles

:: 配置CMake
echo Configuring CMake...
cmake -G "MinGW Makefiles" ^
    -DCMAKE_TOOLCHAIN_FILE="%ANDROID_TOOLCHAIN%" ^
    -DANDROID_ABI=arm64-v8a ^
    -DANDROID_PLATFORM=android-29 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DANDROID_STL=c++_static ^
    ..

if %ERRORLEVEL% neq 0 (
    echo Error: CMake configuration failed
    pause
    exit /b 1
)

:: 编译项目
echo Building project...
cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo Error: Build failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Executable location: build\OpenGLStereoGenerator
echo.
echo To deploy to Android device:
echo 1. adb push build\OpenGLStereoGenerator /data/local/tmp/
echo 2. adb shell chmod +x /data/local/tmp/OpenGLStereoGenerator
echo 3. adb shell /data/local/tmp/OpenGLStereoGenerator
echo.

pause 