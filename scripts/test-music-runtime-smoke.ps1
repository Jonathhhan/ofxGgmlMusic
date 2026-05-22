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
$script = Join-Path $scriptRoot "run-music-runtime-smoke.ps1"

Write-Step "Music runtime smoke dry-run"
$textOutput = & $script -DryRun 2>&1 6>&1 | Out-String
Assert-Contains $textOutput "ofxGgmlMusic runtime smoke plan" "runtime smoke dry-run"
Assert-Contains $textOutput "Backend: procedural-sketch" "runtime smoke dry-run"
Assert-Contains $textOutput "ModelBacked: False" "runtime smoke dry-run"
Assert-Contains $textOutput "ProceduralBacked: True" "runtime smoke dry-run"
Assert-Contains $textOutput "Dry run complete; no files were changed" "runtime smoke dry-run"

Write-Step "Music runtime smoke JSON dry-run"
$jsonOutput = & $script -DryRun -Json -SummaryOnly 2>&1 6>&1 | Out-String
$summary = $jsonOutput | ConvertFrom-Json
if ($summary.Name -ne "ofxGgmlMusic runtime smoke") {
	throw "Unexpected runtime smoke name: $($summary.Name)"
}
if ($summary.Backend -ne "procedural-sketch") {
	throw "Unexpected runtime smoke backend: $($summary.Backend)"
}
if ($summary.ModelBacked -or !$summary.ProceduralBacked) {
	throw "Music runtime smoke should report model-free procedural generation."
}
if (!($summary.NextCommands -contains "scripts\run-music-runtime-smoke.bat -Json -SummaryOnly")) {
	throw "JSON dry-run did not include the runtime smoke command."
}
if ($summary.SmokeKind -ne "music-procedural-runtime-smoke") {
	throw "Unexpected runtime smoke kind: $($summary.SmokeKind)"
}
if ($summary.InferenceChecked) {
	throw "Procedural runtime smoke should not report inference evidence."
}

Write-Step "Music runtime smoke report contract"
$scratchDir = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusic-runtime-smoke-contract"
if (Test-Path -LiteralPath $scratchDir) {
	Remove-Item -LiteralPath $scratchDir -Recurse -Force
}
New-Item -ItemType Directory -Path $scratchDir | Out-Null
$reportPath = Join-Path $scratchDir "music-runtime-smoke.json"
$runtimeBuildDir = Join-Path $scratchDir "runtime-build"
try {
	$smokeOutput = & $script -Json -SummaryOnly -BuildDir $runtimeBuildDir -Clean -OutputPath $reportPath 2>&1 6>&1 | Out-String
	$runtimeSummary = $smokeOutput | ConvertFrom-Json
	if (!$runtimeSummary.Passed) {
		throw "Runtime smoke summary did not pass:`n$smokeOutput"
	}
	if (!(Test-Path -LiteralPath $reportPath -PathType Leaf)) {
		throw "Runtime smoke report was not written: $reportPath"
	}
	$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
	if (!$report.Summary.Passed -or $report.Summary.SmokeKind -ne "music-procedural-runtime-smoke") {
		throw "Runtime smoke report summary was malformed."
	}
	if ($report.Summary.InferenceChecked) {
		throw "Runtime smoke report should not claim model-backed inference evidence."
	}
} finally {
	if (Test-Path -LiteralPath $scratchDir) {
		Remove-Item -LiteralPath $scratchDir -Recurse -Force
	}
}

Write-Step "Music runtime smoke contract passed"
