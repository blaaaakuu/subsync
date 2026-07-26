# -*- mode: python ; coding: utf-8 -*-

from pathlib import Path


repo = Path(SPECPATH).parents[1]
build_root = repo / "build" / "full-portable"
native_dir = build_root / "app" / "Release"
vcpkg_bin = build_root / "vcpkg_installed" / "x64-windows" / "bin"
pocketsphinx_bin = build_root / "prefix" / "bin"

native_modules = list(native_dir.glob("gizmo*.pyd"))
if len(native_modules) != 1:
    raise RuntimeError(
        f"Expected one gizmo extension in {native_dir}, found {native_modules}"
    )

native_dlls = [
    *vcpkg_bin.glob("*.dll"),
    pocketsphinx_bin / "pocketsphinx.dll",
]
missing_dlls = [path for path in native_dlls if not path.is_file()]
if missing_dlls:
    raise RuntimeError(f"Missing native runtime DLLs: {missing_dlls}")

binaries = [(str(native_modules[0]), ".")]
binaries.extend((str(path), ".") for path in native_dlls)

datas = [
    (str(repo / "LICENSE"), "."),
    (str(repo / "subsync" / "key.pub"), "."),
    (str(repo / "subsync" / "img"), "img"),
    (str(repo / "subsync" / "locale"), "locale"),
]

analysis = Analysis(
    [str(repo / "bin" / "portable")],
    pathex=[str(native_dir), str(repo)],
    binaries=binaries,
    datas=datas,
    hiddenimports=[],
    hookspath=[],
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)

pyz = PYZ(analysis.pure)

gui = EXE(
    pyz,
    analysis.scripts,
    [],
    exclude_binaries=True,
    name="subsync",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,
    icon=str(repo / "resources" / "icon.ico"),
)

cli = EXE(
    pyz,
    analysis.scripts,
    [],
    exclude_binaries=True,
    name="subsync-cmd",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=True,
    icon=str(repo / "resources" / "icon.ico"),
)

collection = COLLECT(
    gui,
    cli,
    analysis.binaries,
    analysis.datas,
    strip=False,
    upx=False,
    name="subsync-portable",
)
