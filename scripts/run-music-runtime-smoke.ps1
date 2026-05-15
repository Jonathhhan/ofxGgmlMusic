param(
	[string]$Configuration = "Release",
	[string]$BuildDir = "",
	[switch]$Clean,
	[switch]$DryRun,
	[switch]$Json,
	[switch]$SummaryOnly
)

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
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

function Test-RuntimeSmokeReady {
	return (Test-Path -LiteralPath (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicProceduralGenerationBackend.cpp") -PathType Leaf) -and
		(Test-Path -LiteralPath (Join-Path $addonRoot "tools\ofxGgmlMusicGenerate\main.cpp") -PathType Leaf) -and
		(Test-Path -LiteralPath (Join-Path $addonRoot "tests\test_main.cpp") -PathType Leaf) -and
		(Test-Path -LiteralPath (Join-Path $addonsRoot "ofxGgmlCore") -PathType Container)
}

function New-DryRunSummary {
	$ready = Test-RuntimeSmokeReady
	return [ordered]@{
		Name = "ofxGgmlMusic runtime smoke"
		Root = [string]$addonRoot
		Backend = "procedural-sketch"
		BuildDir = $BuildDir
		Ready = $ready
		ModelBacked = $false
		ProceduralBacked = $true
		ExternalBridgeBacked = $false
		TestScript = $testScript
		DoctorScript = $doctorScript
		ProceduralScript = $proceduralScript
		NextCommands = @(
			"scripts\run-music-runtime-smoke.bat -Json -SummaryOnly",
			"scripts\generate-procedural-music.bat -Preset lofi -Output <temp>\music-runtime-smoke.wav -Loop",
			"scripts\test-external-generation-contract.bat -Clean"
		)
	}
}

function Invoke-SmokeStep {
	param(
		[string]$Name,
		[string[]]$Arguments
	)

	$output = @()
	$exitCode = 0
	try {
		$output = & $powerShell @Arguments 2>&1 | ForEach-Object { "$_" }
		$exitCode = $LASTEXITCODE
	} catch {
		$output += "$_"
		$exitCode = 1
	}

	return [ordered]@{
		Name = $Name
		Passed = ($exitCode -eq 0)
		ExitCode = $exitCode
		Output = $output
	}
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
$addonsRoot = Split-Path -Parent $addonRoot
$testScript = Join-Path $scriptRoot "test-addon.ps1"
$doctorScript = Join-Path $scriptRoot "doctor-music.ps1"
$proceduralScript = Join-Path $scriptRoot "generate-procedural-music.ps1"

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
	$BuildDir = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusic-runtime-smoke"
}

if ($DryRun) {
	$summary = New-DryRunSummary
	if ($Json) {
		$summary | ConvertTo-Json -Depth 5
		return
	}

	Write-Step "ofxGgmlMusic runtime smoke plan"
	Write-Host "  Backend: $($summary.Backend)"
	Write-Host "  BuildDir: $($summary.BuildDir)"
	Write-Host "  Ready: $($summary.Ready)"
	Write-Host "  ModelBacked: $($summary.ModelBacked)"
	Write-Host "  ProceduralBacked: $($summary.ProceduralBacked)"
	Write-Host "  ExternalBridgeBacked: $($summary.ExternalBridgeBacked)"
	Write-Host "  Test: $($summary.TestScript)"
	Write-Host "  Doctor: $($summary.DoctorScript)"
	Write-Host "  Procedural: $($summary.ProceduralScript)"
	Write-Host "  Next: $($summary.NextCommands[0])"
	Write-Step "Dry run complete; no files were changed"
	return
}

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
	Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$started = Get-Date
$powerShell = Get-PowerShellExecutable
$testBuildDir = Join-Path $BuildDir "tests"
$proceduralBuildDir = Join-Path $BuildDir "procedural-build"
$artifactDir = Join-Path $BuildDir "artifacts"
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
$outputPath = Join-Path $artifactDir "music-runtime-smoke.wav"

$testArgs = @(
	"-NoProfile",
	"-ExecutionPolicy",
	"Bypass",
	"-File",
	$testScript,
	"-Configuration",
	$Configuration,
	"-BuildDir",
	$testBuildDir
)
if ($Clean) {
	$testArgs += "-Clean"
}
$doctorArgs = @(
	"-NoProfile",
	"-ExecutionPolicy",
	"Bypass",
	"-File",
	$doctorScript,
	"-Json"
)
$proceduralArgs = @(
	"-NoProfile",
	"-ExecutionPolicy",
	"Bypass",
	"-File",
	$proceduralScript,
	"-Preset",
	"lofi",
	"-Prompt",
	"runtime smoke lofi motif",
	"-Output",
	$outputPath,
	"-Duration",
	"1.0",
	"-Tempo",
	"96",
	"-Key",
	"C",
	"-Mode",
	"major",
	"-Stem",
	"melody",
	"-BuildDir",
	$proceduralBuildDir,
	"-Loop"
)
if ($Clean) {
	$proceduralArgs += "-Clean"
}

$results = @()
$results += Invoke-SmokeStep -Name "music helper tests" -Arguments $testArgs
$results += Invoke-SmokeStep -Name "Music doctor" -Arguments $doctorArgs
$results += Invoke-SmokeStep -Name "procedural-sketch generation" -Arguments $proceduralArgs

$artifactCheck = [ordered]@{
	Name = "procedural-sketch artifacts"
	Passed = $false
	ExitCode = 1
	Output = @()
}
try {
	$artifacts = @()
	$artifacts += Test-GeneratedArtifact -Path $outputPath -Label "procedural WAV"
	$artifacts += Test-GeneratedArtifact -Path ($outputPath + ".json") -Label "procedural manifest"
	$artifacts += Test-GeneratedArtifact -Path (Join-Path $artifactDir "ofxGgmlMusic-history.json") -Label "procedural history"
	$artifacts += Test-GeneratedArtifact -Path (Join-Path $artifactDir "music-runtime-smoke-melody.mid") -Label "melody MIDI"
	$artifacts += Test-GeneratedArtifact -Path (Join-Path $artifactDir "music-runtime-smoke-chords.mid") -Label "chord MIDI"
	$artifacts += Test-GeneratedArtifact -Path (Join-Path $artifactDir "music-runtime-smoke-arrangement.mid") -Label "arrangement MIDI"
	$artifacts += Test-GeneratedArtifact -Path (Join-Path $artifactDir "music-runtime-smoke-melody.wav") -Label "melody stem"
	$artifactCheck.Passed = $true
	$artifactCheck.ExitCode = 0
	$artifactCheck.Output = $artifacts
} catch {
	$artifactCheck.Output = @("$_")
}
$results += $artifactCheck

$failed = @($results | Where-Object { -not $_.Passed })
$elapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
$summary = [ordered]@{
	Name = "ofxGgmlMusic runtime smoke"
	Passed = ($failed.Count -eq 0)
	Backend = "procedural-sketch"
	Configuration = $Configuration
	BuildDir = $BuildDir
	ModelBacked = $false
	ProceduralBacked = $true
	ExternalBridgeBacked = $false
	ResultCount = $results.Count
	FailedCount = $failed.Count
	ElapsedMs = $elapsedMs
	OutputPath = $outputPath
	Error = $(if ($failed.Count -eq 0) { "" } else { (($failed | ForEach-Object { $_.Output }) -join "`n") })
}

if ($Json) {
	if ($SummaryOnly) {
		$summary | ConvertTo-Json -Depth 5
	} else {
		[ordered]@{
			Summary = $summary
			Results = $results
		} | ConvertTo-Json -Depth 6
	}
} else {
	foreach ($result in $results) {
		Write-Step $result.Name
		foreach ($line in $result.Output) {
			Write-Host $line
		}
	}
	Write-Step "ofxGgmlMusic runtime smoke summary"
	Write-Host "  Backend: $($summary.Backend)"
	Write-Host "  ModelBacked: $($summary.ModelBacked)"
	Write-Host "  ProceduralBacked: $($summary.ProceduralBacked)"
	Write-Host "  Passed: $($summary.Passed)"
	Write-Host "  OutputPath: $($summary.OutputPath)"
	Write-Host "  ElapsedMs: $($summary.ElapsedMs)"
}

if ($failed.Count -gt 0) {
	exit 1
}
