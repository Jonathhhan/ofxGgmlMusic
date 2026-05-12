param(
	[string]$Prompt = "loopable ambient piano motif with granular texture",
	[string]$Output = "",
	[string]$Style = "ambient",
	[double]$Tempo = 92.0,
	[double]$Duration = 8.0,
	[int]$Seed = 42,
	[string]$Key = "C",
	[string]$Mode = "major",
	[string[]]$Stem = @("melody", "bass"),
	[string]$BuildDir = "",
	[switch]$Loop,
	[switch]$Clean
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
$toolRoot = Join-Path $addonRoot "tools\ofxGgmlMusicGenerate"
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
	$BuildDir = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusicGenerate"
}
if ([string]::IsNullOrWhiteSpace($Output)) {
	$Output = Join-Path $addonRoot "ofxGgmlMusicGenerationExample\bin\data\outputs\ofxGgmlMusicGenerate.wav"
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
	Write-Step "Building ofxGgmlMusicGenerate"
	Invoke-CheckedCmd "ofxGgmlMusicGenerate build" $command
	$exe = Join-Path $BuildDir "ofxGgmlMusicGenerate.exe"
} else {
	Write-Step "Configuring ofxGgmlMusicGenerate"
	Invoke-CheckedNative "cmake configure ofxGgmlMusicGenerate" {
		cmake -S $toolRoot -B $BuildDir -DCMAKE_BUILD_TYPE=Release
	}
	Write-Step "Building ofxGgmlMusicGenerate"
	Invoke-CheckedNative "cmake build ofxGgmlMusicGenerate" {
		cmake --build $BuildDir --config Release
	}
	$exe = Join-Path $BuildDir "ofxGgmlMusicGenerate"
}

$args = @(
	"--prompt", $Prompt,
	"--output", $Output,
	"--style", $Style,
	"--tempo", ([string]$Tempo),
	"--duration", ([string]$Duration),
	"--seed", ([string]$Seed),
	"--key", $Key,
	"--mode", $Mode
)
if ($Loop) {
	$args += "--loop"
}
foreach ($stemName in $Stem) {
	if (![string]::IsNullOrWhiteSpace($stemName)) {
		$args += @("--stem", $stemName)
	}
}

Write-Step "Generating procedural music"
& $exe @args
if ($LASTEXITCODE -ne 0) {
	throw "ofxGgmlMusicGenerate failed with exit code $LASTEXITCODE"
}
