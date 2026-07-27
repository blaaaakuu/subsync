SubSync Portable - Windows x64
==============================

This is the complete portable SubSync application. It includes the graphical
interface, command-line interface, embedded Python runtime, FFmpeg media
libraries, PocketSphinx speech recognizer, CUDA/OpenCL word matchers, and an
English speech model. FFmpeg includes iconv support for encoded text
subtitles.

Start the application
---------------------

Double-click subsync.exe for the graphical interface.

For command-line use, open Command Prompt in this directory:

  subsync-cmd.exe --help
  subsync-cmd.exe --version

Portable data
-------------

Settings, downloaded language models, dictionaries, logs, and assets are saved
inside this extracted directory. Keep the entire directory together and make
sure it is writable.

GPU support
-----------

CPU matching works without a GPU. CUDA requires a compatible NVIDIA display
driver; OpenCL requires a vendor OpenCL driver. Neither SDK is needed on the
target machine. Automatic matching uses the CPU for small batches and an
available GPU for batches of 8,192 candidates or more.

The application can be forced to one backend in Settings or from the command
line using:

  --matching-backend=cpu
  --matching-backend=cuda
  --matching-backend=opencl
  --matching-backend=auto

Target requirements
-------------------

* 64-bit Windows 10 or Windows 11.
* About 250 MB of free space after extraction.
* Write access to the extracted directory.
* Internet access is optional. It is used to download additional speech models
  and translation dictionaries.

Troubleshooting
---------------

Run subsync-cmd.exe --version first. If the GUI does not open, run the command
line executable from Command Prompt and enable a log for the failing task:

  subsync-cmd.exe --loglevel=DEBUG --logfile=subsync-debug.log ...

If an encoded subtitle cannot be opened, confirm that _internal\iconv-2.dll is
present. Keep the complete extracted directory together.

Licensing
---------

SubSync is GPL-3.0-or-later. See LICENSE and the notices under licenses.
