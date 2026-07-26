[CmdletBinding()]
param(
    [ValidateSet("quick", "full")]
    [string]$Mode = "quick"
)

$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$benchmark = Join-Path $root "subsync-matcher-benchmark.exe"
$resultsDir = Join-Path $root "results"
New-Item -ItemType Directory -Path $resultsDir -Force | Out-Null

$safeComputerName = $env:COMPUTERNAME -replace '[^A-Za-z0-9_.-]', '_'
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resultPath = Join-Path $resultsDir "$safeComputerName-$timestamp.txt"

$report = [Collections.Generic.List[string]]::new()
function Add-Line([object]$Value = "") {
    $report.Add([string]$Value)
}
function Add-Section([string]$Name) {
    Add-Line
    Add-Line "=== $Name ==="
}

Add-Line "SubSync GPU pocket machine report"
Add-Line "Created: $([DateTime]::Now.ToString('o'))"
Add-Line "Mode: $Mode"

Add-Section "Operating system"
try {
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    Add-Line "$($os.Caption) $($os.Version) build $($os.BuildNumber)"
    Add-Line "Architecture: $($os.OSArchitecture)"
} catch {
    Add-Line ([Environment]::OSVersion.VersionString)
    Add-Line "Architecture: $env:PROCESSOR_ARCHITECTURE"
    Add-Line "Detailed OS query unavailable: $($_.Exception.Message)"
}

Add-Section "Processor and memory"
try {
    Get-CimInstance Win32_Processor -ErrorAction Stop | ForEach-Object {
        Add-Line ("$($_.Name.Trim()) | $($_.NumberOfCores) cores / " +
            "$($_.NumberOfLogicalProcessors) logical processors")
    }
    $system = Get-CimInstance Win32_ComputerSystem -ErrorAction Stop
    Add-Line ("Memory: {0:N1} GiB" -f ($system.TotalPhysicalMemory / 1GB))
} catch {
    Add-Line "$env:PROCESSOR_IDENTIFIER | $env:NUMBER_OF_PROCESSORS logical processors"
    Add-Line "Detailed CPU query unavailable: $($_.Exception.Message)"
}

Add-Section "Display adapters"
try {
    Get-CimInstance Win32_VideoController -ErrorAction Stop | ForEach-Object {
        Add-Line "$($_.Name) | driver $($_.DriverVersion)"
    }
} catch {
    Add-Line "Detailed display query unavailable: $($_.Exception.Message)"
}

Add-Section "NVIDIA driver"
$nvidiaSmi = Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
if ($nvidiaSmi) {
    $nvidiaOutput = & $nvidiaSmi.Source `
        --query-gpu=name,driver_version,compute_cap `
        --format=csv,noheader 2>&1
    foreach ($line in $nvidiaOutput) {
        Add-Line $line
    }
} else {
    Add-Line "nvidia-smi.exe was not found."
}

Add-Section "Matcher benchmark"
if (-not (Test-Path -LiteralPath $benchmark -PathType Leaf)) {
    Add-Line "Missing executable: $benchmark"
    $exitCode = 3
} else {
    $arguments = if ($Mode -eq "quick") {
        @("--batch", "65536", "--iterations", "20")
    } else {
        @("--iterations", "50")
    }
    $benchmarkOutput = & $benchmark @arguments 2>&1
    $exitCode = $LASTEXITCODE
    foreach ($line in $benchmarkOutput) {
        Add-Line $line
    }
    Add-Line "Benchmark exit code: $exitCode"
}

$report | Set-Content -LiteralPath $resultPath -Encoding UTF8
$report | ForEach-Object { Write-Host $_ }
Write-Host
Write-Host "Report saved to: $resultPath"
exit $exitCode
