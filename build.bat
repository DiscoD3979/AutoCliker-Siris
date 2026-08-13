@echo off
cd /d "%~dp0"
call "D:\Visual code\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

if not exist "build\CMakeCache.txt" (
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
)

cmake --build build
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo ===== Build successful =====
echo Executable: build\AutoCliker-Siris.exe