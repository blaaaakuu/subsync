# SubSync

SubSync aligns subtitle timing against another subtitle or an audio track.
This modernization branch provides a Python 3.10+/C++17 build, PocketSphinx
5.1 speech endpointing, and optional CUDA or OpenCL word-matching backends.

The default `auto` matcher uses an available accelerator for large candidate
batches and preserves the CPU implementation as both the reference path and a
runtime fallback. CPU-only builds remain supported.

See [the build guide](doc/install.md) for native prerequisites, accelerator
flags, runtime selection, and tests. [The architecture overview](doc/architecture.md)
describes the backend seam and data flow.

The original upstream project announced that it was no longer actively
maintained in [issue #197](https://github.com/sc0ty/subsync/issues/197). This
branch should therefore be treated as a source modernization until new release
artifacts and hardware CI are established.
