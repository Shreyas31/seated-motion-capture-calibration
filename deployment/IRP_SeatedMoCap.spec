# -*- mode: python ; coding: utf-8 -*-
from pathlib import Path


spec_location = Path(SPECPATH).resolve()
deployment_directory = (
    spec_location if spec_location.is_dir() else spec_location.parent
)
repository = deployment_directory.parent
frontend = repository / "frontend"

a = Analysis(
    [str(frontend / "app.py")],
    pathex=[str(repository)],
    binaries=[],
    datas=[
        (str(frontend / "style.qss"), "."),
        (str(frontend / "style_dark.qss"), "."),
    ],
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="IRP_SeatedMoCap",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name="IRP_SeatedMoCap",
)
