$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Get-PythonExecutable {
	if (![string]::IsNullOrWhiteSpace($env:OFXGGML_MUSIC_PYTHON) -and
		(Test-Path -LiteralPath $env:OFXGGML_MUSIC_PYTHON -PathType Leaf)) {
		return $env:OFXGGML_MUSIC_PYTHON
	}

	$python = Get-Command python -ErrorAction SilentlyContinue
	if ($python) {
		return $python.Source
	}

	$python3 = Get-Command python3 -ErrorAction SilentlyContinue
	if ($python3) {
		return $python3.Source
	}

	return ""
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$runner = Join-Path $addonRoot "tools\musicgen_hf_runner.py"

Write-Step "MusicGen HF smoke contract"
$python = Get-PythonExecutable
if ([string]::IsNullOrWhiteSpace($python)) {
	Write-Host "Python was not found; skipping optional MusicGen HF smoke probe."
	return
}

$generateScript = Join-Path $scriptRoot "generate-musicgen-hf.ps1"
$jsonOutput = & $generateScript `
	-SmokeTest `
	-Json `
	-AllowMissingDeps `
	-Model facebook/musicgen-small 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) {
	throw "MusicGen HF smoke probe failed with exit code $LASTEXITCODE`n$jsonOutput"
}

$summary = $jsonOutput | ConvertFrom-Json
if ($summary.name -ne "ofxGgmlMusic Hugging Face MusicGen smoke") {
	throw "Unexpected MusicGen HF smoke name: $($summary.name)"
}
if ($summary.model -ne "facebook/musicgen-small") {
	throw "MusicGen HF smoke did not preserve the model id."
}
if (!$summary.dependencies.PSObject.Properties.Name.Contains("torch") -or
	!$summary.dependencies.PSObject.Properties.Name.Contains("transformers") -or
	!$summary.dependencies.PSObject.Properties.Name.Contains("numpy")) {
	throw "MusicGen HF smoke did not report the expected Python dependencies."
}
if ($summary.loadModel) {
	throw "MusicGen HF smoke contract should not load a model unless explicitly requested."
}

Write-Step "MusicGen HF smoke contract passed"
