# SubSync architecture

SubSync is split between a Python application layer and the native `gizmo`
extension. Python owns the GUI, command-line workflow, persisted settings,
assets, subtitle writing, and synchronization jobs. C++17 owns media decoding,
speech recognition, word matching, and line correlation.

```text
GUI / CLI
   |
Python synchronization controller
   |
pybind11 `gizmo` extension
   +-- FFmpeg demux and audio/subtitle decoding
   +-- PocketSphinx endpointing and recognition
   +-- CPU / CUDA / OpenCL word matching
   `-- correlation and timing formula
   |
pysubs2 output writer
```

## Synchronization pipeline

An input file is probed by FFmpeg and represented as a subtitle or reference
stream. Text subtitles are decoded to timed words. Audio references are
resampled to the PocketSphinx model format, segmented by the supported
PocketSphinx 5.1 endpointer API, and converted to timed recognition results.

The correlator compares incoming words against a bounded candidate window. It
collects synchronization points, rejects outliers, and calculates the timing
formula applied by the Python subtitle writer.

FFmpeg 6.1 is intentionally retained for this source line because the media
pipeline still uses APIs removed in FFmpeg 7. The Windows portable build enables
`iconv`; without it, FFmpeg cannot open a text subtitle when SubSync supplies
the detected character encoding.

## Matching seam

`WordMatcher` is the narrow interface between correlation and compute
backends. Backend-specific device discovery, allocation, encoding, caches, and
kernels remain behind that interface.

The CPU adapter calls the reference `compareWords` implementation. CUDA and
OpenCL encode UTF-8 code points into flat arrays and apply the same
prefix/case-folded scoring rule in parallel. Accelerator results are checked
against the CPU implementation by the standalone benchmark.

Both GPU adapters maintain append-only candidate caches. Immutable candidate
encodings stay on the host and device, repeated windows transfer only spans,
and buffers grow geometrically instead of reallocating for every match call.

The dispatcher implements four modes:

- `auto`: CPU below `gpuMinBatch`; above it, prefer CUDA, then OpenCL, with CPU
  fallback if initialization or matching fails;
- `cpu`: always use the reference implementation;
- `cuda`: require a compiled CUDA adapter and usable NVIDIA device;
- `opencl`: require a compiled OpenCL adapter and usable GPU device.

The default crossover is 8,192 candidates. GPU acceleration is not applied to
FFmpeg decoding, PocketSphinx recognition, or correlation fitting.

## Python boundary

pybind11 exposes the native pipeline as `gizmo`. Backend configuration crosses
the boundary as two stable values:

- `matchingBackend`: `auto`, `cpu`, `cuda`, or `opencl`;
- `gpuMinBatch`: the candidate count at which `auto` moves matching to a GPU.

The GUI Settings window and command-line parser persist or supply those values
to `Correlator`. `gizmo.availableMatchingBackends()` reports the adapters
compiled into the loaded extension.

The modern package uses PEP 517 with scikit-build-core. CMake can build the
application extension, the standalone benchmark, or both.

## Portable Windows distribution

`tools/package-windows-full-portable.ps1` creates an isolated Python
environment, installs pinned Python dependencies, resolves FFmpeg and OpenCL
through a vcpkg manifest, builds PocketSphinx 5.1.1, compiles `gizmo` with CUDA
and OpenCL, and freezes two entry points with PyInstaller:

- `subsync.exe`: windowed GUI;
- `subsync-cmd.exe`: console application.

Runtime DLLs, `iconv`, the English model, assets metadata, licenses, and the
standalone matcher benchmark are staged beside the executables. The portable
launcher redirects configuration and asset paths to the extracted directory.

## WebAssembly

The web build shares the CPU matcher and correlation logic. It intentionally
omits CUDA and OpenCL because browser WebAssembly cannot access native GPU
runtimes through those APIs. Its FFmpeg and PocketSphinx dependencies are built
inside the pinned Emscripten toolchain.
