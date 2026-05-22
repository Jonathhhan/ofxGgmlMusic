param()

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Assert-Contains {
	param(
		[string]$Text,
		[string]$Needle,
		[string]$Label
	)
	if (!$Text.Contains($Needle)) {
		throw "$Label did not contain expected text: $Needle`n$Text"
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$script = Join-Path $scriptRoot "run-musicgen-hf-inference-smoke.ps1"

Write-Step "MusicGen HF inference smoke dry-run"
$textOutput = & $script -DryRun 2>&1 6>&1 | Out-String
Assert-Contains $textOutput "ofxGgmlMusic MusicGen HF inference smoke plan" "MusicGen inference smoke dry-run"
Assert-Contains $textOutput "Backend: huggingface-musicgen" "MusicGen inference smoke dry-run"
Assert-Contains $textOutput "Generate: False" "MusicGen inference smoke dry-run"
Assert-Contains $textOutput "Dry run complete; no files were changed" "MusicGen inference smoke dry-run"

Write-Step "MusicGen HF inference smoke JSON dry-run"
$jsonOutput = & $script -DryRun -Json -SummaryOnly 2>&1 6>&1 | Out-String
$summary = $jsonOutput | ConvertFrom-Json
if ($summary.Name -ne "ofxGgmlMusic MusicGen HF inference smoke") {
	throw "Unexpected MusicGen inference smoke name: $($summary.Name)"
}
if ($summary.Backend -ne "huggingface-musicgen") {
	throw "Unexpected MusicGen inference smoke backend: $($summary.Backend)"
}
if ($summary.SmokeKind -ne "musicgen-hf-load-model") {
	throw "Unexpected MusicGen inference smoke kind: $($summary.SmokeKind)"
}
if (!$summary.ModelBacked) {
	throw "MusicGen inference smoke should be model-backed."
}
if ($summary.InferenceChecked) {
	throw "Dry-run should not claim inference evidence."
}
if (!($summary.NextCommands -contains "scripts\run-musicgen-hf-inference-smoke.bat -Json -SummaryOnly -OutputPath .musicgen-inference-smoke.json")) {
	throw "JSON dry-run did not include the model-load evidence command."
}

Write-Step "MusicGen HF generation smoke JSON dry-run"
$generationJsonOutput = & $script -DryRun -Generate -Json -SummaryOnly 2>&1 6>&1 | Out-String
$generationSummary = $generationJsonOutput | ConvertFrom-Json
if ($generationSummary.SmokeKind -ne "musicgen-hf-generation") {
	throw "Unexpected MusicGen generation smoke kind: $($generationSummary.SmokeKind)"
}

Write-Step "MusicGen HF inference smoke contract passed"
