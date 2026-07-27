# Snap packaging status

The legacy Snapcraft manifest was retired during the Python 3.10/C++17
modernization. It was pinned to Python 3.5, FFmpeg 4.2, pybind11 2.4, the
removed standalone sphinxbase project, and an obsolete wxPython wheel.

A replacement should target a current Snapcraft base and consume the same
CMake/PEP 517 build defined at the repository root. It must also package
FFmpeg 6.1 with `iconv`, PocketSphinx 5.1, wxPython 4.2, and any selected GPU
runtime dependencies.

CUDA and OpenCL should remain optional Snap build features, with the CPU
backend as the portable default. Device interfaces and driver libraries need
explicit confinement testing before advertising GPU support.

Snap packaging should be restored only with CI that builds and launches the
resulting snap. Until then, the supported prebuilt test artifact is the
[Windows x64 GPU preview](https://github.com/blaaaakuu/subsync/releases/tag/0.18.0-dev.0).
