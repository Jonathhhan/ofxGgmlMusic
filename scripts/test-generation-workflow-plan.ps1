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

function Assert-Equals {
	param([object]$Actual, [object]$Expected, [string]$Label)
	if ($Actual -ne $Expected) {
		throw "$Label expected '$Expected' but got '$Actual'."
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$script = Join-Path $scriptRoot "plan-generation-workflow.ps1"

Write-Step "MusicGen workflow JSON plan"
$musicGenJson = & $script `
	-Backend MusicGenHf `
	-Prompt "test prompt" `
	-NegativePrompt "muddy drums" `
	-Output "C:\tmp\musicgen-plan.wav" `
	-Tempo 92 `
	-Key C `
	-Mode major `
	-Json 2>&1 | Out-String
if (!$?) {
	throw "MusicGen workflow plan failed.`n$musicGenJson"
}
$musicGen = $musicGenJson | ConvertFrom-Json
Assert-Equals $musicGen.name "ofxGgmlMusic generation workflow plan" "MusicGen plan name"
Assert-Equals $musicGen.plans.Count 1 "MusicGen plan count"
Assert-Equals $musicGen.plans[0].backend "musicgen-hf" "MusicGen backend"
Assert-Contains $musicGen.plans[0].commands.generate "generate-musicgen-hf" "MusicGen generate command"
Assert-Contains $musicGen.plans[0].commands.generate "-NegativePrompt `"muddy drums`"" "MusicGen generate command"
Assert-Contains $musicGen.plans[0].commands.generate "-Tempo 92" "MusicGen generate command"
Assert-Contains $musicGen.plans[0].commands.generate "-Key C" "MusicGen generate command"
Assert-Contains $musicGen.plans[0].commands.generate "-Mode major" "MusicGen generate command"
Assert-Contains $musicGen.plans[0].commands.dryRun "-DryRun" "MusicGen dry-run command"
Assert-Contains ($musicGen.plans[0].artifacts -join "`n") "C:\tmp\musicgen-plan.wav.json" "MusicGen artifacts"

Write-Step "ACE-Step workflow JSON plan"
$aceStepJson = & $script `
	-Backend AceStep `
	-ServerExecutable "mock-acestep-server.exe" `
	-ModelPath "C:\mock models\acestep" `
	-ServerUrl "127.0.0.1:8185" `
	-Json 2>&1 | Out-String
if (!$?) {
	throw "ACE-Step workflow plan failed.`n$aceStepJson"
}
$aceStep = $aceStepJson | ConvertFrom-Json
Assert-Equals $aceStep.plans.Count 1 "ACE-Step plan count"
Assert-Equals $aceStep.plans[0].backend "acestep" "ACE-Step backend"
Assert-Contains $aceStep.plans[0].commands.setupDryRun "setup-acestep-server" "ACE-Step setup command"
Assert-Contains $aceStep.plans[0].commands.startDryRun "-DryRun" "ACE-Step start dry-run command"
Assert-Contains $aceStep.plans[0].commands.startServer "mock-acestep-server.exe" "ACE-Step start command"
Assert-Contains ($aceStep.plans[0].environment -join "`n") "OFXGGML_ACESTEP_AUTOSTART" "ACE-Step env"

Write-Step "Combined workflow text plan"
$textPlan = & $script -Backend all 2>&1 6>&1 | Out-String
if (!$?) {
	throw "Combined workflow plan failed.`n$textPlan"
}
Assert-Contains $textPlan "Generation workflow plan" "combined text plan"
Assert-Contains $textPlan "Hugging Face MusicGen" "combined text plan"
Assert-Contains $textPlan "ACE-Step local server" "combined text plan"
Assert-Contains $textPlan "Generated audio and manifests stay outside git." "combined text plan"

Write-Step "Generation workflow plan contract passed"
