param(
	[ValidateSet("all", "MusicGenHf", "musicgen-hf", "AceStep", "acestep")]
	[string]$Backend = "all",
	[string]$Prompt = "warm lofi loop with soft keys",
	[string]$Output = "",
	[string]$Model = "facebook/musicgen-small",
	[string]$NegativePrompt = "",
	[double]$Duration = 8.0,
	[double]$Tempo = 0.0,
	[string]$Key = "",
	[string]$Mode = "",
	[int]$Seed = 42,
	[double]$Guidance = 3.0,
	[string]$Device = "auto",
	[string]$ServerUrl = $(if ($env:OFXGGML_ACESTEP_SERVER_URL) { $env:OFXGGML_ACESTEP_SERVER_URL } else { "http://127.0.0.1:8085" }),
	[string]$ServerExecutable = $(if ($env:OFXGGML_ACESTEP_SERVER_EXE) { $env:OFXGGML_ACESTEP_SERVER_EXE } else { "" }),
	[string]$ModelPath = $(if ($env:OFXGGML_ACESTEP_MODEL_PATH) { $env:OFXGGML_ACESTEP_MODEL_PATH } else { "" }),
	[Alias("OutputPath")]
	[string]$ReportPath = "",
	[switch]$UseManifestReportPath,
	[switch]$LoadModel,
	[switch]$Json,
	[switch]$SummaryOnly,
	[switch]$Strict
)

$ErrorActionPreference = "Stop"

function Invoke-JsonScript {
	param(
		[string]$ScriptPath,
		[string[]]$ScriptArguments,
		[string]$Label
	)
	$powerShell = Get-PowerShellExecutable
	$arguments = @(
		"-NoProfile",
		"-ExecutionPolicy",
		"Bypass",
		"-File",
		$ScriptPath
	) + $ScriptArguments
	$output = & $powerShell @arguments 2>&1 6>&1 | Out-String
	if (!$?) {
		throw "$Label failed.`n$output"
	}
	try {
		return $output | ConvertFrom-Json
	} catch {
		throw "$Label did not return JSON.`n$output"
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

function Get-ManifestReportPath {
	$manifestPath = Join-Path $addonRoot "ofxggml-addon.json"
	if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
		throw "Addon manifest was not found: $manifestPath"
	}
	$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
	$path = [string]$manifest.generationWorkflowReport
	if ([string]::IsNullOrWhiteSpace($path)) {
		throw "Addon manifest does not define generationWorkflowReport."
	}
	return $path
}

function Write-Report {
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
	[System.IO.File]::WriteAllText($target, ($Payload | ConvertTo-Json -Depth 10))
}

function Write-ReportText {
	param([object]$Summary, [object]$Plan, [object]$Readiness)
	Write-Host "Generation workflow report"
	Write-Host "  Backend: $($Summary.Backend)"
	Write-Host "  Model: $($Summary.Model)"
	Write-Host "  ServerUrl: $($Summary.ServerUrl)"
	Write-Host "  PlanCount: $($Summary.PlanCount)"
	Write-Host "  ReadinessWarnings: $($Summary.ReadinessWarnings)"
	Write-Host "  ReportPath: $($Summary.ReportPath)"
	Write-Host "  ManifestReportPath: $($Summary.ManifestReportPath)"
	foreach ($plan in @($Plan.plans)) {
		Write-Host "  Plan: $($plan.name) [$($plan.backend)]"
	}
	foreach ($check in @($Readiness.Checks)) {
		$line = "  {0,-5} {1} [{2}]" -f $check.State, $check.Name, $check.Backend
		if (![string]::IsNullOrWhiteSpace([string]$check.Detail)) {
			$line += " - $($check.Detail)"
		}
		Write-Host $line
	}
	if ($Summary.Passed) {
		Write-Host "Generation workflow report passed."
	} else {
		Write-Host "Generation workflow report found warning(s)."
	}
}

function New-NextCommands {
	param([string]$ManifestReportPath)
	return @(
		"scripts\plan-generation-workflow.bat -Backend $Backend",
		"scripts\check-generation-readiness.bat -Backend $Backend",
		"scripts\write-generation-workflow-report.bat -Backend $Backend -UseManifestReportPath",
		"scripts\write-generation-workflow-report.bat -Backend $Backend -Json -SummaryOnly",
		"scripts\generate-musicgen-hf.bat -SmokeTest -Json -AllowMissingDeps",
		"scripts\start-acestep-server.bat -DryRun",
		"scripts\test-external-generation-contract.bat -Clean"
	)
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$planScript = Join-Path $scriptRoot "plan-generation-workflow.ps1"
$readinessScript = Join-Path $scriptRoot "check-generation-readiness.ps1"
$effectiveReportPath = $ReportPath
if ($UseManifestReportPath -and [string]::IsNullOrWhiteSpace($effectiveReportPath)) {
	$effectiveReportPath = Get-ManifestReportPath
}

$planArgs = @(
	"-Backend", $Backend,
	"-Prompt", $Prompt,
	"-Model", $Model,
	"-Duration", ([string]$Duration),
	"-Seed", ([string]$Seed),
	"-Guidance", ([string]$Guidance),
	"-ServerUrl", $ServerUrl,
	"-Json"
)
if (![string]::IsNullOrWhiteSpace($Output)) {
	$planArgs += @("-Output", $Output)
}
if (![string]::IsNullOrWhiteSpace($NegativePrompt)) {
	$planArgs += @("-NegativePrompt", $NegativePrompt)
}
if ($Tempo -gt 0.0) {
	$planArgs += @("-Tempo", ([string]$Tempo))
}
if (![string]::IsNullOrWhiteSpace($Key)) {
	$planArgs += @("-Key", $Key)
}
if (![string]::IsNullOrWhiteSpace($Mode)) {
	$planArgs += @("-Mode", $Mode)
}
if (![string]::IsNullOrWhiteSpace($ServerExecutable)) {
	$planArgs += @("-ServerExecutable", $ServerExecutable)
}
if (![string]::IsNullOrWhiteSpace($ModelPath)) {
	$planArgs += @("-ModelPath", $ModelPath)
}

$readinessArgs = @(
	"-Backend", $Backend,
	"-Model", $Model,
	"-Device", $Device,
	"-ServerUrl", $ServerUrl,
	"-Json"
)
if ($LoadModel) {
	$readinessArgs += "-LoadModel"
}

$started = Get-Date
$plan = Invoke-JsonScript -ScriptPath $planScript -ScriptArguments $planArgs -Label "generation workflow plan"
$readiness = Invoke-JsonScript -ScriptPath $readinessScript -ScriptArguments $readinessArgs -Label "generation readiness"
$elapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
$warnings = [int]$readiness.Warnings
$manifestReportPath = Get-ManifestReportPath
$summary = [ordered]@{
	Name = "ofxGgmlMusic generation workflow report"
	Root = $addonRoot.Path
	Backend = $Backend
	Model = $Model
	ServerUrl = $ServerUrl
	ManifestReportPath = $manifestReportPath
	PlanCount = @($plan.plans).Count
	ReadinessWarnings = $warnings
	Passed = ($warnings -eq 0)
	LoadModel = [bool]$LoadModel
	ReportPath = $effectiveReportPath
	UsedManifestReportPath = [bool]($UseManifestReportPath -and [string]::IsNullOrWhiteSpace($ReportPath))
	ElapsedMs = $elapsedMs
	NextCommands = New-NextCommands $manifestReportPath
}
$payload = [ordered]@{
	Summary = $summary
	Plan = $plan
	Readiness = $readiness
}
Write-Report -Path $effectiveReportPath -Payload $payload

if ($Json) {
	if ($SummaryOnly) {
		[pscustomobject]$summary | ConvertTo-Json -Depth 6
	} else {
		[pscustomobject]$payload | ConvertTo-Json -Depth 10
	}
} else {
	Write-ReportText ([pscustomobject]$summary) $plan $readiness
}

if ($Strict -and $warnings -gt 0) {
	exit 1
}
