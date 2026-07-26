# Building SubSync

SubSync now uses a PEP 517/scikit-build-core build backed by CMake. The
supported baseline is:

- Python 3.10 or newer;
- a C++17 compiler and CMake 3.25 or newer;
- FFmpeg 6.1 development libraries;
- PocketSphinx 5.1 or newer within the 5.x API line;
- pkg-config (or pkgconf) so CMake can locate FFmpeg and PocketSphinx.

The Python dependencies are declared in `pyproject.toml`. `requirements.txt`
contains the same runtime-only dependency set for tools that still consume a
requirements file.

## CPU build

Create and activate a virtual environment, then build an editable development
install:

```sh
python -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -e ".[gui,dev]"
```

On Windows PowerShell, activate it with:

```powershell
.\.venv\Scripts\Activate.ps1
```

For a headless installation, omit the `gui` extra:

```sh
python -m pip install .
```

## CUDA backend

Install an NVIDIA CUDA toolkit supported by your compiler and driver, then
enable the backend through scikit-build-core:

```sh
python -m pip install . \
  --config-settings=cmake.define.SUBSYNC_ENABLE_CUDA=ON
```

CUDA is compiled into the extension only when this option is enabled. At
runtime, select it explicitly with `--matching-backend=cuda`, or use the
default `auto` mode.

## OpenCL backend

Install OpenCL headers plus an ICD loader/development package, then build with:

```sh
python -m pip install . \
  --config-settings=cmake.define.SUBSYNC_ENABLE_OPENCL=ON
```

To compile both accelerator backends:

```sh
python -m pip install . \
  --config-settings=cmake.define.SUBSYNC_ENABLE_CUDA=ON \
  --config-settings=cmake.define.SUBSYNC_ENABLE_OPENCL=ON
```

The OpenCL implementation targets the portable OpenCL 1.2 API and runs on
modern OpenCL 3.x drivers. It currently selects the first GPU device exposed
by the installed ICD.

## Runtime selection

The available modes are:

- `auto`: prefer CUDA, then OpenCL, and fall back to CPU;
- `cpu`: always use the reference CPU matcher;
- `cuda`: require a compiled, usable CUDA backend;
- `opencl`: require a compiled, usable OpenCL backend.

The same backend and batch threshold controls are available on the GUI's
Synchronization settings page.

In `auto` mode, small batches stay on the CPU because transfer and kernel
launch overhead would outweigh useful GPU work. The conservative default
crossover is 8,192 candidates and can be changed with `--gpu-min-batch`:

```sh
subsync-cmd --matching-backend=auto --gpu-min-batch=16384 ...
```

An explicit `cuda` or `opencl` selection fails early if that backend was not
compiled or no compatible device is available. `auto` remains usable if a
driver fails during matching by dropping back to CPU.

You can inspect the backends compiled into the extension:

```sh
python -c "import gizmo; print(gizmo.availableMatchingBackends())"
```

## Native dependency discovery

On Linux and macOS, install development packages through the platform package
manager and verify they are visible:

```sh
pkg-config --modversion pocketsphinx
pkg-config --modversion libavcodec
```

On Windows, a package manager such as vcpkg can provide the native libraries.
Configure `PKG_CONFIG_PATH` to point at the selected vcpkg triplet's
`lib/pkgconfig` directory before invoking `pip`.

This source line deliberately targets FFmpeg 6.1 ABI versions. FFmpeg 7 and 8
removed legacy channel-layout fields still used by the media pipeline; moving
to them requires a separate decoder/resampler API migration rather than an
unsafe version-only bump.

## Tests

The pure C++ matcher and geometry tests live under `gizmo/test`:

```sh
make -C gizmo/test
```

Builds with CUDA or OpenCL should run the same matcher parity cases on a host
with the corresponding SDK and hardware. WebAssembly remains CPU-only because
browser WebAssembly cannot access native CUDA or OpenCL runtimes.

## Standalone matcher benchmark

The matcher can be built and benchmarked without Python, FFmpeg, PocketSphinx,
or pkg-config. This is the quickest way to validate an accelerator toolchain
and measure whether transfer overhead is worthwhile on a particular device.

For an OpenCL build using vcpkg on Windows:

```powershell
cmake --fresh -S . -B build/benchmark-opencl `
  -G "Visual Studio 16 2019" -A x64 `
  -DSUBSYNC_BUILD_APP=OFF `
  -DSUBSYNC_BUILD_MATCHER_BENCHMARK=ON `
  -DSUBSYNC_ENABLE_CUDA=OFF `
  -DSUBSYNC_ENABLE_OPENCL=ON `
  -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build/benchmark-opencl --config Release
.\build\benchmark-opencl\Release\subsync-matcher-benchmark.exe
```

For CUDA, replace the backend definitions with:

```powershell
-T "cuda=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.3" `
-DSUBSYNC_ENABLE_CUDA=ON `
-DSUBSYNC_ENABLE_OPENCL=OFF `
-DCMAKE_CUDA_ARCHITECTURES=120
```

The `-T cuda=...` field points a Visual Studio generator at a standalone CUDA
toolkit and its bundled MSBuild integration files. It is required when `nvcc`
is installed but the CUDA `.props` and `.targets` files were not registered in
Visual Studio's global `BuildCustomizations` directory.

The benchmark verifies each accelerator's results against the CPU matcher,
including a Unicode case, before timing it. By default it reports steady-state
median and p95 latency for batches from 128 through 65,536 candidates. Its
parity check and warm-up calls populate the persistent candidate cache before
measurement. Timed calls still include query encoding, span construction and
transfer, kernel launch, and result transfer; newly encountered candidates also
include incremental encoding and upload. Use `--help` to select one backend,
batch size, or iteration count.

## Web build

The web client uses Node.js 22 or newer and the official Emscripten SDK
container. On a POSIX Docker host:

```sh
cd web
npm install
npm run native:build
npm run build
```

`native:build` fetches pinned FFmpeg 6.1.6 and PocketSphinx 5.1.1 sources,
builds the CPU-only WebAssembly modules, and leaves CUDA/OpenCL out of the
browser target.
