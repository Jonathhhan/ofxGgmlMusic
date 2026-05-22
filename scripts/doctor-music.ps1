param(
	[string]$Prompt = $(if ($env:OFXGGML_MUSIC_PROMPT) { $env:OFXGGML_MUSIC_PROMPT } else { "" }),
	[string]$Output = $(if ($env:OFXGGML_MUSIC_OUTPUT) { $env:OFXGGML_MUSIC_OUTPUT } else { "" }),
	[string]$ExternalExecutable = $(if ($env:OFXGGML_MUSIC_EXTERNAL_EXECUTABLE) { $env:OFXGGML_MUSIC_EXTERNAL_EXECUTABLE } else { "" }),
	[string]$Model = $(if ($env:OFXGGML_MUSIC_MODEL) { $env:OFXGGML_MUSIC_MODEL } else { "" }),
	[string]$Python = $(if ($env:OFXGGML_MUSIC_PYTHON) { $env:OFXGGML_MUSIC_PYTHON } else { "" }),
	[switch]$Json,
	[switch]$Strict
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$addonsRoot = Split-Path -Parent $addonRoot
$script:Warnings = 0

function New-Check {
	param(
		[string]$State,
		[string]$Name,
		[string]$Detail = ""
	)
	if ($State -eq "WARN") {
		$script:Warnings++
	}
	return [pscustomobject]@{
		State = $State
		Name = $Name
		Detail = $Detail
	}
}

function Test-CommandAvailable {
	param([string]$Name)
	return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Test-PathCheck {
	param(
		[string]$Path,
		[string]$Name,
		[string]$MissingDetail,
		[switch]$Directory
	)
	$exists = if ($Directory) {
		Test-Path -LiteralPath $Path -PathType Container
	} else {
		Test-Path -LiteralPath $Path -PathType Leaf
	}
	if ($exists) {
		return New-Check "OK" $Name $Path
	}
	return New-Check "WARN" $Name $MissingDetail
}

function Test-ConfiguredCommandOrFile {
	param(
		[string]$Value,
		[string]$Name,
		[string]$Hint
	)
	if ([string]::IsNullOrWhiteSpace($Value)) {
		return New-Check "WARN" $Name $Hint
	}

	$expanded = [Environment]::ExpandEnvironmentVariables($Value)
	if (Test-Path -LiteralPath $expanded -PathType Leaf) {
		return New-Check "OK" $Name $expanded
	}
	if (Test-CommandAvailable $expanded) {
		return New-Check "OK" $Name ((Get-Command $expanded).Source)
	}
	return New-Check "WARN" $Name "configured command or file was not found: $expanded"
}

function Test-ConfiguredFile {
	param(
		[string]$Path,
		[string]$Name,
		[string]$Hint
	)
	if ([string]::IsNullOrWhiteSpace($Path)) {
		return New-Check "WARN" $Name $Hint
	}
	$expanded = [Environment]::ExpandEnvironmentVariables($Path)
	if (Test-Path -LiteralPath $expanded -PathType Leaf) {
		return New-Check "OK" $Name $expanded
	}
	return New-Check "WARN" $Name "configured path was not found: $expanded"
}

function Test-OutputPath {
	param([string]$Path)
	if ([string]::IsNullOrWhiteSpace($Path)) {
		return New-Check "WARN" "music output" "set OFXGGML_MUSIC_OUTPUT or pass -Output"
	}
	$expanded = [Environment]::ExpandEnvironmentVariables($Path)
	$parent = Split-Path -Parent $expanded
	if ([string]::IsNullOrWhiteSpace($parent)) {
		$parent = "."
	}
	if (Test-Path -LiteralPath $parent -PathType Container) {
		return New-Check "OK" "music output" $expanded
	}
	return New-Check "WARN" "music output" "output directory was not found: $parent"
}

function Test-ForbiddenPath {
	param([string]$RelativePath)
	$path = Join-Path $addonRoot $RelativePath
	if (Test-Path -LiteralPath $path) {
		return New-Check "WARN" "artifact hygiene" "generated/local path exists: $RelativePath"
	}
	return $null
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

function Convert-ToOnOff {
	param([bool]$Value)
	if ($Value) { return "ON" }
	return "OFF"
}

function Get-InstalledFiles {
	param(
		[string]$Directory,
		[string[]]$Patterns
	)
	if (!(Test-Path -LiteralPath $Directory -PathType Container)) {
		return @()
	}
	$files = @()
	foreach ($pattern in $Patterns) {
		$files += @(Get-ChildItem -LiteralPath $Directory -File -Filter $pattern -ErrorAction SilentlyContinue)
	}
	return @($files | Sort-Object -Property Name -Unique)
}

function Join-FileNames {
	param([object[]]$Files)
	if ($Files.Count -eq 0) {
		return "none"
	}
	return (($Files | ForEach-Object { $_.Name }) -join ", ")
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

function Get-AceStepChecks {
	$runtimeRoot = Join-Path $addonRoot "libs\acestep"
	$installBin = Join-Path $runtimeRoot "bin"
	$buildDir = Join-Path $runtimeRoot "build"
	$coreGgmlSource = Join-Path (Join-Path $addonsRoot "ofxGgmlCore") "libs\ggml\.source"
	$serverCandidates = @(
		(Join-Path $installBin "ace-server.exe"),
		(Join-Path $installBin "ace-server")
	)
	$server = $serverCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
	$cacheFile = Join-Path $buildDir "CMakeCache.txt"
	$cpuFiles = Get-InstalledFiles -Directory $installBin -Patterns @(
		"ggml-cpu*.dll", "ggml-cpu*.so", "libggml-cpu*.so", "libggml-cpu*.dylib")
	$cudaFiles = Get-InstalledFiles -Directory $installBin -Patterns @(
		"ggml-cuda*.dll", "ggml-cuda*.so", "libggml-cuda*.so", "libggml-cuda*.dylib")
	$vulkanFiles = Get-InstalledFiles -Directory $installBin -Patterns @(
		"ggml-vulkan*.dll", "ggml-vulkan*.so", "libggml-vulkan*.so", "libggml-vulkan*.dylib")
	$metalFiles = Get-InstalledFiles -Directory $installBin -Patterns @(
		"ggml-metal*.dll", "ggml-metal*.so", "libggml-metal*.so", "libggml-metal*.dylib")

	$result = @()
	if (Test-GgmlSourceHasAceStepOps $coreGgmlSource) {
		$result += New-Check "OK" "AceStep ggml source" "ofxGgmlCore ggml has ACE-Step ops"
	} else {
		$result += New-Check "WARN" "AceStep ggml source" "ofxGgmlCore ggml is missing ACE-Step ops; setup falls back to bundled ggml"
	}

	if ($server) {
		$result += New-Check "OK" "AceStep server" $server
	} else {
		$result += New-Check "WARN" "AceStep server" "build with scripts\setup-acestep-server.ps1 -Clean -Cuda"
	}

	if (Test-Path -LiteralPath $cacheFile -PathType Leaf) {
		$result += New-Check "OK" "AceStep build cache" $cacheFile
		$cudaEnabled = Test-CmakeCacheBoolOn -BuildDir $buildDir -Name "GGML_CUDA"
		$vulkanEnabled = Test-CmakeCacheBoolOn -BuildDir $buildDir -Name "GGML_VULKAN"
		$metalEnabled = Test-CmakeCacheBoolOn -BuildDir $buildDir -Name "GGML_METAL"
		$backendDetail = "CMakeCache CUDA=$(Convert-ToOnOff $cudaEnabled) Vulkan=$(Convert-ToOnOff $vulkanEnabled) Metal=$(Convert-ToOnOff $metalEnabled); installed CUDA=$(Join-FileNames $cudaFiles)"
		if ($cudaEnabled -and $cudaFiles.Count -gt 0) {
			$result += New-Check "OK" "AceStep backend" $backendDetail
		} elseif ($cudaEnabled) {
			$result += New-Check "WARN" "AceStep backend" "$backendDetail; CUDA is configured but no ggml-cuda backend artifact is installed"
		} elseif ($vulkanEnabled -or $metalEnabled) {
			$result += New-Check "OK" "AceStep backend" $backendDetail
		} else {
			$result += New-Check "WARN" "AceStep backend" "$backendDetail; installed runtime appears CPU-only"
		}
	} else {
		$result += New-Check "WARN" "AceStep build cache" "CMakeCache.txt not found under libs\acestep\build"
		$result += New-Check "WARN" "AceStep backend" "backend state unknown; build with scripts\setup-acestep-server.ps1 -Clean -Cuda"
	}

	$runtimeDlls = @()
	$runtimeDlls += @($cpuFiles)
	$runtimeDlls += @($cudaFiles)
	$runtimeDlls += @($vulkanFiles)
	$runtimeDlls += @($metalFiles)
	if ($runtimeDlls.Count -gt 0) {
		$result += New-Check "OK" "AceStep runtime DLLs" (Join-FileNames $runtimeDlls)
	} else {
		$result += New-Check "WARN" "AceStep runtime DLLs" "no ggml backend runtime artifacts found in libs\acestep\bin"
	}

	return $result
}

$checks = @()
$checks += New-Check "OK" "addon root" $addonRoot.Path

foreach ($tool in @("git", "cmake")) {
	if (Test-CommandAvailable $tool) {
		$checks += New-Check "OK" $tool ((Get-Command $tool).Source)
	} else {
		$checks += New-Check "WARN" $tool "not found in PATH"
	}
}

$checks += Test-PathCheck `
	-Path (Join-Path $addonsRoot "ofxGgmlCore") `
	-Name "ofxGgmlCore sibling" `
	-MissingDetail "clone beside ofxGgmlMusic" `
	-Directory

$checks += Test-PathCheck `
	-Path (Join-Path $addonsRoot "ofxImGui") `
	-Name "ofxImGui" `
	-MissingDetail "install beside ofxGgmlMusic before building examples" `
	-Directory

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "ofxGgmlMusicAnalysisExample\addons.make") `
	-Name "analysis example" `
	-MissingDetail "ofxGgmlMusicAnalysisExample skeleton is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "ofxGgmlMusicGenerationExample\src\ofApp.cpp") `
	-Name "generation example source" `
	-MissingDetail "generation example source is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicTypes.h") `
	-Name "music request types" `
	-MissingDetail "music request types header is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicProceduralGenerationBackend.cpp") `
	-Name "procedural backend" `
	-MissingDetail "procedural generation backend is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicExternalGenerationBackend.cpp") `
	-Name "external bridge backend" `
	-MissingDetail "external generation bridge is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "tools\ofxGgmlMusicGenerate\main.cpp") `
	-Name "procedural generator CLI" `
	-MissingDetail "procedural generator CLI source is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "tools\musicgen_hf_runner.py") `
	-Name "MusicGen HF runner" `
	-MissingDetail "optional MusicGen runner script is missing"

$checks += Get-AceStepChecks

if (![string]::IsNullOrWhiteSpace($Prompt)) {
	$checks += New-Check "OK" "music prompt" "configured"
} else {
	$checks += New-Check "WARN" "music prompt" "set OFXGGML_MUSIC_PROMPT or pass -Prompt"
}

$checks += Test-OutputPath -Path $Output

$checks += Test-ConfiguredCommandOrFile `
	-Value $ExternalExecutable `
	-Name "external generator" `
	-Hint "set OFXGGML_MUSIC_EXTERNAL_EXECUTABLE or pass -ExternalExecutable for a real model bridge"

$checks += Test-ConfiguredFile `
	-Path $Model `
	-Name "music model" `
	-Hint "set OFXGGML_MUSIC_MODEL or pass -Model when a local model file is available"

$checks += Test-ConfiguredCommandOrFile `
	-Value $Python `
	-Name "MusicGen Python" `
	-Hint "set OFXGGML_MUSIC_PYTHON or pass -Python for the optional Hugging Face MusicGen runner"

$artifactWarnings = @()
foreach ($relative in @(
	"build",
	".vs",
	"ofxGgmlMusicAnalysisExample\bin",
	"ofxGgmlMusicAnalysisExample\obj",
	"ofxGgmlMusicAnalysisExample\.vs",
	"ofxGgmlMusicGenerationExample\bin",
	"ofxGgmlMusicGenerationExample\obj",
	"ofxGgmlMusicGenerationExample\.vs",
	"models"
)) {
	$warning = Test-ForbiddenPath -RelativePath $relative
	if ($null -ne $warning) {
		$artifactWarnings += $warning
	}
}
if ($artifactWarnings.Count -eq 0) {
	$checks += New-Check "OK" "artifact hygiene" "no generated/local paths detected"
} else {
	$checks += $artifactWarnings
}

if ($Json) {
	[pscustomobject]@{
		Root = $addonRoot.Path
		Warnings = $script:Warnings
		Checks = $checks
	} | ConvertTo-Json -Depth 5
} else {
	Write-Host "ofxGgmlMusic doctor"
	Write-Host "Root  $addonRoot"
	Write-Host ""
	foreach ($check in $checks) {
		$line = "{0,-5} {1}" -f $check.State, $check.Name
		if (![string]::IsNullOrWhiteSpace($check.Detail)) {
			$line += " - $($check.Detail)"
		}
		Write-Host $line
	}
	Write-Host ""
	if ($script:Warnings -eq 0) {
		Write-Host "Doctor passed."
	} else {
		Write-Host "Doctor found $script:Warnings warning(s)."
	}
}

if ($Strict -and $script:Warnings -gt 0) {
	exit 1
}
