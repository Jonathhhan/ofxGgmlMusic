param(
	[string]$Repo = "https://github.com/ServeurpersoCom/acestep.cpp.git",
	[string]$Revision = "",
	[string]$Configuration = "Release",
	[string]$Generator = "",
	[int]$Jobs = 0,
	[string]$SourceDir = "",
	[string]$BuildDir = "",
	[string]$InstallDir = "",
	[switch]$Auto,
	[switch]$CpuOnly,
	[switch]$Cuda,
	[switch]$Vulkan,
	[switch]$Metal,
	[switch]$Blas,
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

function Test-ChildPath {
	param(
		[string]$Parent,
		[string]$Child
	)
	$resolvedParent = (Resolve-PathSafe $Parent).TrimEnd(
		[System.IO.Path]::DirectorySeparatorChar,
		[System.IO.Path]::AltDirectorySeparatorChar)
	$resolvedChild = Resolve-PathSafe $Child
	$prefix = $resolvedParent + [System.IO.Path]::DirectorySeparatorChar
	return $resolvedChild.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-RevisionLabel {
	param([string]$Revision)
	if ([string]::IsNullOrWhiteSpace($Revision)) {
		return "remote default"
	}
	return $Revision
}

function Get-GitCloneArgs {
	param(
		[string]$Repo,
		[string]$Revision,
		[string]$SourceDir
	)

	$args = @("clone", "--recurse-submodules", "--depth", "1")
	if (![string]::IsNullOrWhiteSpace($Revision)) {
		$args += "--branch"
		$args += $Revision
	}
	$args += $Repo
	$args += $SourceDir
	return $args
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

function New-CmakeArgs {
	param(
		[string]$SourceDir,
		[string]$BuildDir,
		[string]$Configuration,
		[string]$Generator,
		[bool]$EnableCpu,
		[bool]$EnableCuda,
		[bool]$EnableVulkan,
		[bool]$EnableMetal,
		[bool]$EnableBlas
	)

	$args = @(
		"-S", $SourceDir,
		"-B", $BuildDir,
		"-DCMAKE_BUILD_TYPE=$Configuration",
		"-DGGML_BLAS=$(Convert-ToOnOff $EnableBlas)",
		"-DGGML_CUDA=$(Convert-ToOnOff $EnableCuda)",
		"-DGGML_VULKAN=$(Convert-ToOnOff $EnableVulkan)",
		"-DGGML_METAL=$(Convert-ToOnOff $EnableMetal)"
	)
	if ($EnableCpu -and ($EnableCuda -or $EnableVulkan -or $EnableMetal)) {
		$args += "-DGGML_CPU_ALL_VARIANTS=ON"
		$args += "-DGGML_BACKEND_DL=ON"
	}
	$generatorArgs = @()
	if (![string]::IsNullOrWhiteSpace($Generator)) {
		$generatorArgs += @("-G", $Generator)
	}
	if ((Test-WindowsHost) -and $EnableCuda -and $env:CUDA_PATH -and
		([string]::IsNullOrWhiteSpace($Generator) -or $Generator -match "Visual Studio")) {
		$generatorArgs += @("-A", "x64", "-T", "host=x64,cuda=$env:CUDA_PATH")
	}
	if ($generatorArgs.Count -gt 0) {
		$args = @($generatorArgs + $args)
	}
	return $args
}

function Clear-CmakeConfigureCache {
	param([string]$BuildDir)
	$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
	$cacheDir = Join-Path $BuildDir "CMakeFiles"
	if (Test-Path -LiteralPath $cacheFile -PathType Leaf) {
		Remove-Item -LiteralPath $cacheFile -Force
	}
	if (Test-Path -LiteralPath $cacheDir -PathType Container) {
		Remove-Item -LiteralPath $cacheDir -Recurse -Force
	}
}

function Test-GgmlSourceHasAceStepOps {
	param([string]$GgmlSource)
	$ggmlHeader = Join-Path $GgmlSource "include\ggml.h"
	if (!(Test-Path -LiteralPath $ggmlHeader -PathType Leaf)) {
		return $false
	}
	$headerText = Get-Content -LiteralPath $ggmlHeader -Raw
	return $headerText -match "ggml_col2im_1d"
}

function Assert-GgmlSourceHasAceStepOps {
	param(
		[string]$GgmlSource,
		[string]$Label
	)
	if (!(Test-GgmlSourceHasAceStepOps $GgmlSource)) {
		throw "$Label does not expose the ACE-Step patched ggml_col2im_1d op. Refresh the acestep.cpp source and its ggml submodule."
	}
}

function Read-CmakeCacheValue {
	param(
		[string]$BuildDir,
		[string]$Name
	)
	$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
	if (!(Test-Path -LiteralPath $cacheFile -PathType Leaf)) {
		return ""
	}
	$pattern = "^{0}:[^=]*=(.*)$" -f [regex]::Escape($Name)
	foreach ($line in Get-Content -LiteralPath $cacheFile) {
		$match = [regex]::Match($line, $pattern)
		if ($match.Success) {
			return $match.Groups[1].Value.Trim()
		}
	}
	return ""
}

function Test-CmakeCacheBoolOn {
	param(
		[string]$BuildDir,
		[string]$Name
	)
	$value = Read-CmakeCacheValue -BuildDir $BuildDir -Name $Name
	return $value -match "^(ON|TRUE|1|YES)$"
}

function Get-InstalledBackendFiles {
	param(
		[string]$InstallBin,
		[string[]]$Patterns
	)
	if (!(Test-Path -LiteralPath $InstallBin -PathType Container)) {
		return @()
	}
	$files = @()
	foreach ($pattern in $Patterns) {
		$files += @(Get-ChildItem -LiteralPath $InstallBin -File -Filter $pattern -ErrorAction SilentlyContinue)
	}
	return @($files | Sort-Object -Property Name -Unique)
}

function Join-BackendFileNames {
	param([object[]]$Files)
	if ($Files.Count -eq 0) {
		return "none"
	}
	return (($Files | ForEach-Object { $_.Name }) -join ", ")
}

function Get-AceStepBackendReport {
	param(
		[string]$BuildDir,
		[string]$InstallBin
	)
	$cudaFiles = Get-InstalledBackendFiles -InstallBin $InstallBin -Patterns @(
		"ggml-cuda*.dll", "ggml-cuda*.so", "libggml-cuda*.so", "libggml-cuda*.dylib")
	$vulkanFiles = Get-InstalledBackendFiles -InstallBin $InstallBin -Patterns @(
		"ggml-vulkan*.dll", "ggml-vulkan*.so", "libggml-vulkan*.so", "libggml-vulkan*.dylib")
	$metalFiles = Get-InstalledBackendFiles -InstallBin $InstallBin -Patterns @(
		"ggml-metal*.dll", "ggml-metal*.so", "libggml-metal*.so", "libggml-metal*.dylib")
	$cpuFiles = Get-InstalledBackendFiles -InstallBin $InstallBin -Patterns @(
		"ggml-cpu*.dll", "ggml-cpu*.so", "libggml-cpu*.so", "libggml-cpu*.dylib")

	return [pscustomobject]@{
		CudaConfigured = Test-CmakeCacheBoolOn -BuildDir $BuildDir -Name "GGML_CUDA"
		VulkanConfigured = Test-CmakeCacheBoolOn -BuildDir $BuildDir -Name "GGML_VULKAN"
		MetalConfigured = Test-CmakeCacheBoolOn -BuildDir $BuildDir -Name "GGML_METAL"
		BackendDlConfigured = Test-CmakeCacheBoolOn -BuildDir $BuildDir -Name "GGML_BACKEND_DL"
		CpuFiles = $cpuFiles
		CudaFiles = $cudaFiles
		VulkanFiles = $vulkanFiles
		MetalFiles = $metalFiles
	}
}

function Write-AceStepBackendReport {
	param([object]$Report)
	Write-Step "AceStep backend summary"
	Write-Host ("  CMakeCache: CUDA={0} Vulkan={1} Metal={2} BackendDL={3}" -f `
		(Convert-ToOnOff $Report.CudaConfigured),
		(Convert-ToOnOff $Report.VulkanConfigured),
		(Convert-ToOnOff $Report.MetalConfigured),
		(Convert-ToOnOff $Report.BackendDlConfigured))
	Write-Host "  installed CPU backend artifacts: $(Join-BackendFileNames $Report.CpuFiles)"
	Write-Host "  installed CUDA backend artifacts: $(Join-BackendFileNames $Report.CudaFiles)"
	Write-Host "  installed Vulkan backend artifacts: $(Join-BackendFileNames $Report.VulkanFiles)"
	Write-Host "  installed Metal backend artifacts: $(Join-BackendFileNames $Report.MetalFiles)"
}

function Assert-RequestedBackendInstalled {
	param(
		[object]$Report,
		[bool]$RequireCuda,
		[bool]$RequireVulkan,
		[bool]$RequireMetal
	)
	if ($RequireCuda) {
		if (!$Report.CudaConfigured) {
			throw "CUDA was requested but final CMakeCache.txt reports GGML_CUDA=OFF. Re-run with -Clean -Cuda and inspect the CMake configure log."
		}
		if ($Report.CudaFiles.Count -eq 0) {
			throw "CUDA was requested but no ggml-cuda backend artifact was installed in libs/acestep/bin."
		}
	}
	if ($RequireVulkan) {
		if (!$Report.VulkanConfigured) {
			throw "Vulkan was requested but final CMakeCache.txt reports GGML_VULKAN=OFF."
		}
		if ($Report.VulkanFiles.Count -eq 0) {
			throw "Vulkan was requested but no ggml-vulkan backend artifact was installed in libs/acestep/bin."
		}
	}
	if ($RequireMetal) {
		if (!$Report.MetalConfigured) {
			throw "Metal was requested but final CMakeCache.txt reports GGML_METAL=OFF."
		}
		if ($Report.MetalFiles.Count -eq 0) {
			throw "Metal was requested but no ggml-metal backend artifact was installed in libs/acestep/bin."
		}
	}
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

function Install-RuntimeDlls {
	param(
		[string]$BuiltServer,
		[string]$InstallBin
	)
	$builtServerDir = [System.IO.Path]::GetDirectoryName($BuiltServer)
	$dlls = @(Get-ChildItem -LiteralPath $builtServerDir -File -Filter *.dll -ErrorAction SilentlyContinue)
	foreach ($dll in $dlls) {
		Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $InstallBin $dll.Name) -Force
	}
	if ($dlls.Count -gt 0) {
		Write-Step "Installed $($dlls.Count) runtime DLL(s) to $InstallBin"
	}
}

function Clear-InstalledRuntimeArtifacts {
	param([string]$InstallBin)
	if (!(Test-Path -LiteralPath $InstallBin -PathType Container)) {
		return
	}
	$patterns = @("ace-server", "ace-server.exe", "ggml*.dll", "ggml*.so", "libggml*.so", "libggml*.dylib")
	foreach ($pattern in $patterns) {
		foreach ($file in @(Get-ChildItem -LiteralPath $InstallBin -File -Filter $pattern -ErrorAction SilentlyContinue)) {
			try {
				Remove-Item -LiteralPath $file.FullName -Force -ErrorAction Stop
			} catch {
				throw "Could not replace installed AceStep runtime artifact '$($file.FullName)'. Stop any running ace-server process and retry. $($_.Exception.Message)"
			}
		}
	}
}

function Remove-ReparsePointDirectory {
	param([string]$Path)
	if (!(Test-ChildPath -Parent (Split-Path -Parent $Path) -Child $Path)) {
		throw "Refusing to remove unexpected reparse point: $Path"
	}
	[System.IO.Directory]::Delete($Path)
}

function Ensure-AcestepSource {
	param([string]$SourceDir)
	if (Test-Path -LiteralPath $SourceDir) {
		$gitDir = Join-Path $SourceDir ".git"
		$cmakeFile = Join-Path $SourceDir "CMakeLists.txt"
		if (!(Test-Path -LiteralPath $gitDir -PathType Container) -or
			!(Test-Path -LiteralPath $cmakeFile -PathType Leaf)) {
			throw "acestep.cpp source directory exists but does not look like a complete clone: $SourceDir"
		}
		Write-Step "acestep.cpp source already exists; skipping clone."
		return
	}
	$revisionLabel = Get-RevisionLabel $Revision
	Write-Step "Cloning acestep.cpp ($revisionLabel)"
	$cloneArgs = Get-GitCloneArgs -Repo $Repo -Revision $Revision -SourceDir $SourceDir
	& "git" @cloneArgs
	if ($LASTEXITCODE -ne 0) {
		$hint = if ([string]::IsNullOrWhiteSpace($Revision)) {
			""
		} else {
			" Verify that '$Revision' is a branch or tag in $Repo, or omit -Revision to use the remote default branch."
		}
		throw "git clone failed with exit code $LASTEXITCODE.$hint"
	}
}

function Ensure-AceStepGgmlSource {
	param([string]$SourceDir)
	$targetPath = Join-Path $SourceDir "ggml"
	$ggmlHeader = Join-Path $targetPath "include\ggml.h"

	if (Test-Path -LiteralPath $targetPath) {
		$item = Get-Item -LiteralPath $targetPath -Force
		if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
			Write-Step "Restoring bundled acestep.cpp ggml submodule"
			Remove-ReparsePointDirectory $targetPath
		}
	}

	if (!(Test-Path -LiteralPath $ggmlHeader -PathType Leaf)) {
		Write-Step "Updating bundled acestep.cpp ggml submodule"
		& "git" -C $SourceDir submodule update --init --depth 1 "ggml"
		if ($LASTEXITCODE -ne 0) {
			throw "git submodule update for acestep.cpp ggml failed with exit code $LASTEXITCODE"
		}
	}
	Assert-GgmlSourceHasAceStepOps -GgmlSource $targetPath -Label "bundled acestep.cpp ggml source"
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Split-Path -Parent $scriptRoot
$runtimeRoot = Join-Path $addonRoot "libs\acestep"

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
$enableBlas = $Blas -or ($env:OFXGGML_ACESTEP_BLAS -and $env:OFXGGML_ACESTEP_BLAS -ne "0")
$cudaAvailable = Test-CudaAvailable
$vulkanAvailable = Test-VulkanAvailable
$metalAvailable = Test-MetalAvailable

if ($CpuOnly) {
	$enableCuda = $false
	$enableVulkan = $false
	$enableMetal = $false
} elseif ($Auto -or (!$explicitBackend -and !$CpuOnly)) {
	$enableCuda = $cudaAvailable
	$enableVulkan = $vulkanAvailable
	$enableMetal = $metalAvailable
} else {
	$enableCuda = [bool]$Cuda
	$enableVulkan = [bool]$Vulkan
	$enableMetal = [bool]$Metal
}

if (!$DryRun -and $Cuda -and !$cudaAvailable) {
	throw "CUDA was requested but CUDA toolkit was not found."
}
if (!$DryRun -and $Vulkan -and !$vulkanAvailable) {
	throw "Vulkan was requested but Vulkan SDK/tools were not found."
}
if (!$DryRun -and $Metal -and !$metalAvailable) {
	throw "Metal was requested but this host is not macOS or xcrun is unavailable."
}

$resolvedGenerator = Get-DefaultGenerator $Generator
$cmakeArgs = New-CmakeArgs `
	-SourceDir $SourceDir `
	-BuildDir $BuildDir `
	-Configuration $Configuration `
	-Generator $resolvedGenerator `
	-EnableCpu $enableCpu `
	-EnableCuda $enableCuda `
	-EnableVulkan $enableVulkan `
	-EnableMetal $enableMetal `
	-EnableBlas $enableBlas

if ($DryRun) {
	$cloneArgs = Get-GitCloneArgs -Repo $Repo -Revision $Revision -SourceDir $SourceDir
	Write-Step "Dry run: acestep.cpp server setup plan"
	Write-Host "  repo: $Repo"
	Write-Host "  revision: $(Get-RevisionLabel $Revision)"
	Write-Host "  root: $runtimeRoot"
	Write-Host "  source: $SourceDir"
	Write-Host "  build: $BuildDir"
	Write-Host "  install: $InstallDir"
	Write-Host "  mode: $mode"
	Write-Host ("  enabled backends: CPU={0} CUDA={1} Vulkan={2} Metal={3}" -f `
		(Convert-ToOnOff $enableCpu), (Convert-ToOnOff $enableCuda), (Convert-ToOnOff $enableVulkan), (Convert-ToOnOff $enableMetal))
	Write-Host "  external BLAS: $(Convert-ToOnOff $enableBlas)"
	if ($Auto -and ($enableCuda -or $enableVulkan -or $enableMetal)) {
		Write-Host "  auto fallback: retry CPU-only if optional GPU configure fails"
	}
	Write-Host "  ggml: bundled ACE-Step fork"
	Write-Host "  ggml owner: ofxGgmlMusic"
	Write-Host "  jobs: $Jobs"
	Write-Host "git $($cloneArgs -join ' ')"
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

Ensure-AceStepGgmlSource -SourceDir $SourceDir

Write-Step "Configuring acestep.cpp"
$usedCpuOnlyFallback = $false
& "cmake" @cmakeArgs
$configureExitCode = $LASTEXITCODE
if ($configureExitCode -ne 0 -and $Auto -and !$CpuOnly -and ($enableCuda -or $enableVulkan -or $enableMetal)) {
	Write-Warning "Optional GPU backend configure failed; retrying AceStep setup as CPU-only. Pass -Cuda, -Vulkan, or -Metal to make that backend strict."
	$usedCpuOnlyFallback = $true
	$enableCuda = $false
	$enableVulkan = $false
	$enableMetal = $false
	$cmakeArgs = New-CmakeArgs `
		-SourceDir $SourceDir `
		-BuildDir $BuildDir `
		-Configuration $Configuration `
		-Generator $resolvedGenerator `
		-EnableCpu $enableCpu `
		-EnableCuda $enableCuda `
		-EnableVulkan $enableVulkan `
		-EnableMetal $enableMetal `
		-EnableBlas $enableBlas
	Clear-CmakeConfigureCache $BuildDir
	Write-Step "Configuring acestep.cpp (CPU-only fallback)"
	& "cmake" @cmakeArgs
	$configureExitCode = $LASTEXITCODE
}
if ($configureExitCode -ne 0) {
	throw "cmake configure failed with exit code $configureExitCode"
}

Write-Step "Building ace-server"
$buildArgs = @(
	"--build", $BuildDir,
	"--config", $Configuration,
	"--target", "ace-server",
	"--parallel", ([string]$Jobs)
)
& "cmake" @buildArgs
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
Clear-InstalledRuntimeArtifacts -InstallBin $installBin
Copy-Item -LiteralPath $builtServer -Destination $installedServer -Force
Write-Step "Installed ace-server to $installedServer"
Install-RuntimeDlls -BuiltServer $builtServer -InstallBin $installBin

$backendReport = Get-AceStepBackendReport -BuildDir $BuildDir -InstallBin $installBin
Write-AceStepBackendReport $backendReport
Assert-RequestedBackendInstalled `
	-Report $backendReport `
	-RequireCuda ([bool]$Cuda) `
	-RequireVulkan ([bool]$Vulkan) `
	-RequireMetal ([bool]$Metal)
if ($usedCpuOnlyFallback -or ($Auto -and !$backendReport.CudaConfigured -and !$backendReport.VulkanConfigured -and !$backendReport.MetalConfigured)) {
	Write-Warning "AceStep setup completed as CPU-only fallback. Use -Clean -Cuda to require CUDA and fail instead of falling back."
}

Write-Step "Done. To start the server, run:"
Write-Host "scripts\start-acestep-server.ps1 -ServerExecutable $installedServer -ModelPath <models folder>"
