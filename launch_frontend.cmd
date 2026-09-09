@echo off
setlocal

cd /d "%~dp0"

if not exist ".venv\Scripts\python.exe" (
    echo ERROR: Python environment not found.
    echo Run: py -m venv .venv
    echo Then install frontend\requirements.txt
    pause
    exit /b 1
)

".venv\Scripts\python.exe" -m frontend.app

if errorlevel 1 (
    echo.
    echo The frontend exited with an error.
    echo Check logs\frontend.log for details.
    pause
)

endlocal
