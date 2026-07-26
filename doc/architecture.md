# SubSync architecture overview

SubSync has a Python application layer and a native `gizmo` module. Python
owns the GUI, settings, asset management, and synchronization jobs. The C++17
module owns media decoding, speech recognition, word matching, and line
correlation.

## Matching seam

`WordMatcher` is the narrow boundary between synchronization and compute
backends. The synchronizer builds a bounded candidate batch for each incoming
word and asks the selected matcher for matching indices. Backend-specific
allocation, device discovery, encoding, and kernels stay behind that
interface.

The reference CPU adapter calls the original `compareWords` implementation.
CUDA and OpenCL adapters encode UTF-8 code points into flat batches and execute
the same prefix/case-folded scoring rule in parallel. Their append-only caches
retain immutable candidate encodings on the host and device, so repeated
windows transfer only spans and newly encountered words. Reusable buffers grow
geometrically rather than allocating on every match call. `auto` wraps an
accelerator with a CPU fallback and a configurable minimum GPU batch size.

This keeps media extraction and correlation independent of accelerator SDKs:
a CPU wheel has neither CUDA nor OpenCL build requirements, while accelerated
wheels add only their selected adapter.

## Media and speech

FFmpeg 6.1 decodes audio and subtitle streams. Audio is resampled to the
PocketSphinx model format. PocketSphinx 5.1's supported endpointer API segments
speech before decoder input; arbitrary FFmpeg audio frames are buffered into
the fixed frame size required by the VAD.

## Python boundary

pybind11 exposes the native pipeline as the `gizmo` extension. Matching
selection crosses this boundary as two stable values:

- `matchingBackend`: `auto`, `cpu`, `cuda`, or `opencl`;
- `gpuMinBatch`: the candidate count at which `auto` uses an accelerator.

The command line and persisted settings feed those values into `Correlator`.
`gizmo.availableMatchingBackends()` reports which optional adapters were
compiled into the current extension.

## WebAssembly

The web build shares the CPU matcher and correlation logic. It intentionally
does not compile CUDA or OpenCL adapters: browser WebAssembly has no direct
access to either native runtime.
