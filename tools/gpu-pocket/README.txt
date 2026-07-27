SubSync GPU Pocket - Windows x64
================================

This portable package tests SubSync's word matcher on the CPU and on every
compiled accelerator backend available on the machine.

No installation, Python environment, CUDA SDK, or OpenCL SDK is required.
Extract the complete folder before running it.

Quick test
----------

Double-click test-this-machine.cmd. It records system, driver, CPU, CUDA, and
OpenCL results under the results directory. Send that text file back with any
bug report.

Longer test
-----------

Double-click benchmark-full.cmd to test all default candidate counts with 50
timed iterations.

Custom test
-----------

Open Command Prompt in this directory and run, for example:

  benchmark-custom.cmd --backend cuda --batch 65536 --iterations 100
  benchmark-custom.cmd --backend opencl --batch 16384 --iterations 50
  benchmark-custom.cmd --help

Target requirements
-------------------

* 64-bit Windows 10 or Windows 11.
* CPU testing has no GPU requirement.
* CUDA testing requires an NVIDIA GPU and a compatible NVIDIA display driver.
  The CUDA SDK is not needed on the target computer.
* OpenCL testing requires a vendor OpenCL GPU driver. OpenCL.dll in this
  package is only the portable ICD loader; it does not replace the GPU driver.

The CUDA binary contains native kernels for NVIDIA compute capabilities 7.5,
8.0, 8.6, 8.9, 9.0, 10.0, 10.3, 11.0, 12.0, and 12.1, plus forward-compatible
PTX starting at 7.5. Older NVIDIA GPUs can still be tested through OpenCL if
their installed driver exposes it.

Interpreting results
--------------------

Each GPU result is checked against the CPU result before it is timed. Matching
checksums mean the backends agreed. "unavailable" means that backend was
compiled into the package but its driver or compatible hardware was not found.

Median is typical latency; p95 exposes slower outliers. CPU usually wins for
small candidate batches because a GPU launch has fixed overhead. SubSync's
automatic backend currently switches at 8,192 candidates.

This package benchmarks only the word matcher. It does not contain the GUI,
FFmpeg subtitle/audio pipeline, PocketSphinx speech recognition, language
assets, or subtitle writer. Use the full portable application ZIP for
end-to-end synchronization tests.

Licensing
---------

SubSync is distributed under GPL-3.0-or-later; see LICENSE. Third-party loader
notices are under licenses.
