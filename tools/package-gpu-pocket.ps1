[CmdletBinding()]
param(
    [string]$BuildDir = "build/gpu-pocket",
    [string]$DistDir = "dist",
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$CudaToolkitPath = $env:CUDA_PATH,
    [string]$Generator = "Visual Studio 16 2019",
    [string]$CudaArchitectures =
        "75-real;80-real;86-real;89-real;90-real;100-real;103-real;" +
        "110-real;120-real;121-real;75-virtual",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildPath = [IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$distPath = [IO.Path]::GetFullPath((Join-Path $repoRoot $DistDir))

function Find-FirstDirectory {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Container)) {
            return [IO.Path]::GetFullPath($candidate)
        }
    }
    return $null
}

if (-not $CudaToolkitPath) {
    $cudaRoot = Join-Path $env:ProgramFiles "NVIDIA GPU Computing Toolkit\CUDA"
    if (Test-Path -LiteralPath $cudaRoot) {
        $CudaToolkitPath = Get-ChildItem -LiteralPath $cudaRoot -Directory |
            Sort-Object Name -Descending |
            Select-Object -First 1 -ExpandProperty FullName
    }
}

if (-not $VcpkgRoot) {
    $VcpkgRoot = Find-FirstDirectory @(
        (Join-Path $env:USERPROFILE "dev\vcpkg"),
        (Join-Path $env:USERPROFILE "vcpkg"),
        "C:\dev\vcpkg"
    )
}

if (-not $CudaToolkitPath -or
        -not (Test-Path -LiteralPath (Join-Path $CudaToolkitPath "bin\nvcc.exe"))) {
    throw "CUDA toolkit not found. Pass -CudaToolkitPath or set CUDA_PATH."
}

$vcpkgToolchain = if ($VcpkgRoot) {
    Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
}
if (-not $vcpkgToolchain -or
        -not (Test-Path -LiteralPath $vcpkgToolchain -PathType Leaf)) {
    throw "vcpkg toolchain not found. Pass -VcpkgRoot or set VCPKG_ROOT."
}

$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmakePath = $cmakeCommand.Source
} else {
    $pythonCmake = "C:\Python313\Scripts\cmake.exe"
    if (Test-Path -LiteralPath $pythonCmake) {
        $cmakePath = $pythonCmake
    } else {
        throw "CMake was not found on PATH."
    }
}

if (-not $SkipBuild) {
    & $cmakePath --fresh `
        -S $repoRoot `
        -B $buildPath `
        -G $Generator `
        -A x64 `
        -T "cuda=$CudaToolkitPath" `
        -DSUBSYNC_BUILD_APP=OFF `
        -DSUBSYNC_BUILD_MATCHER_BENCHMARK=ON `
        -DSUBSYNC_ENABLE_CUDA=ON `
        -DSUBSYNC_ENABLE_OPENCL=ON `
        "-DCMAKE_CUDA_ARCHITECTURES=$CudaArchitectures" `
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
        -DCMAKE_CUDA_RUNTIME_LIBRARY=Static `
        "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain"
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE."
    }

    & $cmakePath --build $buildPath --config Release
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE."
    }
}

$versionMatch = Select-String -LiteralPath (Join-Path $repoRoot "pyproject.toml") `
    -Pattern '^version\s*=\s*"([^"]+)"' |
    Select-Object -First 1
$version = if ($versionMatch) {
    $versionMatch.Matches[0].Groups[1].Value
} else {
    "dev"
}

$packageName = "subsync-gpu-pocket-$version-win-x64"
$stageRoot = Join-Path $buildPath "package"
$stagePath = Join-Path $stageRoot $packageName
$resolvedStageRoot = [IO.Path]::GetFullPath($stageRoot)
if (-not $resolvedStageRoot.StartsWith(
        $buildPath.TrimEnd("\") + "\", [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to clean staging path outside the build directory."
}
if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $stagePath | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stagePath "licenses") | Out-Null

$releasePath = Join-Path $buildPath "Release"
$benchmark = Join-Path $releasePath "subsync-matcher-benchmark.exe"
if (-not (Test-Path -LiteralPath $benchmark -PathType Leaf)) {
    throw "Benchmark executable not found at $benchmark."
}

Copy-Item -LiteralPath $benchmark -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "gpu-pocket\README.txt") `
    -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "gpu-pocket\test-machine.ps1") `
    -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "gpu-pocket\test-this-machine.cmd") `
    -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "gpu-pocket\benchmark-full.cmd") `
    -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "gpu-pocket\benchmark-custom.cmd") `
    -Destination $stagePath

$openClDll = Join-Path $releasePath "OpenCL.dll"
if (-not (Test-Path -LiteralPath $openClDll -PathType Leaf)) {
    $openClDll = Join-Path $VcpkgRoot "installed\x64-windows\bin\OpenCL.dll"
}
if (-not (Test-Path -LiteralPath $openClDll -PathType Leaf)) {
    throw "OpenCL.dll was not copied by CMake and was not found in vcpkg."
}
Copy-Item -LiteralPath $openClDll -Destination $stagePath

$openClLicense = Join-Path $VcpkgRoot "installed\x64-windows\share\opencl\copyright"
if (Test-Path -LiteralPath $openClLicense -PathType Leaf) {
    Copy-Item -LiteralPath $openClLicense `
        -Destination (Join-Path $stagePath "licenses\OpenCL.txt")
}

# The benchmark itself uses the static MSVC runtime. The vcpkg OpenCL loader
# is dynamically linked to vcruntime140, so deploy that DLL beside the loader.
$vcRuntime = Join-Path $env:WINDIR "System32\vcruntime140.dll"
if (Test-Path -LiteralPath $vcRuntime -PathType Leaf) {
    Copy-Item -LiteralPath $vcRuntime -Destination $stagePath
}

$manifest = [ordered]@{
    package = $packageName
    createdUtc = [DateTime]::UtcNow.ToString("o")
    cudaToolkit = Split-Path -Leaf $CudaToolkitPath
    cudaArchitectures = $CudaArchitectures.Split(";")
    files = Get-ChildItem -LiteralPath $stagePath -File -Recurse | ForEach-Object {
        [ordered]@{
            path = $_.FullName.Substring($stagePath.Length + 1)
            bytes = $_.Length
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    }
}
$manifest | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $stagePath "manifest.json") -Encoding UTF8

New-Item -ItemType Directory -Path $distPath -Force | Out-Null
$zipPath = Join-Path $distPath "$packageName.zip"
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath $stagePath -DestinationPath $zipPath `
    -CompressionLevel Optimal

Write-Host "Portable folder: $stagePath"
Write-Host "Portable ZIP:    $zipPath"
