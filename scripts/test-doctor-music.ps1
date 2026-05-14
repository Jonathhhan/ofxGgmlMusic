$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$doctorScript = Join-Path $scriptRoot "doctor-music.ps1"

$output = & $doctorScript *>&1 | ForEach-Object { $_.ToString() }
if (!$?) {
	throw "doctor-music.ps1 failed."
}

$text = $output -join "`n"
foreach ($expected in @(
	"ofxGgmlMusic doctor",
	"ofxGgmlCore sibling",
	"analysis example",
	"generation example source",
	"music request types",
	"procedural backend",
	"external bridge backend",
	"music prompt",
	"artifact hygiene"
)) {
	if ($text -notmatch [regex]::Escape($expected)) {
		throw "doctor output did not contain expected text: $expected"
	}
}

$jsonOutput = & $doctorScript -Json *>&1 | ForEach-Object { $_.ToString() }
if (!$?) {
	throw "doctor-music.ps1 -Json failed."
}

$parsed = ($jsonOutput -join "`n") | ConvertFrom-Json
if ([string]::IsNullOrWhiteSpace([string]$parsed.Root)) {
	throw "doctor JSON output did not include Root."
}
if (!$parsed.Checks -or $parsed.Checks.Count -eq 0) {
	throw "doctor JSON output did not include checks."
}
