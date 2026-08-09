param(
	[string]$Model = "facebook/musicgen-small",
	[string]$Prompt = "release smoke loopable electronic texture",
	[string]$BuildDir = "",
	[Alias("OutputPath")]
	[string]$ReportPath = "",
	[switch]$Generate,
	[switch]$DryRun,
	[switch]$Json,
	[switch]$SummaryOnly
)

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Write-SmokeReport {
	param(
		[string]$Path,
		[object]$Payload
	)

	if ([string]::IsNullOrWhiteSpace($Path)) {
		return
	}

	$target = if ([System.IO.Path]::IsPathRooted($Path)) {
		$Path
	} else {
		Join-Path $addonRoot $Path
	}
	$directory = Split-Path -Parent $target
	if (![string]::IsNullOrWhiteSpace($directory) -and !(Test-Path -LiteralPath $directory -PathType Container)) {
		New-Item -ItemType Directory -Path $directory -Force | Out-Null
	}
	[System.IO.File]::WriteAllText($target, ($Payload | ConvertTo-Json -Depth 8))
}

function Invoke-Step {
	param(
		[string]$Name,
		[string[]]$Arguments
	)

	$output = @()
	$exitCode = 0
	$previousErrorActionPreference = $ErrorActionPreference
	try {
		$ErrorActionPreference = "Continue"
		$output = & $powerShell @Arguments 2>&1 | ForEach-Object { "$_" }
		$exitCode = $LASTEXITCODE
	} catch {
		$output += "$_"
		$exitCode = 1
	} finally {
		$ErrorActionPreference = $previousErrorActionPreference
	}

	[ordered]@{
		Name = $Name
		Passed = ($exitCode -eq 0)
		ExitCode = $exitCode
		Output = @($output)
	}
}

function Get-PowerShellExecutable {
	$pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
	if ($pwsh) {
		return $pwsh.Source
	}

	$windowsPowerShell = Get-Command powershell -ErrorAction SilentlyContinue
	if ($windowsPowerShell) {
		return $windowsPowerShell.Source
	}

	throw "Could not find pwsh or powershell."
}

function Test-GeneratedArtifact {
	param(
		[string]$Path,
		[string]$Label
	)

	if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
		throw "$Label was not generated: $Path"
	}
	return $Path
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$generateScript = Join-Path $scriptRoot "generate-musicgen-hf.ps1"

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
	$BuildDir = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusic-musicgen-hf-smoke"
}

$artifactDir = Join-Path $BuildDir "artifacts"
$generatedAudioPath = Join-Path $artifactDir "musicgen-hf-smoke.wav"

if ($DryRun) {
	$summary = [ordered]@{
		Name = "ofxGgmlMusic MusicGen HF inference smoke"
		Passed = $true
		SmokeKind = if ($Generate) { "musicgen-hf-generation" } else { "musicgen-hf-load-model" }
		Backend = "huggingface-musicgen"
		ModelPath = $Model
		ModelBacked = $true
		InferenceChecked = $false
		GenerationChecked = $false
		BuildDir = $BuildDir
		OutputPath = $generatedAudioPath
		ReportPath = $ReportPath
		NextCommands = @(
			"scripts\run-musicgen-hf-inference-smoke.bat -Json -SummaryOnly -OutputPath .musicgen-inference-smoke.json",
			"scripts\run-musicgen-hf-inference-smoke.bat -Generate -Json -SummaryOnly -OutputPath .musicgen-inference-smoke.json"
		)
		Error = ""
	}
	if ($Json) {
		$summary | ConvertTo-Json -Depth 5
		return
	}

	Write-Step "ofxGgmlMusic MusicGen HF inference smoke plan"
	Write-Host "  Backend: $($summary.Backend)"
	Write-Host "  Model: $($summary.ModelPath)"
	Write-Host "  Generate: $($Generate.IsPresent)"
	Write-Host "  BuildDir: $($summary.BuildDir)"
	Write-Host "  Report: $($summary.ReportPath)"
	Write-Step "Dry run complete; no files were changed"
	return
}

New-Item -ItemType Directory -Path $artifactDir -Force | Out-Null
$powerShell = Get-PowerShellExecutable
$started = Get-Date
$args = @(
	"-NoProfile",
	"-ExecutionPolicy",
	"Bypass",
	"-File",
	$generateScript
)

if ($Generate) {
	$args += @(
		"-Prompt",
		$Prompt,
		"-Output",
		$generatedAudioPath,
		"-Model",
		$Model,
		"-Duration",
		"1.0",
		"-BuildDir",
		$BuildDir,
		"-Clean"
	)
} else {
	$args += @(
		"-SmokeTest",
		"-Json",
		"-LoadModel",
		"-Model",
		$Model
	)
}

$results = @()
$results += Invoke-Step -Name $(if ($Generate) { "MusicGen HF generation" } else { "MusicGen HF model load" }) -Arguments $args

if ($Generate) {
	$artifactCheck = [ordered]@{
		Name = "MusicGen HF generated artifacts"
		Passed = $false
		ExitCode = 1
		Output = @()
	}
	try {
		$artifacts = @()
		$artifacts += Test-GeneratedArtifact -Path $generatedAudioPath -Label "MusicGen WAV"
		$artifacts += Test-GeneratedArtifact -Path ($generatedAudioPath + ".json") -Label "MusicGen manifest"
		$artifactCheck.Passed = $true
		$artifactCheck.ExitCode = 0
		$artifactCheck.Output = $artifacts
	} catch {
		$artifactCheck.Output = @("$_")
	}
	$results += $artifactCheck
}

$failed = @($results | Where-Object { -not $_.Passed })
$elapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
$passed = $failed.Count -eq 0
$summary = [ordered]@{
	Name = "ofxGgmlMusic MusicGen HF inference smoke"
	Passed = $passed
	SmokeKind = if ($Generate) { "musicgen-hf-generation" } else { "musicgen-hf-load-model" }
	Backend = "huggingface-musicgen"
	ModelPath = $Model
	ModelBacked = $true
	InferenceChecked = $passed
	GenerationChecked = [bool]($Generate -and $passed)
	BuildDir = $BuildDir
	OutputPath = $generatedAudioPath
	ReportPath = $ReportPath
	ResultCount = $results.Count
	FailedCount = $failed.Count
	ElapsedMs = $elapsedMs
	Error = $(if ($passed) { "" } else { (($failed | ForEach-Object { $_.Output }) -join "`n") })
}
$payload = [ordered]@{
	Summary = $summary
	Results = $results
}
Write-SmokeReport -Path $ReportPath -Payload $payload

if ($Json) {
	if ($SummaryOnly) {
		$summary | ConvertTo-Json -Depth 5
	} else {
		$payload | ConvertTo-Json -Depth 6
	}
} else {
	foreach ($result in $results) {
		Write-Step $result.Name
		foreach ($line in @($result.Output)) {
			Write-Host $line
		}
	}
	Write-Step "ofxGgmlMusic MusicGen HF inference smoke summary"
	Write-Host "  Backend: $($summary.Backend)"
	Write-Host "  ModelBacked: $($summary.ModelBacked)"
	Write-Host "  InferenceChecked: $($summary.InferenceChecked)"
	Write-Host "  GenerationChecked: $($summary.GenerationChecked)"
	Write-Host "  Passed: $($summary.Passed)"
}

if (!$passed) {
	exit 1
}
