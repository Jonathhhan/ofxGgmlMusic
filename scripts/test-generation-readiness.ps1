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
$script = Join-Path $scriptRoot "check-generation-readiness.ps1"

Write-Step "Generation readiness text"
$textOutput = & $script -Backend all -ServerUrl "127.0.0.1:9" 2>&1 6>&1 | Out-String
if (!$?) {
	throw "Generation readiness text check failed.`n$textOutput"
}
Assert-Contains $textOutput "Generation readiness" "readiness text"
Assert-Contains $textOutput "MusicGen Python stack" "readiness text"
Assert-Contains $textOutput "AceStep server health" "readiness text"

Write-Step "MusicGen readiness JSON"
$musicGenJson = & $script -Backend MusicGenHf -Json 2>&1 6>&1 | Out-String
if (!$?) {
	throw "MusicGen readiness JSON check failed.`n$musicGenJson"
}
$musicGen = $musicGenJson | ConvertFrom-Json
if ($musicGen.Name -ne "ofxGgmlMusic generation readiness") {
	throw "Unexpected readiness JSON name: $($musicGen.Name)"
}
if ($musicGen.Checks.Count -ne 1 -or $musicGen.Checks[0].Backend -ne "musicgen-hf") {
	throw "MusicGen readiness JSON did not include one MusicGen check."
}

Write-Step "AceStep readiness JSON"
$aceJson = & $script -Backend AceStep -ServerUrl "127.0.0.1:9" -Json 2>&1 6>&1 | Out-String
if (!$?) {
	throw "AceStep readiness JSON check failed.`n$aceJson"
}
$ace = $aceJson | ConvertFrom-Json
if ($ace.Checks.Count -ne 1 -or $ace.Checks[0].Backend -ne "acestep") {
	throw "AceStep readiness JSON did not include one AceStep check."
}
if ($ace.Checks[0].State -ne "WARN") {
	throw "AceStep offline readiness check should report WARN."
}

Write-Step "Generation readiness contract passed"
