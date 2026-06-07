param()

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Assert-Contains {
	param([string]$Text, [string]$Needle, [string]$Label)
	if (!$Text.Contains($Needle)) {
		throw "$Label did not contain '$Needle'. Output:`n$Text"
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Split-Path -Parent $scriptRoot
$script = Join-Path $scriptRoot "write-generation-workflow-report.ps1"
$reportPath = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusic-generation-workflow-report.json"
$manifestReportPath = Join-Path $addonRoot ".generation-workflow-report.json"
if (Test-Path -LiteralPath $reportPath) {
	Remove-Item -LiteralPath $reportPath -Force
}
if (Test-Path -LiteralPath $manifestReportPath) {
	Remove-Item -LiteralPath $manifestReportPath -Force
}

Write-Step "Generation workflow report text"
$textOutput = & $script `
	-Backend all `
	-ServerUrl "127.0.0.1:9" `
	-ReportPath $reportPath 2>&1 6>&1 | Out-String
if (!$?) {
	throw "Generation workflow report text failed.`n$textOutput"
}
Assert-Contains $textOutput "Generation workflow report" "workflow report text"
Assert-Contains $textOutput "Plan:" "workflow report text"
Assert-Contains $textOutput "MusicGen Python stack" "workflow report text"
if (!(Test-Path -LiteralPath $reportPath -PathType Leaf)) {
	throw "Generation workflow report did not write ReportPath: $reportPath"
}

Write-Step "Generation workflow report JSON"
$jsonOutput = & $script `
	-Backend MusicGenHf `
	-NegativePrompt "muddy drums" `
	-Tempo 92 `
	-Key C `
	-Mode major `
	-ServerUrl "127.0.0.1:9" `
	-Json 2>&1 6>&1 | Out-String
if (!$?) {
	throw "Generation workflow report JSON failed.`n$jsonOutput"
}
$report = $jsonOutput | ConvertFrom-Json
if ($report.Summary.Name -ne "ofxGgmlMusic generation workflow report") {
	throw "Unexpected workflow report name: $($report.Summary.Name)"
}
if ($report.Summary.PlanCount -ne 1) {
	throw "MusicGen workflow report should contain one plan."
}
if ($report.Summary.ManifestReportPath -ne ".generation-workflow-report.json") {
	throw "Workflow report did not include manifest report path."
}
if (!($report.Summary.NextCommands -contains "scripts\write-generation-workflow-report.bat -Backend MusicGenHf -UseManifestReportPath")) {
	throw "Workflow report did not include the manifest report next command."
}
if ($report.Plan.plans[0].backend -ne "musicgen-hf") {
	throw "MusicGen workflow report did not include the MusicGen plan."
}
if ($report.Readiness.Checks[0].Backend -ne "musicgen-hf") {
	throw "MusicGen workflow report did not include MusicGen readiness."
}

Write-Step "Generation workflow report summary JSON"
$summaryJson = & $script -Backend AceStep -ServerUrl "127.0.0.1:9" -Json -SummaryOnly 2>&1 6>&1 | Out-String
if (!$?) {
	throw "Generation workflow report summary JSON failed.`n$summaryJson"
}
$summary = $summaryJson | ConvertFrom-Json
if ($summary.Backend -ne "AceStep") {
	throw "Summary did not preserve requested backend: $($summary.Backend)"
}
if ($summary.PlanCount -ne 1) {
	throw "AceStep summary should contain one plan."
}
if (!($summary.NextCommands -contains "scripts\check-generation-readiness.bat -Backend AceStep")) {
	throw "Summary did not include the readiness next command."
}

Write-Step "Generation workflow manifest report path"
$manifestOutput = & $script -Backend MusicGenHf -UseManifestReportPath -Json -SummaryOnly 2>&1 6>&1 | Out-String
if (!$?) {
	throw "Generation workflow manifest report path failed.`n$manifestOutput"
}
$manifestSummary = $manifestOutput | ConvertFrom-Json
if (!$manifestSummary.UsedManifestReportPath) {
	throw "Summary did not report manifest path usage."
}
if ($manifestSummary.ReportPath -ne ".generation-workflow-report.json") {
	throw "Summary did not use manifest report path: $($manifestSummary.ReportPath)"
}
if (!(Test-Path -LiteralPath $manifestReportPath -PathType Leaf)) {
	throw "Generation workflow report did not write manifest report path: $manifestReportPath"
}

Remove-Item -LiteralPath $reportPath -Force
Remove-Item -LiteralPath $manifestReportPath -Force
Write-Step "Generation workflow report contract passed"
