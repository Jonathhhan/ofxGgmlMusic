@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "RUNNER=%SCRIPT_DIR%..\tools\musicgen_hf_runner.py"

if not "%OFXGGML_MUSIC_PYTHON%"=="" (
	"%OFXGGML_MUSIC_PYTHON%" "%RUNNER%" %*
	exit /b %ERRORLEVEL%
)

where python.exe >nul 2>nul
if errorlevel 1 goto python_missing
python.exe "%RUNNER%" %*
exit /b %ERRORLEVEL%

:python_missing
echo Could not find python.exe.
echo Set OFXGGML_MUSIC_PYTHON to a Python executable with torch, transformers, and numpy installed.
exit /b 1
