# SubSync

SubSync automatically aligns subtitle timing against another subtitle track or
the spoken audio in a video. This fork modernizes the original application for
current Python and native toolchains and adds optional CUDA and OpenCL
acceleration to the word-matching stage.

> [!NOTE]
> The GPU-enabled Windows build is currently a pre-release. Keep a copy of the
> original subtitle while testing it and report the generated log with any
> problem.

## Download

The latest Windows x64 preview is
[Subsync 0.18.0 GPU Preview](https://github.com/blaaaakuu/subsync/releases/tag/0.18.0-dev.0).

Download `subsync-0.18.0.dev0-portable-win-x64.zip`, extract the complete
directory, and run `subsync.exe`. The package includes:

- the full graphical and command-line applications;
- an embedded Python 3.13 runtime and wxPython 4.2;
- FFmpeg 6.1.1 with subtitle character-set conversion;
- PocketSphinx 5.1.1 and an offline English speech model;
- CPU, CUDA, and OpenCL word-matching backends.

No Python installation or GPU SDK is needed on the target computer. CPU
matching works everywhere supported by the package. CUDA requires a compatible
NVIDIA display driver, while OpenCL requires a vendor OpenCL runtime supplied
with the GPU or CPU driver.

## What changed

- Python 3.10+ packaging through PEP 517 and scikit-build-core.
- CMake 3.25+ and C++17 native build.
- Modern pybind11, wxPython, PocketSphinx, and Python dependencies.
- FFmpeg 6.1 ABI support with `iconv` enabled for encoded subtitle streams.
- CUDA and OpenCL matchers with CPU parity checks and automatic fallback.
- Backend selection in both Settings and the command line.
- Portable Windows application and standalone GPU benchmark packages.
- Compatibility fixes for strict integer arguments in current wxPython.

GPU acceleration is deliberately limited to word matching. Media decoding,
speech recognition, and correlation retain their established CPU pipelines.
Small matching batches also stay on the CPU because device transfer and kernel
launch overhead can cost more than they save.

## Quick start

In the graphical application:

1. Open the subtitle that needs correction in the left panel.
2. Open a correctly timed subtitle, audio file, or video in the right panel.
3. Select the correct streams and languages.
4. Press **Start** and save the result after SubSync finds a correlation.

The portable build includes an English speech model. Other languages may
require a downloadable speech model or translation dictionary.

For command-line synchronization:

```powershell
.\subsync-cmd.exe --matching-backend=auto sync `
  --sub="C:\media\episode.unsynced.srt" `
  --ref="C:\media\episode.mkv" `
  --out="C:\media\episode.synced.srt" `
  --overwrite
```

Use `subsync-cmd.exe --help` for all stream, language, encoding, matching, and
output options.

## Matching backends

| Mode | Behaviour |
| --- | --- |
| `auto` | Uses CPU for small batches, then prefers CUDA, OpenCL, and finally CPU. |
| `cpu` | Always uses the reference CPU matcher. |
| `cuda` | Requires the CUDA backend and a compatible NVIDIA driver/device. |
| `opencl` | Requires the OpenCL backend and a compatible vendor runtime/device. |

The default automatic crossover is 8,192 candidates. It can be changed in
Settings or with `--gpu-min-batch`. Explicit `cuda` or `opencl` selection
reports an error when that backend is unavailable; `auto` safely falls back to
CPU.

## Building and testing

See the documentation for the workflow you need:

- [User guide](doc/usage.md)
- [Build and packaging guide](doc/install.md)
- [Architecture overview](doc/architecture.md)
- [Assets and speech models](doc/assets.md)
- [Documentation index](doc/index.md)

The standalone matcher benchmark can be built without Python, FFmpeg, or
PocketSphinx. Windows users can also create a portable diagnostic ZIP with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tools/package-gpu-pocket.ps1
```

The complete portable application is built with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tools/package-windows-full-portable.ps1
```

## Project status

This repository is a modernization fork of
[sc0ty/subsync](https://github.com/sc0ty/subsync), which was archived after the
original project was retired. Version `0.18.0.dev0` is intended for testing on
different Windows and GPU configurations before a stable `0.18.0` release.

Please report:

- the Windows version and GPU/driver;
- whether `cpu`, `cuda`, `opencl`, or `auto` was selected;
- the application log or GPU pocket-test report;
- a minimal subtitle/reference example when it can be shared.

## License

SubSync is distributed under
[GPL-3.0-or-later](LICENSE). Portable packages include the applicable
third-party notices under their `licenses` directory.
