@echo off
setlocal
cd /d "%~dp0"

set "MINGW=C:\Program Files (x86)\Dev-Cpp\MinGW32\bin"
if not exist "%MINGW%\g++.exe" (
    echo Could not find Dev-C++ MinGW at "%MINGW%".
    echo Edit build.bat and set MINGW to the bin folder of your Dev-C++ installation.
    exit /b 1
)
for %%I in ("%MINGW%") do set "MINGW_SHORT=%%~sI"
set "PATH=%MINGW_SHORT%;%PATH%"

if not exist build mkdir build

set "CXX=%MINGW_SHORT%\g++.exe"
set "RC=%MINGW_SHORT%\windres.exe"
set "CXXFLAGS=-std=c++17 -O2 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -DNDEBUG -Wall -Wextra -Wno-cast-function-type -Wno-unused-parameter -Isrc"
set "LIBS=-s -static -static-libgcc -static-libstdc++ -mwindows -lcomctl32 -lgdiplus -lshlwapi -lwinmm -lole32 -loleaut32 -luuid -lstrmiids -lquartz -lshell32 -luser32 -lgdi32 -lmsimg32 -ladvapi32 -lcomdlg32"

echo [1/3] Compiling resources...
"%RC%" -i MAKASOUNDER.rc -o build\app_res.o --input-format=rc --output-format=coff --codepage=65001
if errorlevel 1 goto :failed

echo [2/3] Compiling sources...
set "OBJECTS=build\common.o build\fileio.o build\lyrics.o build\metadata.o build\artwork.o build\audio.o build\settings.o build\association.o build\ui_main.o build\ui_float.o build\app.o build\main.o"
for %%F in (common fileio lyrics metadata artwork audio settings association ui_main ui_float app main) do (
    echo   - %%~F.cpp
    "%CXX%" %CXXFLAGS% -c "src\%%F.cpp" -o "build\%%F.o"
    if errorlevel 1 goto :failed
)

echo [3/3] Linking...
"%CXX%" %OBJECTS% build\app_res.o -o build\MAKASOUNDER.exe %LIBS%
if errorlevel 1 goto :failed

echo.
echo Built: %CD%\build\MAKASOUNDER.exe
if exist build\bass.dll echo BASS found next to the executable - native BASS backend enabled.
exit /b 0

:failed
echo.
echo Build failed.
exit /b 1
