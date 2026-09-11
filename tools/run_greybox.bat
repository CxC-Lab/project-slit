@echo off
setlocal
cd /d "%~dp0.."
if not exist "build\Release\project_slit.exe" (
    echo Release executable missing. Build project_slit with --config Release first.
    exit /b 1
)
"build\Release\project_slit.exe" greybox
