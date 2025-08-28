@echo off
setlocal

REM ===== Build config =====
set "CONFIG=%~1"
if /i "%CONFIG%"=="" set "CONFIG=Release"
if /i not "%CONFIG%"=="Release" if /i not "%CONFIG%"=="Debug" set "CONFIG=Release"

REM ===== vcpkg triplet (optional 2nd arg) =====
set "TRIPLET=%~2"
if /i "%TRIPLET%"=="" set "TRIPLET=x64-windows"

REM ===== Go to script dir =====
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

echo [INFO] Config : %CONFIG%
echo [INFO] Triplet: %TRIPLET%
echo [INFO] Project root: %CD%

REM ===== Locate VCPKG_ROOT =====
if not defined VCPKG_ROOT (
  if exist "%SCRIPT_DIR%vcpkg.exe" set "VCPKG_ROOT=%SCRIPT_DIR%"
)
if not defined VCPKG_ROOT (
  if exist "%SCRIPT_DIR%..\vcpkg\vcpkg.exe" set "VCPKG_ROOT=%SCRIPT_DIR%..\vcpkg"
)
if not defined VCPKG_ROOT (
  echo [ERROR] VCPKG_ROOT not set and vcpkg.exe not found near the project.
  echo         Please set VCPKG_ROOT first, for example:
  echo           set VCPKG_ROOT=C:\src\vcpkg
  exit /b 1
)

for %%I in ("%VCPKG_ROOT%") do set "VCPKG_ROOT=%%~fI"
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
  echo [STEP] Bootstrapping vcpkg...
  if exist "%VCPKG_ROOT%\bootstrap-vcpkg.bat" (
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics
    if errorlevel 1 (
      echo [ERROR] bootstrap-vcpkg failed
      exit /b 1
    )
  ) else (
    echo [ERROR] bootstrap-vcpkg.bat not found under %VCPKG_ROOT%
    exit /b 1
  )
)

REM ===== Manifest install (no package args) =====
if not exist "%CD%\vcpkg.json" (
  echo [ERROR] vcpkg.json not found in %CD%
  exit /b 1
)
echo [STEP] vcpkg install --triplet %TRIPLET%
"%VCPKG_ROOT%\vcpkg.exe" install --triplet %TRIPLET%
if errorlevel 1 (
  echo [ERROR] vcpkg install failed
  exit /b 1
)

REM ===== Choose generator =====
set "GEN=VS"
where ninja >nul 2>nul && set "GEN=NINJA"
if /i "%GEN%"=="NINJA" (
  echo [INFO] Using Ninja generator
) else (
  echo [INFO] Using Visual Studio 17 2022 generator
)

REM ===== Configure =====
if /i "%GEN%"=="NINJA" (
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=%TRIPLET%
) else (
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=%TRIPLET%
)
if errorlevel 1 (
  echo [ERROR] CMake configure failed
  exit /b 1
)

REM ===== Build =====
cmake --build build --config %CONFIG% -j
if errorlevel 1 (
  echo [ERROR] Build failed
  exit /b 1
)

REM ===== Locate executable =====
set "EXE_PATH=build\OpenGLStereoGenerator.exe"
if /i not "%GEN%"=="NINJA" set "EXE_PATH=build\%CONFIG%\OpenGLStereoGenerator.exe"

echo [DONE] Build finished.
if exist "%EXE_PATH%" (
  echo [INFO] Executable: "%EXE_PATH%"
  echo [TIP] Run: "%EXE_PATH%"
) else (
  echo [WARN] Executable not found at %EXE_PATH%
)

exit /b 0