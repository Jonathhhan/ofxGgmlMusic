param(
	[ValidateSet("all", "MusicGenHf", "musicgen-hf", "AceStep", "acestep")]
	[string]$Backend = "all",
	[string]$Model = "facebook/musicgen-small",
	[string]$Device = "auto",
	[string]$ServerUrl = $(if ($env:OFXGGML_ACESTEP_SERVER_URL) { $env:OFXGGML_ACESTEP_SERVER_URL } else { "http://127.0.0.1:8085" }),
	[int]$TimeoutSeconds = 2,
	[switch]$LoadModel,
	[switch]$Json,
	[switch]$Strict
)

$ErrorActionPreference = "Stop"
$script:Warnings = 0

function Normalize-Backend {
	param([string]$Value)
	$normalized = $Value.ToLowerInvariant()
	if ($normalized -eq "musicgenhf") {
		return "musicgen-hf"
	}
	return $normalized
}

function Normalize-ServerUrl {
	param([string]$Url)
	if ([string]::IsNullOrWhiteSpace($Url)) {
		return "http://127.0.0.1:8085"
	}
	$value = $Url.Trim()
	if ($value -match "^\w+://") {
		return $value
	}
	return "http://$value"
}

function New-ReadinessCheck {
	param(
		[string]$Backend,
		[string]$State,
		[string]$Name,
		[string]$Detail = "",
		[object]$Data = $null
	)
	if ($State -eq "WARN") {
		$script:Warnings++
	}
	$payload = [ordered]@{
		Backend = $Backend
		State = $State
		Name = $Name
		Detail = $Detail
	}
	if ($null -ne $Data) {
		$payload.Data = $Data
	}
	return [pscustomobject]$payload
}

function Test-MusicGenReadiness {
	$script = Join-Path $scriptRoot "generate-musicgen-hf.ps1"
	$args = @(
		"-SmokeTest",
		"-Json",
		"-AllowMissingDeps",
		"-Model",
		$Model,
		"-Device",
		$Device
	)
	if ($LoadModel) {
		$args += "-LoadModel"
	}

	$output = @()
	$exitCode = 0
	try {
		$output = & $script @args 2>&1 | ForEach-Object { "$_" }
		$exitCode = if ($null -ne $LASTEXITCODE) { $LASTEXITCODE } else { 0 }
	} catch {
		$output += "$_"
		$exitCode = 1
	}
	$text = $output -join "`n"
	try {
		$status = $text | ConvertFrom-Json
		$missing = @($status.missingDependencies)
		if ($status.passed) {
			return New-ReadinessCheck "musicgen-hf" "OK" "MusicGen Python stack" "dependencies ready" $status
		}
		$detail = if ($missing.Count -gt 0) {
			"missing optional Python dependencies: $($missing -join ', ')"
		} elseif (![string]::IsNullOrWhiteSpace([string]$status.error)) {
			[ string ]$status.error
		} else {
			"MusicGen readiness probe reported not ready"
		}
		return New-ReadinessCheck "musicgen-hf" "WARN" "MusicGen Python stack" $detail $status
	} catch {
		$detail = if ($exitCode -ne 0) {
			"MusicGen readiness probe failed with exit code $exitCode"
		} else {
			"MusicGen readiness probe did not return JSON"
		}
		if (![string]::IsNullOrWhiteSpace($text)) {
			$detail += ": $text"
		}
		return New-ReadinessCheck "musicgen-hf" "WARN" "MusicGen Python stack" $detail
	}
}

function Test-AceStepReadiness {
	$normalizedUrl = Normalize-ServerUrl $ServerUrl
	$healthUrl = $normalizedUrl.TrimEnd("/") + "/health"
	try {
		$response = Invoke-WebRequest `
			-Uri $healthUrl `
			-UseBasicParsing `
			-TimeoutSec ([Math]::Max(1, $TimeoutSeconds)) `
			-ErrorAction Stop
		$statusCode = [int]$response.StatusCode
		$data = [pscustomobject]@{
			ServerUrl = $normalizedUrl
			HealthUrl = $healthUrl
			StatusCode = $statusCode
			Body = ($response.Content | Out-String).Trim()
		}
		if ($statusCode -ge 200 -and $statusCode -lt 300) {
			return New-ReadinessCheck "acestep" "OK" "AceStep server health" "server reachable" $data
		}
		return New-ReadinessCheck "acestep" "WARN" "AceStep server health" "unexpected HTTP status $statusCode" $data
	} catch {
		$data = [pscustomobject]@{
			ServerUrl = $normalizedUrl
			HealthUrl = $healthUrl
		}
		return New-ReadinessCheck "acestep" "WARN" "AceStep server health" $_.Exception.Message $data
	}
}

function Write-ReadinessText {
	param([object[]]$Checks)
	Write-Host "Generation readiness"
	foreach ($check in $Checks) {
		$line = "{0,-5} {1} [{2}]" -f $check.State, $check.Name, $check.Backend
		if (![string]::IsNullOrWhiteSpace($check.Detail)) {
			$line += " - $($check.Detail)"
		}
		Write-Host $line
	}
	if ($script:Warnings -eq 0) {
		Write-Host "Generation readiness passed."
	} else {
		Write-Host "Generation readiness found $script:Warnings warning(s)."
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$backendName = Normalize-Backend $Backend
$checks = @()

if ($backendName -eq "all" -or $backendName -eq "musicgen-hf") {
	$checks += Test-MusicGenReadiness
}
if ($backendName -eq "all" -or $backendName -eq "acestep") {
	$checks += Test-AceStepReadiness
}

if ($Json) {
	[pscustomobject]@{
		Name = "ofxGgmlMusic generation readiness"
		Root = $addonRoot.Path
		Warnings = $script:Warnings
		Checks = $checks
	} | ConvertTo-Json -Depth 8
} else {
	Write-ReadinessText $checks
}

if ($Strict -and $script:Warnings -gt 0) {
	exit 1
}
