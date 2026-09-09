@echo off
setlocal

cd /d "%~dp0"

where py >nul 2>nul
if errorlevel 1 (
    echo ERROR: Python launcher was not found.
    echo Install 64-bit Python 3.11 or newer.
    pause
    exit /b 1
)

if not exist ".venv\Scripts\python.exe" (
    py -m venv .venv
    if errorlevel 1 (
        echo ERROR: Could not create Python environment.
        pause
        exit /b 1
    )
)

".venv\Scripts\python.exe" -m pip install --upgrade pip
if errorlevel 1 goto install_failed

".venv\Scripts\python.exe" -m pip install -r "frontend\requirements.txt"
if errorlevel 1 goto install_failed

echo.
echo Frontend setup completed successfully.
echo Run launch_frontend.cmd to start the application.
pause
exit /b 0

:install_failed
echo.
echo ERROR: Frontend dependency installation failed.
pause
exit /b 1
