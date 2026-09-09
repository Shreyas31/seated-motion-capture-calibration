$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourceFrontend = Join-Path $repositoryRoot "frontend"
$releaseFrontend = Join-Path `
    $repositoryRoot `
    "dist\IRP_SeatedMoCap\frontend"

New-Item `
    -ItemType Directory `
    -Path $releaseFrontend `
    -Force | Out-Null

$frontendFiles = @(
    "__init__.py",
    "app.py",
    "app_config.py",
    "application_bootstrap.py",
    "calibration_page.py",
    "events_controller.py",
    "execution_controller.py",
    "presentation_controller.py",
    "process_supervisor.py",
    "protocol.py",
    "recording_page.py",
    "runtime_context.py",
    "sensor_mapping.py",
    "session_files.py",
    "session_state.py",
    "setup_page.py",
    "shutdown_controller.py",
    "system_check.py",
    "workflow_controller.py",
    "workflow_ui.py",
    "style.qss",
    "style_dark.qss",
    "requirements.txt"
)

foreach ($fileName in $frontendFiles) {
    $sourcePath = Join-Path $sourceFrontend $fileName
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Required frontend release file is missing: $sourcePath"
    }

    Copy-Item `
        -LiteralPath $sourcePath `
        -Destination (Join-Path $releaseFrontend $fileName) `
        -Force
}

Write-Host "Frontend release files synchronized to:"
Write-Host $releaseFrontend
