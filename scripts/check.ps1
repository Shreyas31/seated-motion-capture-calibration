param(
    [ValidateSet(
        "x64-debug",
        "x64-release",
        "x64-opensim-relwithdebinfo"
    )]
    [string]$Preset = "x64-debug",
    [switch]$SkipPython,
    [switch]$SkipFormat
)

$ErrorActionPreference = "Stop"
$repository = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Assert-CommandSucceeded {
    param([string]$Step)

    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE."
    }
}

function Resolve-ClangFormat {
    $command = Get-Command clang-format.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $installations = @()
    if ($env:VSINSTALLDIR) {
        $installations += $env:VSINSTALLDIR
    }

    $vswhere = Join-Path (
        [Environment]::GetFolderPath("ProgramFilesX86")
    ) "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installation = & $vswhere -latest -products * -property installationPath
        if ($LASTEXITCODE -eq 0 -and $installation) {
            $installations += $installation
        }
    }

    foreach ($installation in $installations | Select-Object -Unique) {
        foreach ($relativePath in @(
            "VC\Tools\Llvm\x64\bin\clang-format.exe",
            "VC\Tools\Llvm\bin\clang-format.exe"
        )) {
            $candidate = Join-Path $installation $relativePath
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return $candidate
            }
        }
    }

    throw (
        "clang-format was not found. Install the Visual Studio C++ Clang tools " +
        "or pass -SkipFormat for a build-only check."
    )
}

function Get-FirstPartyCppFiles {
    $sourceDirectories = @(
        "backend\include",
        "backend\src",
        "backend\tests",
        "opensim\include",
        "opensim\src",
        "opensim\tests",
        "shared\include"
    )

    foreach ($directory in $sourceDirectories) {
        Get-ChildItem -LiteralPath (Join-Path $repository $directory) -Recurse -File |
            Where-Object {
                $_.Extension -in @(".cpp", ".h", ".hpp")
            }
    }
}

Push-Location $repository
try {
    if (-not $SkipPython) {
        $python = Join-Path $repository ".venv\Scripts\python.exe"
        if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
            throw (
                "Python environment not found at $python. " +
                "Follow the setup commands in README.md or pass -SkipPython."
            )
        }

        Write-Host "Checking Python lint..."
        & $python -m ruff check --no-cache frontend
        Assert-CommandSucceeded "Ruff"

        if (-not $SkipFormat) {
            Write-Host "Checking Python formatting..."
            & $python -m ruff format --no-cache --check frontend
            Assert-CommandSucceeded "Ruff format"
        }

        Write-Host "Running Python tests..."
        & $python -B -m pytest -p no:cacheprovider
        Assert-CommandSucceeded "Pytest"
    }

    if (-not $SkipFormat) {
        $clangFormat = Resolve-ClangFormat
        $incorrectlyFormatted = @()

        Write-Host "Checking C++ formatting..."
        foreach ($file in Get-FirstPartyCppFiles) {
            $replacements = & $clangFormat -style=file -output-replacements-xml $file.FullName
            Assert-CommandSucceeded "clang-format"
            if ($replacements -match "<replacement ") {
                $incorrectlyFormatted += $file.FullName
            }
        }

        if ($incorrectlyFormatted.Count -gt 0) {
            throw (
                "C++ formatting differs from .clang-format:`n" +
                ($incorrectlyFormatted -join "`n")
            )
        }
    }

    Write-Host "Configuring native preset: $Preset"
    cmake --preset $Preset
    Assert-CommandSucceeded "CMake configure"

    Write-Host "Building native preset: $Preset"
    cmake --build --preset $Preset
    Assert-CommandSucceeded "CMake build"

    Write-Host "Running native tests: $Preset"
    ctest --preset $Preset
    Assert-CommandSucceeded "CTest"

    Write-Host "All requested checks passed."
}
finally {
    Pop-Location
}
