$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$smokeScript = Join-Path $scriptRoot "run-acestep-inference-smoke.ps1"
$reportPath = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusic-acestep-dry-run.json"

try {
	$output = & $smokeScript -DryRun -Json -SummaryOnly -ReportPath $reportPath
	$parsed = ($output -join "\n") | ConvertFrom-Json
	if (!$parsed.Summary -or $parsed.Summary.InferenceChecked -or !$parsed.Summary.ModelBacked) {
		throw "AceStep dry-run did not preserve model-backed, non-inference evidence semantics."
	}
	if ([string]$parsed.Summary.SmokeKind -ne "model-backed-acestep-music-generation") {
		throw "AceStep dry-run reported an unexpected smoke kind."
	}
	if ([string]$parsed.Summary.Backend -ne "acestep-server") {
		throw "AceStep dry-run reported an unexpected backend."
	}
	if (!(Test-Path -LiteralPath $reportPath -PathType Leaf)) {
		throw "AceStep dry-run did not write the requested report."
	}
	$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
	if ($report.Summary.InferenceChecked) {
		throw "AceStep dry-run report must not claim inference."
	}
} finally {
	if (Test-Path -LiteralPath $reportPath -PathType Leaf) {
		Remove-Item -LiteralPath $reportPath -Force
	}
}
