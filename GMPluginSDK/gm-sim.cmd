@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\simulator\run.ps1" %*
exit /b %ERRORLEVEL%
