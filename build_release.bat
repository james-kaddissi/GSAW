@echo off
setlocal

set BUILD_DIR=build
set CONFIG=Release

cmake --build %BUILD_DIR% --config %CONFIG%
if errorlevel 1 exit /b %errorlevel%

set SRC=%CD%\%BUILD_DIR%\gs-engine-build\%CONFIG%
set DST=%CD%\%BUILD_DIR%\%CONFIG%

if not exist "%DST%" mkdir "%DST%"

copy /Y "%SRC%\*.dll" "%DST%\"

endlocal