@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0build-music-example.ps1" %*
exit /b %ERRORLEVEL%
