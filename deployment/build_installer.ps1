param([switch]$SkipReleaseBuild)

$ErrorActionPreference = "Stop"
if (-not $SkipReleaseBuild) {
    & (Join-Path $PSScriptRoot "build_release.ps1")
    if ($LASTEXITCODE -ne 0) { throw "Release assembly failed." }
}

$isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
$isccCandidates = @(
    if ($isccCommand) { $isccCommand.Source }
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
) | Where-Object { $_ }
$iscc = $isccCandidates |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if (-not $iscc) {
    throw "Inno Setup 6 compiler (ISCC.exe) was not found. Install it or add its directory to PATH."
}

Write-Host "Using Inno Setup compiler: $iscc"
& $iscc (Join-Path $PSScriptRoot "IRP_SeatedMoCap.iss")
if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed." }
Write-Host "Installer created under release\installer."
