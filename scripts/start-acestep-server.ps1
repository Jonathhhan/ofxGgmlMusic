param(
	[string]$ServerExecutable = $(if ($env:OFXGGML_ACESTEP_SERVER_EXE) { $env:OFXGGML_ACESTEP_SERVER_EXE } else { "" }),
	[string[]]$ServerArguments = @(),
	[string]$ModelPath = $(if ($env:OFXGGML_ACESTEP_MODEL_PATH) { $env:OFXGGML_ACESTEP_MODEL_PATH } else { "" }),
	[string]$ServerUrl = $(if ($env:OFXGGML_ACESTEP_SERVER_URL) { $env:OFXGGML_ACESTEP_SERVER_URL } else { "" }),
	[string]$HostName = "127.0.0.1",
	[int]$Port = 8085,
	[string]$PythonExecutable = $(if ($env:OFXGGML_ACESTEP_PYTHON) { $env:OFXGGML_ACESTEP_PYTHON } elseif ($env:OFXGGML_PYTHON) { $env:OFXGGML_PYTHON } elseif ($IsLinux -or $IsMacOS) { "python3" } else { "python" }),
	[string]$WorkDir = "",
	[string]$LogDir = "",
	[int]$StartupTimeoutSeconds = 30,
	[bool]$Detached = $true,
	[switch]$ForceNew,
	[switch]$NoHealthCheck,
	[switch]$DryRun
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$libAcestepRoot = Join-Path $addonRoot "libs\\acestep"
$acestepBinRoot = Join-Path $libAcestepRoot "bin"

function Normalize-ServerUrl {
	param([string]$Url, [string]$DefaultHost, [int]$DefaultPort)
	if ([string]::IsNullOrWhiteSpace($Url)) {
		return "http://$DefaultHost`:$DefaultPort"
	}
	$url = $Url.Trim()
	if ($url -match "^\w+://") {
		return $url
	}
	return "http://$url"
}

function Resolve-UrlParts {
	param([string]$Url, [string]$DefaultHost, [int]$DefaultPort)
	$normalized = Normalize-ServerUrl $Url $DefaultHost $DefaultPort
	try {
		$uri = [Uri]$normalized
		return [pscustomobject]@{
			Url = $normalized
			Host = $uri.Host
			Port = if ($uri.Port -gt 0) { $uri.Port } else { $DefaultPort }
		}
	} catch {
		throw "Could not parse server URL: $normalized"
	}
}

function Quote-Argument {
	param([string]$Value)
	if ($Value -match '[\s"]') {
		return ('"' + $Value.Replace('"', '\"') + '"')
	}
	return $Value
}

function Join-ProcessArguments {
	param([string[]]$Arguments)
	return ($Arguments | ForEach-Object { Quote-Argument $_ }) -join " "
}

function Split-ProcessArgumentText {
	param([string]$Text)
	if ([string]::IsNullOrWhiteSpace($Text)) {
		return @()
	}

	$arguments = New-Object System.Collections.Generic.List[string]
	$current = New-Object System.Text.StringBuilder
	$quote = [char]0

	foreach ($char in $Text.ToCharArray()) {
		if ($quote -ne [char]0) {
			if ($char -eq $quote) {
				$quote = [char]0
			} else {
				[void]$current.Append($char)
			}
			continue
		}
		if ($char -eq '"' -or $char -eq "'") {
			$quote = $char
			continue
		}
		if ([char]::IsWhiteSpace($char)) {
			if ($current.Length -gt 0) {
				$arguments.Add($current.ToString())
				[void]$current.Clear()
			}
			continue
		}
		[void]$current.Append($char)
	}
	if ($quote -ne [char]0) {
		throw "Unclosed quote in OFXGGML_ACESTEP_SERVER_ARGS."
	}
	if ($current.Length -gt 0) {
		$arguments.Add($current.ToString())
	}
	return @($arguments)
}

function Has-Arg {
	param([string[]]$Arguments,[string]$Name)
	foreach ($argument in $Arguments) {
		if ($argument -eq $Name -or $argument -like "$Name=*") {
			return $true
		}
	}
	return $false
}

function Test-ServerHealth {
	param([string]$HealthUrl)
	$result = [ordered]@{
		Reachable = $false
		Ready = $false
		StatusCode = 0
		Message = ""
	}
	try {
		$response = Invoke-WebRequest -Uri $HealthUrl -UseBasicParsing -TimeoutSec 2 -ErrorAction Stop
		$result.Reachable = $true
		$result.StatusCode = [int]$response.StatusCode
		$result.Ready = ($response.StatusCode -ge 200 -and $response.StatusCode -lt 300)
		$result.Message = ($response.Content | Out-String).Trim()
	} catch {
		if ($_.Exception.Response) {
			$result.Reachable = $true
			$result.StatusCode = [int]$_.Exception.Response.StatusCode
			$result.Message = $_.Exception.Message
		} else {
			$result.Message = $_.Exception.Message
		}
	}
	return [pscustomobject]$result
}

function Wait-ForServerReady {
	param([string]$HealthUrl, [int]$TimeoutSeconds)
	if ($TimeoutSeconds -le 0) {
		return Test-ServerHealth $HealthUrl
	}
	$deadline = (Get-Date).AddSeconds([Math]::Max(1, $TimeoutSeconds))
	$health = $null
	while ((Get-Date) -lt $deadline) {
		$health = Test-ServerHealth $HealthUrl
		if ($health.Ready) {
			return $health
		}
		Start-Sleep -Milliseconds 500
	}
	return Test-ServerHealth $HealthUrl
}

function Resolve-EnvArgs {
	param([string[]]$CurrentArgs)
	if ($CurrentArgs.Count -gt 0) {
		return $CurrentArgs
	}
	$raw = $env:OFXGGML_ACESTEP_SERVER_ARGS
	if ([string]::IsNullOrWhiteSpace($raw)) {
		return @()
	}
	return @(Split-ProcessArgumentText $raw)
}

function Resolve-Executable {
	param([string]$Executable)
	if ([string]::IsNullOrWhiteSpace($Executable)) {
		$scriptPaths = @(
			(Join-Path $acestepBinRoot "ace-server.exe"),
			(Join-Path $acestepBinRoot "ace-server"),
			(Join-Path $libAcestepRoot "ace-server.exe"),
			(Join-Path $libAcestepRoot "ace-server")
		)

		foreach ($path in $scriptPaths) {
			if (Test-Path -LiteralPath $path -PathType Leaf) {
				return (Resolve-Path -LiteralPath $path).Path
			}
		}

		throw "Could not determine AceStep executable. Set OFXGGML_ACESTEP_SERVER_EXE, pass -ServerExecutable, or build ace-server into libs/acestep/bin."
	}
	if (Test-Path -LiteralPath $Executable -PathType Leaf) {
		return (Resolve-Path -LiteralPath $Executable).Path
	}
	return $Executable
}

function Resolve-PythonExecutable {
	param([string]$Candidate)
	if (Test-Path -LiteralPath $Candidate -PathType Leaf) {
		return (Resolve-Path -LiteralPath $Candidate).Path
	}
	$command = Get-Command $Candidate -ErrorAction SilentlyContinue
	if ($null -ne $command) {
		return $command.Source
	}
	throw "Could not find Python executable: $Candidate"
}

function Resolve-DefaultModelDirectory {
	$localCandidates = @(
		(Join-Path $acestepBinRoot "models"),
		(Join-Path $libAcestepRoot "models"),
		(Join-Path $libAcestepRoot "source/models"),
		(Join-Path $addonRoot "ofxGgmlMusicAceStepExample/bin/data/models"),
		(Join-Path $addonRoot "models/acestep"),
		(Join-Path $addonRoot "data/models/acestep")
	)

	foreach ($candidate in $localCandidates) {
		if ((Test-Path -LiteralPath $candidate -PathType Container) -and
			(Get-ChildItem -Path $candidate -File -Filter *.gguf -ErrorAction SilentlyContinue)) {
			return (Resolve-Path -LiteralPath $candidate).Path
		}
	}

	foreach ($candidate in $localCandidates) {
		if (Test-Path -LiteralPath $candidate -PathType Container) {
			return (Resolve-Path -LiteralPath $candidate).Path
		}
	}

	return ""
}

function Get-ModelArgName {
	param([string]$Executable)
	$ext = [System.IO.Path]::GetExtension($Executable).ToLowerInvariant()
	if ($ext -eq ".py") {
		return "--model"
	}
	return "--models"
}

$resolvedUrl = Resolve-UrlParts $ServerUrl $HostName $Port
$resolvedUrlValue = $resolvedUrl.Url
$hostFromUrl = $resolvedUrl.Host
$portFromUrl = $resolvedUrl.Port
$healthUrl = $resolvedUrlValue.TrimEnd("/") + "/health"

$ServerArguments = Resolve-EnvArgs $ServerArguments
$arguments = $ServerArguments
$resolvedExecutable = Resolve-Executable $ServerExecutable
$modelArg = Get-ModelArgName $resolvedExecutable
if (-not (Has-Arg $arguments "--host")) {
	$arguments += @("--host", $hostFromUrl)
}
if (-not (Has-Arg $arguments "--port")) {
	$arguments += @("--port", [string]$portFromUrl)
}

$effectiveModelPath = if ([string]::IsNullOrWhiteSpace($ModelPath)) {
	Resolve-DefaultModelDirectory
} else {
	$ModelPath
}
$modelArgAliases = @("--model", "--model-path", "--models")
if (-not [string]::IsNullOrWhiteSpace($effectiveModelPath) -and -not ($modelArgAliases | Where-Object { Has-Arg $arguments $_ } | Select-Object -First 1)) {
	$arguments += @($modelArg, $effectiveModelPath)
}

$workingDir = if ([string]::IsNullOrWhiteSpace($WorkDir)) {
	[System.IO.Path]::GetDirectoryName($resolvedExecutable)
} else {
	$WorkDir
}

$launchExe = $resolvedExecutable
$launchArguments = $arguments
$resolvedExecutableExt = [System.IO.Path]::GetExtension($resolvedExecutable).ToLowerInvariant()
if ($resolvedExecutableExt -eq ".py") {
	if (!$DryRun) {
		$launchExe = Resolve-PythonExecutable $PythonExecutable
	} else {
		$launchExe = $PythonExecutable
	}
	$launchArguments = @($resolvedExecutable) + $launchArguments
}

Write-Host "Starting AceStep server"
Write-Host "  exe:       $launchExe"
Write-Host "  script:    $resolvedExecutable"
Write-Host "  args:      $(Join-ProcessArguments $launchArguments)"
if ($workingDir) {
	Write-Host "  workdir:   $workingDir"
}
Write-Host "  url:       $resolvedUrlValue"

if ($DryRun) {
	Write-Output "OFXGGML_ACESTEP_SERVER_DRY_RUN=1"
	Write-Output "OFXGGML_ACESTEP_SERVER_URL=$resolvedUrlValue"
	Write-Output "OFXGGML_ACESTEP_SERVER_COMMAND=$launchExe $(Join-ProcessArguments $launchArguments)"
	return
}

if (!$NoHealthCheck) {
	$health = Test-ServerHealth $healthUrl
	if ($health.Ready -and !$ForceNew) {
		Write-Host "AceStep server already healthy at $resolvedUrlValue. Reusing running server."
		Write-Output "OFXGGML_ACESTEP_SERVER_URL=$resolvedUrlValue"
		Write-Output "OFXGGML_ACESTEP_SERVER_STARTED=0"
		return
	}
}

if ($Detached) {
	if (![string]::IsNullOrWhiteSpace($LogDir)) {
		New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
		$logPath = Join-Path $LogDir "acestep-server.command.txt"
		Set-Content -LiteralPath $logPath -Value ("`"$launchExe`" " + (Join-ProcessArguments $launchArguments))
	}

	$startParams = @{
		FilePath = $launchExe
		ArgumentList = (Join-ProcessArguments $launchArguments)
		WorkingDirectory = $workingDir
		PassThru = $true
		ErrorAction = "Stop"
	}
	if ($IsWindows) {
		$startParams.WindowStyle = "Hidden"
	}
	$process = Start-Process @startParams
	Write-Host "AceStep server started in background (PID $($process.Id))."
	Write-Output "OFXGGML_ACESTEP_SERVER_PID=$($process.Id)"
	Write-Output "OFXGGML_ACESTEP_SERVER_STARTED=1"
	Write-Output "OFXGGML_ACESTEP_SERVER_URL=$resolvedUrlValue"

	if (!$NoHealthCheck) {
		Write-Host "Waiting up to $StartupTimeoutSeconds seconds for AceStep server readiness..."
		$ready = Wait-ForServerReady $healthUrl $StartupTimeoutSeconds
		if ($ready.Ready) {
			Write-Host "AceStep server is ready at $resolvedUrlValue"
		} else {
			$detail = if ($ready.Reachable) {
				"HTTP $($ready.StatusCode) $($ready.Message)"
			} else {
				$ready.Message
			}
			throw "AceStep server did not become ready at $resolvedUrlValue within $StartupTimeoutSeconds seconds. $detail"
		}
	}
} else {
	Write-Host "Running AceStep server in foreground. Press Ctrl+C to stop."
	& $launchExe @launchArguments
}
