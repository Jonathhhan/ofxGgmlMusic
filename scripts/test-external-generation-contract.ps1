param(
	[string]$Configuration = "Release",
	[string]$BuildRoot = "",
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
$generatorDir = Join-Path $addonRoot "tools\ofxGgmlMusicGenerate"
$testsDir = Join-Path $addonRoot "tests"
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
	$BuildRoot = Join-Path ([System.IO.Path]::GetTempPath()) "music-contract"
}
$generatorBuildDir = Join-Path $BuildRoot "generator"
$testBuildDir = Join-Path $BuildRoot "tests"
$generatorExe = if (Test-WindowsHost) {
	Join-Path $generatorBuildDir "ofxGgmlMusicGenerate.exe"
} else {
	Join-Path $generatorBuildDir "ofxGgmlMusicGenerate"
}
$contractExe = if (Test-WindowsHost) {
	Join-Path $testBuildDir "music_external_contract.exe"
} else {
	Join-Path $testBuildDir "music_external_contract"
}

if ($DryRun) {
	Write-Step "External music generation contract plan"
	Write-Host "  generator source: $generatorDir"
	Write-Host "  tests source: $testsDir"
	Write-Host "  build root: $BuildRoot"
	Write-Host "  generator exe: $generatorExe"
	Write-Host "  contract exe: $contractExe"
	Write-Host "  clean: $(if ($Clean) { 'ON' } else { 'OFF' })"
	Write-Step "Dry run complete; no files were changed"
	return
}

if ($Clean -and (Test-Path -LiteralPath $BuildRoot)) {
	Write-Step "Cleaning $BuildRoot"
	Remove-Item -LiteralPath $BuildRoot -Recurse -Force
}

if (Test-WindowsHost) {
	$vsDevCmd = Get-VisualStudioDevCmd
	if ([string]::IsNullOrWhiteSpace($vsDevCmd)) {
		throw "Visual Studio C++ build tools were not found."
	}

	$configureGenerator = "cmake -S $(Convert-ToCmdArgument $generatorDir) -B $(Convert-ToCmdArgument $generatorBuildDir) -G $(Convert-ToCmdArgument "NMake Makefiles") -DCMAKE_BUILD_TYPE=$Configuration"
	$buildGenerator = "cmake --build $(Convert-ToCmdArgument $generatorBuildDir)"
	$configureTests = "cmake -S $(Convert-ToCmdArgument $testsDir) -B $(Convert-ToCmdArgument $testBuildDir) -G $(Convert-ToCmdArgument "NMake Makefiles") -DCMAKE_BUILD_TYPE=$Configuration -DOFXGGMLMUSIC_BUILD_EXTERNAL_GENERATION_CONTRACT=ON"
	$buildTests = "cmake --build $(Convert-ToCmdArgument $testBuildDir) --target music_external_contract"
	$run = "$(Convert-ToCmdArgument $contractExe) $(Convert-ToCmdArgument $generatorExe)"
	$command = "call $(Convert-ToCmdArgument $vsDevCmd) -arch=x64 -host_arch=x64 >nul && $configureGenerator && $buildGenerator && $configureTests && $buildTests && $run"
	Write-Step "Building and running external music generation contract with Visual Studio tools"
	Invoke-CheckedCmd "external music generation contract" $command
} else {
	Write-Step "Building procedural music generator"
	Invoke-CheckedNative "configure procedural music generator" {
		cmake -S $generatorDir -B $generatorBuildDir -DCMAKE_BUILD_TYPE=$Configuration
	}
	Invoke-CheckedNative "build procedural music generator" {
		cmake --build $generatorBuildDir --config $Configuration
	}
	Write-Step "Building external music generation contract test"
	Invoke-CheckedNative "configure external music generation contract" {
		cmake -S $testsDir -B $testBuildDir -DCMAKE_BUILD_TYPE=$Configuration -DOFXGGMLMUSIC_BUILD_EXTERNAL_GENERATION_CONTRACT=ON
	}
	Invoke-CheckedNative "build external music generation contract" {
		cmake --build $testBuildDir --target music_external_contract --config $Configuration
	}
	Write-Step "Running external music generation contract"
	Invoke-CheckedNative "external music generation contract" {
		& $contractExe $generatorExe
	}
}
