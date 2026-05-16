param(
	[string]$Repo = "https://github.com/ServeurpersoCom/acestep.cpp.git",
	[string]$Revision = "main",
	[string]$Configuration = "Release",
	[string]$Generator = "",
	[int]$Jobs = 0,
	[string]$SourceDir = "",
	[string]$BuildDir = "",
	[string]$InstallDir = "",
	[string]$OfxGgmlCorePath = "",
	[switch]$Auto,
	[switch]$CpuOnly,
	[switch]$Cuda,
	[switch]$Vulkan,
	[switch]$Metal,
	[switch]$BundledGgml,
	[switch]$Clean,
	[switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Get-CommandPathOrNull {
	param([string]$Name)
	try {
		return (Get-Command $Name -ErrorAction Stop).Source
	} catch {
		return $null
	}
}

function Convert-ToOnOff {
	param([bool]$Value)
	if ($Value) { return "ON" }
	return "OFF"
}

function Test-WindowsHost {
	return !($IsLinux -or $IsMacOS)
}

function Get-DefaultGenerator {
	param([string]$Generator)
	if (![string]::IsNullOrWhiteSpace($Generator)) {
		return $Generator
	}
	return ""
}

function Resolve-PathSafe {
	param([string]$Path)
	return [System.IO.Path]::GetFullPath($Path)
}

function Test-CudaAvailable {
	if ($env:CUDA_PATH -and (Test-Path -LiteralPath $env:CUDA_PATH)) {
		return $true
	}
	return [bool](Get-CommandPathOrNull "nvcc")
}

function Test-VulkanAvailable {
	if ($env:VULKAN_SDK -and (Test-Path -LiteralPath $env:VULKAN_SDK)) {
		return $true
	}
	return [bool](Get-CommandPathOrNull "vulkaninfo") -or [bool](Get-CommandPathOrNull "glslc")
}

function Test-MetalAvailable {
	return $IsMacOS -and [bool](Get-CommandPathOrNull "xcrun")
}

function Test-CoreGgmlSourceAvailable {
	param([string]$CorePath)
	$ggmlSource = Join-Path $CorePath "libs\ggml\.source"
	if (!(Test-Path -LiteralPath $ggmlSource -PathType Container)) {
		return $false
	}
	$includeDir = Join-Path $ggmlSource "include"
	return Test-Path -LiteralPath (Join-Path $includeDir "ggml.h") -PathType Leaf
}

function Resolve-GeneratedServer {
	param([string]$BuildDir)
	$candidates = @(
		(Join-Path $BuildDir "ace-server"),
		(Join-Path $BuildDir "ace-server.exe"),
		(Join-Path $BuildDir "Release\ace-server"),
		(Join-Path $BuildDir "Release\ace-server.exe"),
		(Join-Path $BuildDir "Debug\ace-server"),
		(Join-Path $BuildDir "Debug\ace-server.exe"),
		(Join-Path $BuildDir "RelWithDebInfo\ace-server"),
		(Join-Path $BuildDir "RelWithDebInfo\ace-server.exe")
	)
	foreach ($candidate in $candidates) {
		if (Test-Path -LiteralPath $candidate -PathType Leaf) {
			return (Resolve-PathSafe $candidate)
		}
	}
	return ""
}

function Ensure-AcestepSource {
	param([string]$SourceDir)
	if (Test-Path -LiteralPath $SourceDir) {
		Write-Step "acestep.cpp source already exists; skipping clone."
		return
	}
	Write-Step "Cloning acestep.cpp"
	& "git" clone "--recurse-submodules" "--depth" "1" "--branch" $Revision $Repo $SourceDir
	if ($LASTEXITCODE -ne 0) {
		throw "git clone failed with exit code $LASTEXITCODE"
	}
}

function Ensure-GgmlSourceFromCore {
	param(
		[string]$SourceDir,
		[string]$CorePath
	)
	$coreSource = Join-Path $CorePath "libs\ggml\.source"
	$targetPath = Join-Path $SourceDir "ggml"

	if (!(Test-Path -LiteralPath $coreSource -PathType Container)) {
		throw "ofxGgmlCore ggml source was not found at: $coreSource"
	}

	if (Test-Path -LiteralPath $targetPath) {
		$item = Get-Item -LiteralPath $targetPath -Force
		if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
			Remove-Item -LiteralPath $targetPath -Force
		} elseif ($item.PSIsContainer -and ($item.GetFiles().Count -gt 0 -or $item.GetDirectories().Count -gt 0)) {
			throw "acestep.cpp already has a non-empty ggml directory at $targetPath. Remove it before rebuilding with ofxGgmlCore source."
		} else {
			Remove-Item -LiteralPath $targetPath -Recurse -Force
		}
	}

	if (Test-WindowsHost) {
		New-Item -ItemType Junction -Path $targetPath -Target $coreSource | Out-Null
	} else {
		New-Item -ItemType SymbolicLink -Path $targetPath -Target $coreSource | Out-Null
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Split-Path -Parent $scriptRoot
$runtimeRoot = Join-Path $addonRoot "lib\acestep"

if ([string]::IsNullOrWhiteSpace($SourceDir)) {
	$SourceDir = Join-Path $runtimeRoot ".source"
}
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
	$BuildDir = Join-Path $runtimeRoot "build"
}
if ([string]::IsNullOrWhiteSpace($InstallDir)) {
	$InstallDir = $runtimeRoot
}
$SourceDir = Resolve-PathSafe $SourceDir
$BuildDir = Resolve-PathSafe $BuildDir
$InstallDir = Resolve-PathSafe $InstallDir
if ([string]::IsNullOrWhiteSpace($OfxGgmlCorePath)) {
	$OfxGgmlCorePath = [System.IO.Path]::GetFullPath((Join-Path $addonRoot "..\ofxGgmlCore"))
} else {
	$OfxGgmlCorePath = Resolve-PathSafe $OfxGgmlCorePath
}
if ($Jobs -le 0) {
	$Jobs = [Math]::Max(1, [Environment]::ProcessorCount)
}

$explicitBackend = $Cuda -or $Vulkan -or $Metal
if (!$explicitBackend -and !$CpuOnly) {
	$Auto = $true
}

$mode = if ($CpuOnly) { "CpuOnly" } else { if ($Auto) { "Auto" } else { "Explicit" } }
$enableCpu = $true
$enableCuda = $false
$enableVulkan = $false
$enableMetal = $false

if ($CpuOnly) {
	$enableCuda = $false
	$enableVulkan = $false
	$enableMetal = $false
} elseif ($Auto -or (!$explicitBackend -and !$CpuOnly)) {
	$enableCuda = Test-CudaAvailable
	$enableVulkan = Test-VulkanAvailable
	$enableMetal = Test-MetalAvailable
} else {
	$enableCuda = [bool]$Cuda
	$enableVulkan = [bool]$Vulkan
	$enableMetal = [bool]$Metal
}

if ($Cuda -and !$enableCuda) {
	throw "CUDA was requested but CUDA toolkit was not found."
}
if ($Vulkan -and !$enableVulkan) {
	throw "Vulkan was requested but Vulkan SDK/tools were not found."
}
if ($Metal -and !$enableMetal) {
	throw "Metal was requested but this host is not macOS or xcrun is unavailable."
}

$coreGgmlAvailable = Test-CoreGgmlSourceAvailable $OfxGgmlCorePath
$ggmlMode = if ($BundledGgml -and $coreGgmlAvailable) {
	"ofxGgmlCore"
} elseif ($BundledGgml -and !$coreGgmlAvailable) {
	"bundled"
} elseif ($coreGgmlAvailable) {
	"ofxGgmlCore"
} else {
	"bundled"
}

$ggmlModeDetail = if ($BundledGgml -and $coreGgmlAvailable) {
	"BundledGgml requested but ignored because ofxGgmlCore ggml source is available"
} elseif ($coreGgmlAvailable) {
	"using ofxGgmlCore .source/ggml"
} else {
	"using bundled ggml in acestep.cpp"
}

$cmakeArgs = @(
	"-S", $SourceDir,
	"-B", $BuildDir,
	"-DCMAKE_BUILD_TYPE=$Configuration",
	"-DGGML_BLAS=$(Convert-ToOnOff $enableCpu)",
	"-DGGML_CUDA=$(Convert-ToOnOff $enableCuda)",
	"-DGGML_VULKAN=$(Convert-ToOnOff $enableVulkan)",
	"-DGGML_METAL=$(Convert-ToOnOff $enableMetal)"
)
if ($enableCpu -and ($enableCuda -or $enableVulkan -or $enableMetal)) {
	$cmakeArgs += "-DGGML_CPU_ALL_VARIANTS=ON"
	$cmakeArgs += "-DGGML_BACKEND_DL=ON"
}

$resolvedGenerator = Get-DefaultGenerator $Generator
if (![string]::IsNullOrWhiteSpace($resolvedGenerator)) {
	$cmakeArgs = @(
		"-G",
		$resolvedGenerator,
		$cmakeArgs
	)
}

if ($DryRun) {
	Write-Step "Dry run: acestep.cpp server setup plan"
	Write-Host "  repo: $Repo"
	Write-Host "  revision: $Revision"
	Write-Host "  root: $runtimeRoot"
	Write-Host "  source: $SourceDir"
	Write-Host "  build: $BuildDir"
	Write-Host "  install: $InstallDir"
	Write-Host "  mode: $mode"
	Write-Host ("  enabled backends: CPU={0} CUDA={1} Vulkan={2} Metal={3}" -f `
		(Convert-ToOnOff $enableCpu), (Convert-ToOnOff $enableCuda), (Convert-ToOnOff $enableVulkan), (Convert-ToOnOff $enableMetal))
	Write-Host "  ggml: $(if ($ggmlMode -eq "ofxGgmlCore") { "ofxGgmlCore source" } else { "bundled" })"
	Write-Host "  ggml detail: $ggmlModeDetail"
	if ($ggmlMode -eq "ofxGgmlCore") {
		Write-Host "  ofxGgmlCore: $OfxGgmlCorePath"
	}
	Write-Host "  jobs: $Jobs"
	Write-Host "cmake $($cmakeArgs -join ' ')"
	Write-Host "cmake --build $BuildDir --config $Configuration --target ace-server --parallel $Jobs"
	Write-Host "  output: $(Join-Path $InstallDir "bin")\ace-server(.exe)"
	Write-Step "Dry run complete; no files were changed"
	return
}

foreach ($tool in @("git", "cmake")) {
	if (!(Get-CommandPathOrNull $tool)) {
		throw "$tool was not found on PATH."
	}
}

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
	Write-Step "Cleaning $BuildDir"
	Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

Ensure-AcestepSource $SourceDir

if ($ggmlMode -eq "ofxGgmlCore") {
	Ensure-GgmlSourceFromCore -SourceDir $SourceDir -CorePath $OfxGgmlCorePath
}

Write-Step "Configuring acestep.cpp"
& "cmake" @cmakeArgs
if ($LASTEXITCODE -ne 0) {
	throw "cmake configure failed with exit code $LASTEXITCODE"
}

Write-Step "Building ace-server"
& "cmake" "--build", $BuildDir, "--config", $Configuration, "--target", "ace-server", "--parallel", [string]$Jobs
if ($LASTEXITCODE -ne 0) {
	throw "cmake build failed with exit code $LASTEXITCODE"
}

$builtServer = Resolve-GeneratedServer $BuildDir
if ([string]::IsNullOrWhiteSpace($builtServer)) {
	throw "ace-server binary was not found in build tree: $BuildDir"
}
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
$installBin = Join-Path $InstallDir "bin"
New-Item -ItemType Directory -Force -Path $installBin | Out-Null
$installedServer = Join-Path $installBin ([System.IO.Path]::GetFileName($builtServer))
Copy-Item -LiteralPath $builtServer -Destination $installedServer -Force
Write-Step "Installed ace-server to $installedServer"

Write-Step "Done. To start the server, run:"
Write-Host "scripts\start-acestep-server.ps1 -ServerExecutable $installedServer -ModelPath <models folder>"
