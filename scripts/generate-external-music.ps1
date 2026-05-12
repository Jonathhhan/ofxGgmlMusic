param(
	[string]$Executable = "",
	[string]$Prompt = "loopable electronic music sketch",
	[string]$Output = "",
	[string]$Model = "",
	[string]$WorkingDirectory = "",
	[string]$Style = "",
	[double]$Tempo = 0.0,
	[double]$Duration = 8.0,
	[int]$Seed = 42,
	[string]$Key = "",
	[string]$Mode = "",
	[string]$PromptFlag = "--prompt",
	[string]$OutputFlag = "--output",
	[string]$ModelFlag = "--model",
	[string]$DurationFlag = "--duration",
	[string]$SeedFlag = "--seed",
	[string[]]$ExtraArgument = @(),
	[string]$BuildDir = "",
	[switch]$AllowModelId,
	[switch]$Json,
	[switch]$Clean,
	[switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Test-WindowsHost {
	return !($IsLinux -or $IsMacOS)
}

function Convert-ToCmdArgument {
	param([string]$Value)
	return '"' + ($Value -replace '"', '""') + '"'
}

function Invoke-CheckedNative {
	param(
		[string]$Step,
		[scriptblock]$Command
	)
	& $Command
	if ($LASTEXITCODE -ne 0) {
		throw "$Step failed with exit code $LASTEXITCODE"
	}
}

function Invoke-CheckedCmd {
	param(
		[string]$Step,
		[string]$Command
	)
	& cmd.exe /d /s /c $Command
	if ($LASTEXITCODE -ne 0) {
		throw "$Step failed with exit code $LASTEXITCODE"
	}
}

function Get-VisualStudioDevCmd {
	$candidates = New-Object System.Collections.Generic.List[string]
	$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
	if (Test-Path -LiteralPath $vswhere) {
		$installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
		if ($installPath) {
			$candidates.Add((Join-Path $installPath "Common7\Tools\VsDevCmd.bat"))
		}
	}

	foreach ($version in @("18", "17", "16")) {
		foreach ($edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
			$candidates.Add("C:\Program Files\Microsoft Visual Studio\$version\$edition\Common7\Tools\VsDevCmd.bat")
			$candidates.Add("C:\Program Files (x86)\Microsoft Visual Studio\$version\$edition\Common7\Tools\VsDevCmd.bat")
		}
	}

	foreach ($candidate in $candidates) {
		if (Test-Path -LiteralPath $candidate) {
			return $candidate
		}
	}
	return ""
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$toolRoot = Join-Path $addonRoot "tools\ofxGgmlMusicExternalGenerate"
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
	$BuildDir = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusicExternalGenerate"
}
if ([string]::IsNullOrWhiteSpace($Output)) {
	$Output = Join-Path $addonRoot "ofxGgmlMusicGenerationExample\bin\data\outputs\ofxGgmlMusicExternal.wav"
}
$toolExe = if (Test-WindowsHost) {
	Join-Path $BuildDir "ofxGgmlMusicExternalGenerate.exe"
} else {
	Join-Path $BuildDir "ofxGgmlMusicExternalGenerate"
}

if ($DryRun) {
	Write-Step "External music generation plan"
	Write-Host "  tool source: $toolRoot"
	Write-Host "  build: $BuildDir"
	Write-Host "  executable: $Executable"
	Write-Host "  model: $Model"
	Write-Host "  allow model id: $(if ($AllowModelId) { 'ON' } else { 'OFF' })"
	Write-Host "  prompt: $Prompt"
	Write-Host "  output: $Output"
	Write-Host "  duration: $Duration"
	Write-Host "  seed: $Seed"
	Write-Step "Dry run complete; no files were changed"
	return
}

if ([string]::IsNullOrWhiteSpace($Executable)) {
	throw "External generator executable is required."
}

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
	Write-Step "Cleaning $BuildDir"
	Remove-Item -LiteralPath $BuildDir -Recurse -Force
}

if (Test-WindowsHost) {
	$vsDevCmd = Get-VisualStudioDevCmd
	if ([string]::IsNullOrWhiteSpace($vsDevCmd)) {
		throw "Visual Studio C++ build tools were not found."
	}
	$configure = "cmake -S $(Convert-ToCmdArgument $toolRoot) -B $(Convert-ToCmdArgument $BuildDir) -G $(Convert-ToCmdArgument "NMake Makefiles") -DCMAKE_BUILD_TYPE=Release"
	$build = "cmake --build $(Convert-ToCmdArgument $BuildDir)"
	$command = "call $(Convert-ToCmdArgument $vsDevCmd) -arch=x64 -host_arch=x64 >nul && $configure && $build"
	Write-Step "Building ofxGgmlMusicExternalGenerate"
	Invoke-CheckedCmd "ofxGgmlMusicExternalGenerate build" $command
} else {
	Write-Step "Configuring ofxGgmlMusicExternalGenerate"
	Invoke-CheckedNative "cmake configure ofxGgmlMusicExternalGenerate" {
		cmake -S $toolRoot -B $BuildDir -DCMAKE_BUILD_TYPE=Release
	}
	Write-Step "Building ofxGgmlMusicExternalGenerate"
	Invoke-CheckedNative "cmake build ofxGgmlMusicExternalGenerate" {
		cmake --build $BuildDir --config Release
	}
}

$args = @(
	"--executable", $Executable,
	"--prompt", $Prompt,
	"--output", $Output,
	"--duration", ([string]$Duration),
	"--seed", ([string]$Seed),
	"--prompt-flag", $PromptFlag,
	"--output-flag", $OutputFlag,
	"--model-flag", $ModelFlag,
	"--duration-flag", $DurationFlag,
	"--seed-flag", $SeedFlag
)
if ($AllowModelId) {
	$args += "--allow-model-id"
}
if (![string]::IsNullOrWhiteSpace($Model)) {
	$args += @("--model", $Model)
}
if (![string]::IsNullOrWhiteSpace($WorkingDirectory)) {
	$args += @("--working-directory", $WorkingDirectory)
}
if (![string]::IsNullOrWhiteSpace($Style)) {
	$args += @("--style", $Style)
}
if ($Tempo -gt 0.0) {
	$args += @("--tempo", ([string]$Tempo))
}
if (![string]::IsNullOrWhiteSpace($Key)) {
	$args += @("--key", $Key)
}
if (![string]::IsNullOrWhiteSpace($Mode)) {
	$args += @("--mode", $Mode)
}
foreach ($argument in $ExtraArgument) {
	if (![string]::IsNullOrWhiteSpace($argument)) {
		$args += @("--extra", $argument)
	}
}
if ($Json) {
	$args += "--json"
}

Write-Step "Generating music through external backend"
& $toolExe @args
if ($LASTEXITCODE -ne 0) {
	throw "ofxGgmlMusicExternalGenerate failed with exit code $LASTEXITCODE"
}
