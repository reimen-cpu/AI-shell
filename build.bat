@echo off
setlocal

set SRC_DIR=%~dp0src
set OUT_DIR=%~dp0bin
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
del /Q "%OUT_DIR%\ai.exe" 2>nul


echo Building ai.exe...

g++ -o "%OUT_DIR%\ai.exe" -I "%SRC_DIR%" ^
    "%SRC_DIR%\main.cpp" ^
    "%SRC_DIR%\json_utils.cpp" ^
    "%SRC_DIR%\http_client.cpp" ^
    "%SRC_DIR%\context_manager.cpp" ^
    "%SRC_DIR%\wrapper.cpp" ^
    "%SRC_DIR%\command_processor.cpp" ^
    "%SRC_DIR%\memory.cpp" ^
    "%SRC_DIR%\process_runner.cpp" ^
    "%SRC_DIR%\command_cache.cpp" ^
    -lwinhttp -static-libgcc -static-libstdc++
    
copy /Y "%~dp0system_prompt.txt" "%OUT_DIR%\" >nul 2>&1

if %ERRORLEVEL% NEQ 0 (
    echo Build FAILED!
    exit /b %ERRORLEVEL%
)

echo Build SUCCESS! Output: %OUT_DIR%\ai.exe
endlocal
