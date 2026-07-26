[CmdletBinding()]
param(
    [string]$BuildDir = "build/full-portable",
    [string]$DistDir = "dist",
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$CudaToolkitPath = $env:CUDA_PATH,
    [string]$Generator = "Visual Studio 16 2019",
    [string]$CudaArchitectures =
        "75-real;80-real;86-real;89-real;90-real;100-real;103-real;" +
        "110-real;120-real;121-real;75-virtual",
    [switch]$SkipDependencies,
    [switch]$SkipNativeBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$distRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot $DistDir))
$venvRoot = Join-Path $buildRoot "venv"
$python = Join-Path $venvRoot "Scripts\python.exe"
$vcpkgInstalled = Join-Path $buildRoot "vcpkg_installed"
$prefix = Join-Path $buildRoot "prefix"
$pocketsphinxSource = Join-Path $buildRoot "pocketsphinx-src"
$pocketsphinxBuild = Join-Path $buildRoot "pocketsphinx-build"
$appBuild = Join-Path $buildRoot "app"

function Invoke-Checked {
    param(
        [Parameter(Mandatory, Position=0)]
        [string]$Program,
        [Parameter(Position=1, ValueFromRemainingArguments=$true)]
        [object[]]$Arguments
    )

    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE."
    }
}

function Find-FirstDirectory {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Container)) {
            return [IO.Path]::GetFullPath($candidate)
        }
    }
    return $null
}

if (-not $VcpkgRoot) {
    $VcpkgRoot = Find-FirstDirectory @(
        (Join-Path $env:USERPROFILE "dev\vcpkg"),
        (Join-Path $env:USERPROFILE "vcpkg"),
        "C:\dev\vcpkg"
    )
}
if (-not $VcpkgRoot) {
    throw "vcpkg was not found. Pass -VcpkgRoot or set VCPKG_ROOT."
}
$vcpkg = Join-Path $VcpkgRoot "vcpkg.exe"
$vcpkgToolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"

if (-not $CudaToolkitPath) {
    $cudaRoot = Join-Path $env:ProgramFiles "NVIDIA GPU Computing Toolkit\CUDA"
    if (Test-Path -LiteralPath $cudaRoot) {
        $CudaToolkitPath = Get-ChildItem -LiteralPath $cudaRoot -Directory |
            Sort-Object Name -Descending |
            Select-Object -First 1 -ExpandProperty FullName
    }
}
if (-not $CudaToolkitPath -or
        -not (Test-Path -LiteralPath (Join-Path $CudaToolkitPath "bin\nvcc.exe"))) {
    throw "CUDA toolkit not found. Pass -CudaToolkitPath or set CUDA_PATH."
}

$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) {
    $cmakeCommand.Source
} elseif (Test-Path -LiteralPath "C:\Python313\Scripts\cmake.exe") {
    "C:\Python313\Scripts\cmake.exe"
} else {
    throw "CMake was not found."
}

New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null

if (-not $SkipDependencies) {
    if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
        Invoke-Checked -Program "python" -Arguments @(
            "-m", "venv", $venvRoot)
    }
    Invoke-Checked -Program $python -Arguments @(
        "-m", "pip", "install", "--only-binary=:all:",
        "pyinstaller>=6.21,<7",
        "wxPython>=4.2.5,<5",
        "certifi>=2026.7.22",
        "cryptography>=49,<50",
        "pysubs2>=1.8.1,<2",
        "PyYAML>=6.0.3,<7",
        "requests>=2.34.2,<3",
        "pybind11>=3.0.4,<4",
        "scikit-build-core>=1.0.3,<2")

    Invoke-Checked -Program $vcpkg -Arguments @(
        "install",
        "--x-manifest-root=$(Join-Path $repoRoot 'tools\full-portable')",
        "--x-install-root=$vcpkgInstalled",
        "--triplet=x64-windows")

    if (-not (Test-Path -LiteralPath $pocketsphinxSource -PathType Container)) {
        Invoke-Checked -Program "git" -Arguments @(
            "clone", "--depth", "1", "--branch", "v5.1.1",
            "https://github.com/cmusphinx/pocketsphinx.git",
            $pocketsphinxSource)
    }

    Invoke-Checked -Program $cmake -Arguments @(
        "--fresh",
        "-S", $pocketsphinxSource,
        "-B", $pocketsphinxBuild,
        "-G", $Generator,
        "-A", "x64",
        "-DBUILD_SHARED_LIBS=ON",
        "-DBUILD_TESTING=OFF",
        "-DCMAKE_INSTALL_PREFIX=$($prefix.Replace('\', '/'))")
    Invoke-Checked -Program $cmake -Arguments @(
        "--build", $pocketsphinxBuild,
        "--config", "Release", "--target", "install")
}

if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
    throw "Portable Python environment is missing at $python."
}

$pkgconf = Join-Path $vcpkgInstalled "x64-windows\tools\pkgconf\pkgconf.exe"
if (-not (Test-Path -LiteralPath $pkgconf -PathType Leaf)) {
    throw "pkgconf is missing at $pkgconf."
}

$pybind11Dir = (& $python -m pybind11 --cmakedir).Trim()
if ($LASTEXITCODE -ne 0 -or -not $pybind11Dir) {
    throw "Could not locate pybind11's CMake package."
}

$savedPkgConfigPath = $env:PKG_CONFIG_PATH
$savedPath = $env:PATH
try {
    $env:PKG_CONFIG_PATH = @(
        (Join-Path $vcpkgInstalled "x64-windows\lib\pkgconfig"),
        (Join-Path $prefix "lib\pkgconfig")
    ) -join ";"
    $env:PATH = @(
        (Join-Path $vcpkgInstalled "x64-windows\bin"),
        (Join-Path $prefix "bin"),
        $savedPath
    ) -join ";"

    if (-not $SkipNativeBuild) {
        Invoke-Checked -Program $cmake -Arguments @(
            "--fresh",
            "-S", $repoRoot,
            "-B", $appBuild,
            "-G", $Generator,
            "-A", "x64",
            "-T", "cuda=$CudaToolkitPath",
            "-DSUBSYNC_BUILD_APP=ON",
            "-DSUBSYNC_BUILD_MATCHER_BENCHMARK=ON",
            "-DSUBSYNC_ENABLE_CUDA=ON",
            "-DSUBSYNC_ENABLE_OPENCL=ON",
            "-DCMAKE_CUDA_ARCHITECTURES=$CudaArchitectures",
            "-DCMAKE_CUDA_RUNTIME_LIBRARY=Static",
            "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain",
            "-DVCPKG_MANIFEST_MODE=OFF",
            "-DVCPKG_INSTALLED_DIR=$vcpkgInstalled",
            "-DVCPKG_TARGET_TRIPLET=x64-windows",
            "-DPython_EXECUTABLE=$python",
            "-Dpybind11_DIR=$pybind11Dir",
            "-DPKG_CONFIG_EXECUTABLE=$pkgconf",
            "-DCMAKE_PREFIX_PATH=$prefix")
        Invoke-Checked -Program $cmake -Arguments @(
            "--build", $appBuild, "--config", "Release")
    }

    $nativeDir = Join-Path $appBuild "Release"
    $env:PATH = "$nativeDir;$env:PATH"
    $frozenRoot = Join-Path $buildRoot "frozen"
    $pyinstallerWork = Join-Path $buildRoot "pyinstaller"
    Invoke-Checked -Program $python -Arguments @(
        "-s", "-m", "PyInstaller",
        "--noconfirm",
        "--clean",
        "--distpath=$frozenRoot",
        "--workpath=$pyinstallerWork",
        (Join-Path $repoRoot "tools\full-portable\windows-portable.spec"))
} finally {
    $env:PKG_CONFIG_PATH = $savedPkgConfigPath
    $env:PATH = $savedPath
}

$stagePath = Join-Path $buildRoot "frozen\subsync-portable"
if (-not (Test-Path -LiteralPath (Join-Path $stagePath "subsync.exe"))) {
    throw "Frozen GUI executable was not created."
}
if (-not (Test-Path -LiteralPath (Join-Path $stagePath "subsync-cmd.exe"))) {
    throw "Frozen CLI executable was not created."
}

Copy-Item -LiteralPath (Join-Path $repoRoot "tools\full-portable\README.txt") `
    -Destination $stagePath -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") `
    -Destination $stagePath -Force

$licenses = Join-Path $stagePath "licenses"
New-Item -ItemType Directory -Path $licenses -Force | Out-Null
$pythonLicense = (& $python -c `
    "import pathlib, sys; print(pathlib.Path(sys.base_prefix) / 'LICENSE.txt')").Trim()
if (Test-Path -LiteralPath $pythonLicense) {
    Copy-Item -LiteralPath $pythonLicense `
        -Destination (Join-Path $licenses "Python.txt") -Force
}
$pythonPackageLicenses = Join-Path $licenses "PythonPackages"
Get-ChildItem -Path (Join-Path $venvRoot "Lib\site-packages\*.dist-info\licenses") `
        -Directory -ErrorAction SilentlyContinue | ForEach-Object {
    $distribution = Split-Path -Leaf (Split-Path -Parent $_.FullName)
    Copy-Item -LiteralPath $_.FullName `
        -Destination (Join-Path $pythonPackageLicenses $distribution) `
        -Recurse -Force
}
$pocketsphinxLicense = Join-Path $pocketsphinxSource "LICENSE"
if (Test-Path -LiteralPath $pocketsphinxLicense) {
    Copy-Item -LiteralPath $pocketsphinxLicense `
        -Destination (Join-Path $licenses "PocketSphinx.txt") -Force
}
$ffmpegLicense = Join-Path $vcpkgInstalled "x64-windows\share\ffmpeg\copyright"
if (Test-Path -LiteralPath $ffmpegLicense) {
    Copy-Item -LiteralPath $ffmpegLicense `
        -Destination (Join-Path $licenses "FFmpeg.txt") -Force
}
$openClLicense = Join-Path $vcpkgInstalled "x64-windows\share\opencl\copyright"
if (Test-Path -LiteralPath $openClLicense) {
    Copy-Item -LiteralPath $openClLicense `
        -Destination (Join-Path $licenses "OpenCL.txt") -Force
}
$iconvLicense = Join-Path $vcpkgInstalled "x64-windows\share\libiconv\copyright"
if (Test-Path -LiteralPath $iconvLicense) {
    Copy-Item -LiteralPath $iconvLicense `
        -Destination (Join-Path $licenses "Libiconv.txt") -Force
}

# Include a ready-to-use English model so an extracted package can perform an
# offline end-to-end synchronization without first downloading an asset.
$speechAssets = Join-Path $stagePath "assets\speech"
$englishModelSource = Join-Path $prefix "share\pocketsphinx\model\en-us"
$englishModelTarget = Join-Path $speechAssets "eng"
New-Item -ItemType Directory -Path $speechAssets -Force | Out-Null
if (Test-Path -LiteralPath $englishModelTarget) {
    Remove-Item -LiteralPath $englishModelTarget -Recurse -Force
}
Copy-Item -LiteralPath $englishModelSource -Destination $englishModelTarget `
    -Recurse
$englishDescriptor = [ordered]@{
    version = "5.1.1"
    dir = "./eng"
    samplerate = 16000
    sampleformat = "S16"
    sphinx = [ordered]@{
        hmm = "./eng/en-us"
        lm = "./eng/en-us.lm.bin"
        dict = "./eng/cmudict-en-us.dict"
    }
}
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$englishJson = $englishDescriptor | ConvertTo-Json -Depth 4
[IO.File]::WriteAllText(
    (Join-Path $speechAssets "eng.speech"), $englishJson, $utf8NoBom)

$benchmark = Join-Path $appBuild "Release\subsync-matcher-benchmark.exe"
if (Test-Path -LiteralPath $benchmark) {
    $toolsDir = Join-Path $stagePath "tools"
    New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null
    Copy-Item -LiteralPath $benchmark -Destination $toolsDir -Force
}

$versionMatch = Select-String `
    -LiteralPath (Join-Path $repoRoot "subsync\version.py") `
    -Pattern '^version = "([^"]+)"' |
    Select-Object -First 1
$version = $versionMatch.Matches[0].Groups[1].Value
$manifestPath = Join-Path $stagePath "manifest.json"
$manifest = [ordered]@{
    package = "subsync-$version-portable-win-x64"
    createdUtc = [DateTime]::UtcNow.ToString("o")
    python = (& $python -c "import platform; print(platform.python_version())")
    pocketsphinx = "5.1.1"
    ffmpeg = "6.1.1"
    cudaToolkit = Split-Path -Leaf $CudaToolkitPath
    cudaArchitectures = $CudaArchitectures.Split(";")
}
$manifest | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $manifestPath -Encoding UTF8

$packageName = "subsync-$version-portable-win-x64"
New-Item -ItemType Directory -Path $distRoot -Force | Out-Null
$zipPath = Join-Path $distRoot "$packageName.zip"
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath $stagePath -DestinationPath $zipPath `
    -CompressionLevel Optimal

Write-Host "Portable folder: $stagePath"
Write-Host "Portable ZIP:    $zipPath"
