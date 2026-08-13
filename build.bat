@echo off
cd /d "%~dp0"
call "D:\Visual code\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

set PATH=D:\LLVM\bin;%PATH%

if not exist "build\CMakeCache.txt" (
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=D:/LLVM/bin/clang-cl.exe -DCMAKE_MT="C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/mt.exe" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
)

cmake --build build
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo ===== Build successful =====
echo Executable: build\AutoCliker-Siris.exe