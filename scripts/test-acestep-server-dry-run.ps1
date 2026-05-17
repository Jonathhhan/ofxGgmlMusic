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
$script = Join-Path $scriptRoot "start-acestep-server.ps1"
$previousArgs = $env:OFXGGML_ACESTEP_SERVER_ARGS

try {
	Write-Step "AceStep server default dry-run"
	$defaultOutput = & $script `
		-ServerExecutable "mock-acestep-server.exe" `
		-ServerUrl "127.0.0.1:8185" `
		-DryRun 2>&1 6>&1 | Out-String
	Assert-Contains $defaultOutput "OFXGGML_ACESTEP_SERVER_DRY_RUN=1" "default dry-run"
	Assert-Contains $defaultOutput "OFXGGML_ACESTEP_SERVER_URL=http://127.0.0.1:8185" "default dry-run"
	Assert-Contains $defaultOutput "--host 127.0.0.1 --port 8185" "default dry-run"

	Write-Step "AceStep quoted env-args dry-run"
	$env:OFXGGML_ACESTEP_SERVER_ARGS = '--models "C:\mock models\acestep" --preset "wide stereo"'
	$quotedOutput = & $script `
		-ServerExecutable "mock-acestep-server.exe" `
		-ServerUrl "http://127.0.0.1:8085" `
		-DryRun 2>&1 6>&1 | Out-String
	Assert-Contains $quotedOutput '--models "C:\mock models\acestep"' "quoted env-args dry-run"
	Assert-Contains $quotedOutput '--preset "wide stereo"' "quoted env-args dry-run"

	Write-Step "AceStep Python launcher dry-run"
	Remove-Item Env:\OFXGGML_ACESTEP_SERVER_ARGS -ErrorAction SilentlyContinue
	$pythonOutput = & $script `
		-ServerExecutable "mock-acestep-server.py" `
		-PythonExecutable "python-mock" `
		-ModelPath "C:\mock models\acestep" `
		-DryRun 2>&1 6>&1 | Out-String
	Assert-Contains $pythonOutput "OFXGGML_ACESTEP_SERVER_COMMAND=python-mock mock-acestep-server.py" "python dry-run"
	Assert-Contains $pythonOutput '--model "C:\mock models\acestep"' "python dry-run"

	Write-Step "AceStep server dry-run coverage passed"
} finally {
	if ($null -eq $previousArgs) {
		Remove-Item Env:\OFXGGML_ACESTEP_SERVER_ARGS -ErrorAction SilentlyContinue
	} else {
		$env:OFXGGML_ACESTEP_SERVER_ARGS = $previousArgs
	}
}
