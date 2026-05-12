@echo off
setlocal
powershell -ExecutionPolicy Bypass -NoProfile -File "%~dp0generate-procedural-music.ps1" %*
exit /b %ERRORLEVEL%
