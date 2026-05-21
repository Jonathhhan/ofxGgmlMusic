param(
	[string]$Prompt = "loopable electronic texture with warm chords",
	[string]$Output = "",
	[string]$Model = "facebook/musicgen-small",
	[double]$Duration = 8.0,
	[int]$Seed = 42,
	[double]$Guidance = 3.0,
	[string]$BuildDir = "",
	[switch]$Json,
	[switch]$Clean,
	[switch]$DryRun,
	[switch]$SmokeTest,
	[switch]$LoadModel,
	[switch]$AllowMissingDeps
)

$ErrorActionPreference = "Stop"

function Test-WindowsHost {
	return !($IsLinux -or $IsMacOS)
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
if ([string]::IsNullOrWhiteSpace($Output)) {
	$Output = Join-Path $addonRoot "ofxGgmlMusicGenerationExample\bin\data\outputs\musicgen-hf.wav"
}
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
	$BuildDir = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusicExternalGenerate"
}
$runner = if (Test-WindowsHost) {
	Join-Path $scriptRoot "run-musicgen-hf.bat"
} else {
	Join-Path $scriptRoot "run-musicgen-hf.sh"
}

if ($SmokeTest) {
	$runnerArgs = @(
		"--smoke-test",
		"--model",
		$Model
	)
	if ($Json) {
		$runnerArgs += "--json"
	}
	if ($LoadModel) {
		$runnerArgs += "--load-model"
	}
	if ($AllowMissingDeps) {
		$runnerArgs += "--allow-missing-deps"
	}
	& $runner @runnerArgs
	if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
		throw "MusicGen HF smoke test failed with exit code $LASTEXITCODE"
	}
	return
}

$args = @{
	Executable = $runner
	Prompt = $Prompt
	Output = $Output
	Model = $Model
	AllowModelId = $true
	Style = "musicgen"
	Duration = $Duration
	Seed = $Seed
	BuildDir = $BuildDir
	ExtraArgument = @("--guidance", ([string]$Guidance))
}
if ($Json) {
	$args.Json = $true
}
if ($Clean) {
	$args.Clean = $true
}
if ($DryRun) {
	$args.DryRun = $true
}

& (Join-Path $scriptRoot "generate-external-music.ps1") @args
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
	throw "MusicGen HF generation failed with exit code $LASTEXITCODE"
}
