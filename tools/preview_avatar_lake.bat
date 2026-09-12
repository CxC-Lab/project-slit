@echo off
setlocal
cd /d "%~dp0.."
if not exist "build\Release\preview.exe" (
    echo preview.exe missing. Build Release first.
    exit /b 1
)
"build\Release\preview.exe" avatar_lake --cover --grid
