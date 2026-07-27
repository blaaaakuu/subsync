# Using SubSync

SubSync corrects subtitle timing by comparing a subtitle with a correctly
timed reference. The reference may be another subtitle stream or spoken audio
from a media file.

## Portable Windows application

Download the Windows x64 ZIP from the
[0.18.0 GPU Preview](https://github.com/blaaaakuu/subsync/releases/tag/0.18.0-dev.0),
extract the whole directory, and start `subsync.exe`.

The package is self-contained. Keep its files together and place it in a
writable directory because settings, logs, downloaded assets, and additional
language models are stored beside the executable.

## Graphical workflow

1. In the left panel, open the subtitle whose timestamps need correction.
2. In the right panel, open a correctly timed subtitle, audio file, or video.
3. Choose the relevant subtitle or audio stream when a container has more than
   one.
4. Confirm the source and reference languages. Audio matching requires a
   speech model for the selected reference language.
5. Press **Start**.
6. Review the correlation result and save the corrected subtitle.

The included English model permits offline English audio matching. SubSync may
offer to download models or dictionaries for other language combinations.

## Choosing a matching backend

Open **Settings** and select a backend on the Synchronization page:

- `auto` keeps small batches on CPU, tries CUDA for larger batches, then
  OpenCL, and falls back to CPU;
- `cpu` is the most portable and useful for comparison or troubleshooting;
- `cuda` requires a compatible NVIDIA driver and GPU;
- `opencl` requires an OpenCL implementation supplied by the hardware vendor.

The CUDA and OpenCL SDKs are build-time dependencies only. They are not needed
to run the portable package. GPU acceleration affects word matching rather
than media decoding or speech recognition, so performance gains depend on the
number and size of candidate batches.

## Command-line workflow

Run `subsync-cmd.exe --help` for the complete option list. A typical command is:

```powershell
.\subsync-cmd.exe --matching-backend=auto sync `
  --sub="C:\media\episode.unsynced.srt" `
  --ref="C:\media\episode.mkv" `
  --ref-stream-by-type=audio `
  --ref-lang=eng `
  --out="C:\media\episode.synced.srt" `
  --overwrite
```

To compare GPU and CPU behaviour, rerun the same task with
`--matching-backend=cpu`, `cuda`, or `opencl`. Use `--loglevel=DEBUG` and
`--logfile=PATH` when collecting a diagnostic log.

## Troubleshooting

### The GUI does not start

Run `subsync-cmd.exe --version` from Command Prompt. Confirm that the complete
ZIP was extracted and that `_internal` remains beside both executables.

### CUDA or OpenCL is unavailable

Update the vendor display/compute driver. The packaged CUDA runtime does not
replace an NVIDIA driver, and the packaged OpenCL ICD loader does not provide
a vendor implementation. Select `auto` or `cpu` to continue without a GPU.

### A subtitle cannot be decoded

The preview includes FFmpeg 6.1.1 with `iconv` for encoded text subtitles.
Confirm that `_internal\iconv-2.dll` is present, then collect a debug log that
contains the detected encoding and decoder name.

### Audio synchronization requests an asset

Choose the reference audio language explicitly. The portable package includes
English; other languages need a compatible PocketSphinx model from SubSync's
asset service.

### Automatic matching is slower on a small file

This is expected when candidate batches remain below the GPU crossover or when
kernel-launch overhead dominates. CPU is often fastest for small comparisons.
The default automatic crossover is 8,192 candidates and can be adjusted with
`--gpu-min-batch`.
