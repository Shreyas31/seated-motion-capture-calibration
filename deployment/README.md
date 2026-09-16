# IRP Seated Motion Capture

## Recipient installation

Run `IRP_SeatedMoCap_Setup.exe`, then start **IRP Seated Motion Capture** from the Start menu. Python, PySide6, OpenSim, the pose presets and the Rajagopal
source model are included in the validated installer; environment variables are not required.

The Xsens Awinda USB/radio driver must still be installed because Windows hardware drivers cannot safely be deployed as ordinary application files.

## Data location

Patient models, recordings and logs are written to:

`Documents\IRP_SeatedMoCap_Data`

Do not place identifiable participant data in a public repository.

## Creating a release

On the development computer, run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File deployment\build_release.ps1
```

This builds the native programs, creates a private packaging virtual environment, freezes the PySide6 frontend, and assembles a self-contained app under `release\IRP_SeatedMoCap`.

To create the installer, install Inno Setup 6 and run:

```powershell
powershell -ExecutionPolicy Bypass -File deployment\build_installer.ps1 -SkipReleaseBuild
```

The installer is written to `release\installer`. If OpenSim or the Rajagopal model are not in their usual locations, pass `-OpenSimHome` and `-RajagopalSourceModel` to `build_release.ps1`.

## Validation before distribution

Test the installer on a clean x64 Windows computer or VM with no Python or OpenSim installation. Run system check, generate a model, connect the Awinda hardware, calibrate, record a CSV and launch the viewer.

Review third-party redistribution terms before external distribution. The builder copies runtime files but does not grant additional redistribution rights. Retain the bundled licence and third-party notices in every distribution.
