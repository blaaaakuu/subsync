# Snap packaging status

The legacy Snapcraft manifest was retired during the Python 3.10/C++17
modernization. It was pinned to Python 3.5, FFmpeg 4.2, pybind11 2.4, the
removed standalone sphinxbase project, and an obsolete wxPython wheel.

A replacement should target a current Snapcraft base and consume the same
CMake/PEP 517 build defined at the repository root. It should be restored only
with CI that builds and launches the resulting snap; keeping the old manifest
would advertise a packaging path that cannot produce this source tree.
