@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Launch.ps1" -Mode Framework %*
if errorlevel 1 pause
