param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "RelWithDebInfo",
    [string]$BackendBuildDirectory = "",
    [string]$OpenSimBuildDirectory = "",
    [string]$OpenSimHome = $env:OPENSIM_HOME,
    [string]$RajagopalSourceModel = $env:RAJAGOPAL_SOURCE_MODEL,
    [switch]$SkipNativeBuild
)

$ErrorActionPreference = "Stop"
$repository = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$releaseRoot = Join-Path $repository "release"
$pyInstallerDist = Join-Path $releaseRoot "pyinstaller-dist"
$pyInstallerWork = Join-Path $releaseRoot "pyinstaller-work"
$staging = Join-Path $releaseRoot "IRP_SeatedMoCap"
$packagingEnvironment = Join-Path $repository ".packaging-venv"

function Select-BuildDirectory {
    param([string]$Requested, [string[]]$Candidates, [string]$Description)
    if ($Requested) {
        if (-not (Test-Path -LiteralPath $Requested -PathType Container)) {
            throw "$Description build directory does not exist: $Requested"
        }
        return (Resolve-Path -LiteralPath $Requested).Path
    }
    foreach ($candidate in $Candidates) {
        $candidatePath = Join-Path $repository $candidate
        if (Test-Path -LiteralPath $candidatePath -PathType Container) {
            return (Resolve-Path -LiteralPath $candidatePath).Path
        }
    }
    throw "Could not find a configured $Description build directory. Pass it explicitly."
}

$BackendBuildDirectory = Select-BuildDirectory $BackendBuildDirectory @(
    "out\build\x64-opensim-relwithdebinfo"
) "backend"
$OpenSimBuildDirectory = Select-BuildDirectory $OpenSimBuildDirectory @(
    "out\build\x64-opensim-relwithdebinfo"
) "OpenSim"

if (-not $OpenSimHome -and (Test-Path -LiteralPath "C:\OpenSim 4.5")) {
    $OpenSimHome = "C:\OpenSim 4.5"
}
if (-not $OpenSimHome -or -not (Test-Path -LiteralPath $OpenSimHome -PathType Container)) {
    throw "OpenSim home was not found. Set OPENSIM_HOME or pass -OpenSimHome."
}
$OpenSimHome = (Resolve-Path -LiteralPath $OpenSimHome).Path

if (-not $RajagopalSourceModel) {
    $RajagopalSourceModel = Join-Path $HOME "Documents\OpenSim\4.5\Code\Python\OpenSenseExample\Rajagopal_2015.osim"
}
if (-not (Test-Path -LiteralPath $RajagopalSourceModel -PathType Leaf)) {
    throw "Rajagopal source model was not found. Set RAJAGOPAL_SOURCE_MODEL or pass -RajagopalSourceModel."
}
$RajagopalSourceModel = (Resolve-Path -LiteralPath $RajagopalSourceModel).Path

if (-not $SkipNativeBuild) {
    cmake --build $BackendBuildDirectory --config $Configuration --target backend
    if ($LASTEXITCODE -ne 0) { throw "Backend build failed." }
    cmake --build $OpenSimBuildDirectory --config $Configuration --target generate_seated_mocap_model opensim_rt_viewer
    if ($LASTEXITCODE -ne 0) { throw "OpenSim tools build failed." }
}

$backendExe = Join-Path $BackendBuildDirectory "backend\$Configuration\backend.exe"
$viewerExe = Join-Path $OpenSimBuildDirectory "opensim\$Configuration\opensim_rt_viewer.exe"
$generatorExe = Join-Path $OpenSimBuildDirectory "opensim\$Configuration\generate_seated_mocap_model.exe"
foreach ($nativeFile in @($backendExe, $viewerExe, $generatorExe)) {
    if (-not (Test-Path -LiteralPath $nativeFile -PathType Leaf)) {
        throw "Required native executable is missing: $nativeFile"
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $packagingEnvironment "Scripts\python.exe"))) {
    py -3 -m venv $packagingEnvironment
    if ($LASTEXITCODE -ne 0) { throw "Could not create the packaging virtual environment." }
}
$packagingPython = Join-Path $packagingEnvironment "Scripts\python.exe"
& $packagingPython -m pip install --disable-pip-version-check -r (Join-Path $repository "frontend\requirements.txt") -r (Join-Path $PSScriptRoot "requirements-build.txt")
if ($LASTEXITCODE -ne 0) { throw "Packaging dependencies could not be installed." }

foreach ($directory in @($pyInstallerDist, $pyInstallerWork, $staging)) {
    if (Test-Path -LiteralPath $directory) {
        Remove-Item -LiteralPath $directory -Recurse -Force
    }
}
New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null

& $packagingPython -m PyInstaller --noconfirm --clean `
    --distpath $pyInstallerDist `
    --workpath $pyInstallerWork `
    (Join-Path $PSScriptRoot "IRP_SeatedMoCap.spec")
if ($LASTEXITCODE -ne 0) { throw "PyInstaller failed." }

$frozenBundle = Join-Path $pyInstallerDist "IRP_SeatedMoCap"
New-Item -ItemType Directory -Path $staging -Force | Out-Null
Copy-Item -Path (Join-Path $frozenBundle "*") -Destination $staging -Recurse -Force

$bin = Join-Path $staging "bin"
$resources = Join-Path $staging "resources"
$config = Join-Path $staging "config"
$bundledOpenSim = Join-Path $staging "runtime\OpenSim"
New-Item -ItemType Directory -Path $bin, $resources, $config, $bundledOpenSim -Force | Out-Null
Copy-Item -LiteralPath $backendExe, $viewerExe, $generatorExe -Destination $bin -Force
Copy-Item -Path (Join-Path $repository "dependencies\xsens\lib\*.dll") -Destination $bin -Force
Copy-Item -LiteralPath (Join-Path $repository "generated\pose_presets.json") -Destination $resources -Force
Copy-Item -LiteralPath (Join-Path $repository "config\sensor_mapping.json") -Destination $config -Force
Copy-Item -LiteralPath $RajagopalSourceModel -Destination (Join-Path $resources "Rajagopal_2015.osim") -Force
Copy-Item -LiteralPath (Join-Path $OpenSimHome "bin") -Destination $bundledOpenSim -Recurse -Force
Copy-Item -LiteralPath (Join-Path $OpenSimHome "Geometry") -Destination $bundledOpenSim -Recurse -Force

foreach ($licenseName in @("LICENSE", "LICENSE.txt", "NOTICE", "NOTICE.txt")) {
    $licensePath = Join-Path $OpenSimHome $licenseName
    if (Test-Path -LiteralPath $licensePath -PathType Leaf) {
        Copy-Item -LiteralPath $licensePath -Destination $bundledOpenSim -Force
    }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "README.md") -Destination (Join-Path $staging "README.md") -Force

$manifestFiles = @(
    (Join-Path $staging "IRP_SeatedMoCap.exe"),
    (Join-Path $bin "backend.exe"),
    (Join-Path $bin "opensim_rt_viewer.exe"),
    (Join-Path $bin "generate_seated_mocap_model.exe")
)
$manifest = [ordered]@{
    createdUtc = (Get-Date).ToUniversalTime().ToString("o")
    configuration = $Configuration
    files = @($manifestFiles | ForEach-Object {
        $relativePath = $_.Substring($staging.Length).TrimStart("\", "/")
        [ordered]@{
            path = $relativePath
            sha256 = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash
        }
    })
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $staging "build_manifest.json") -Encoding UTF8

Write-Host "Portable application created at: $staging"
