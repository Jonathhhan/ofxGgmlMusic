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
