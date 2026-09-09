@echo off
setlocal

cd /d "%~dp0"

if not exist ".venv\Scripts\python.exe" (
    echo ERROR: Frontend environment is not installed.
    echo Run setup_frontend.cmd first.
    pause
    exit /b 1
)

if "%OPENSIM_HOME%"=="" (
    echo ERROR: OPENSIM_HOME is not configured.
    pause
    exit /b 1
)

set "PATH=%OPENSIM_HOME%\bin;%PATH%"

".venv\Scripts\python.exe" -m frontend.app

if errorlevel 1 (
    echo.
    echo The frontend exited with an error.
    echo Check Documents\IRP_SeatedMoCap_Data\logs.
    pause
)

endlocal
