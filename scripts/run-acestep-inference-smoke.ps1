param(
	[string]$ServerUrl = $(if ($env:OFXGGML_ACESTEP_SERVER_URL) { $env:OFXGGML_ACESTEP_SERVER_URL } else { "http://127.0.0.1:8085" }),
	[string]$Prompt = "short warm ambient electronic pulse, sparse and clean",
	[double]$Duration = 4.0,
	[int]$Seed = 42,
	[string]$ModelPath = $(if ($env:OFXGGML_ACESTEP_MODEL_PATH) { $env:OFXGGML_ACESTEP_MODEL_PATH } else { "" }),
	[string]$AudioOutput = "",
	[Alias("OutputPath")]
	[string]$ReportPath = "",
	[int]$TimeoutSeconds = 1200,
	[switch]$DryRun,
	[switch]$Json,
	[switch]$SummaryOnly
)

$ErrorActionPreference = "Stop"

function Normalize-ServerUrl {
	param([string]$Url)
	$value = $Url.Trim()
	if ($value -notmatch "^\w+://") { $value = "http://$value" }
	return $value.TrimEnd("/")
}

function Write-Report {
	param([string]$Path, [object]$Payload)
	if ([string]::IsNullOrWhiteSpace($Path)) { return }
	$target = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $addonRoot $Path }
	$directory = Split-Path -Parent $target
	if (![string]::IsNullOrWhiteSpace($directory) -and !(Test-Path -LiteralPath $directory -PathType Container)) {
		New-Item -ItemType Directory -Path $directory -Force | Out-Null
	}
	[System.IO.File]::WriteAllText($target, ($Payload | ConvertTo-Json -Depth 8))
}

function Wait-AceStepJob {
	param([string]$JobId, [int]$Timeout)
	$deadline = (Get-Date).AddSeconds($Timeout)
	do {
		$status = Invoke-RestMethod -Uri "$serverRoot/job?id=$JobId" -TimeoutSec 30
		if ($status.status -eq "done") { return $status }
		if ($status.status -in @("failed", "cancelled")) {
			throw "AceStep job $JobId ended as $($status.status): $($status.error)"
		}
		Start-Sleep -Milliseconds 250
	} while ((Get-Date) -lt $deadline)
	throw "AceStep job $JobId timed out after $Timeout seconds"
}

function Find-ByteSequence {
	param([byte[]]$Bytes, [byte[]]$Pattern, [int]$Start = 0)
	for ($i = $Start; $i -le $Bytes.Length - $Pattern.Length; $i++) {
		$matched = $true
		for ($j = 0; $j -lt $Pattern.Length; $j++) {
			if ($Bytes[$i + $j] -ne $Pattern[$j]) { $matched = $false; break }
		}
		if ($matched) { return $i }
	}
	return -1
}

function Extract-Wav {
	param([string]$RawPath, [string]$WavPath)
	$bytes = [System.IO.File]::ReadAllBytes($RawPath)
	$riffStart = Find-ByteSequence -Bytes $bytes -Pattern ([byte[]](82, 73, 70, 70))
	if ($riffStart -lt 0 -or $riffStart + 12 -gt $bytes.Length) {
		throw "AceStep synth response did not contain a RIFF WAV payload"
	}
	$waveMarker = [System.Text.Encoding]::ASCII.GetString($bytes, $riffStart + 8, 4)
	if ($waveMarker -ne "WAVE") { throw "AceStep RIFF payload was not WAVE audio" }
	$wavLength = [int64][BitConverter]::ToUInt32($bytes, $riffStart + 4) + 8
	if ($wavLength -le 44 -or $riffStart + $wavLength -gt $bytes.Length) {
		throw "AceStep WAV payload length was invalid"
	}
	$wavBytes = New-Object byte[] $wavLength
	[Array]::Copy($bytes, $riffStart, $wavBytes, 0, $wavLength)
	[System.IO.File]::WriteAllBytes($WavPath, $wavBytes)

	$channels = [BitConverter]::ToUInt16($wavBytes, 22)
	$sampleRate = [BitConverter]::ToUInt32($wavBytes, 24)
	$bitsPerSample = [BitConverter]::ToUInt16($wavBytes, 34)
	$dataMarker = [byte[]](100, 97, 116, 97)
	$dataStart = Find-ByteSequence -Bytes $wavBytes -Pattern $dataMarker -Start 12
	if ($dataStart -lt 0 -or $dataStart + 8 -gt $wavBytes.Length) {
		throw "AceStep WAV payload did not contain a data chunk"
	}
	$dataBytes = [BitConverter]::ToUInt32($wavBytes, $dataStart + 4)
	$bytesPerSecond = [double]$sampleRate * [double]$channels * ([double]$bitsPerSample / 8.0)
	$durationSeconds = if ($bytesPerSecond -gt 0) { [double]$dataBytes / $bytesPerSecond } else { 0.0 }
	if ($channels -lt 1 -or $sampleRate -lt 8000 -or $bitsPerSample -lt 8 -or $durationSeconds -le 0.25) {
		throw "AceStep WAV payload failed audio invariants"
	}
	return [pscustomobject]@{
		Bytes = $wavBytes.Length
		Channels = $channels
		SampleRate = $sampleRate
		BitsPerSample = $bitsPerSample
		DurationSeconds = [Math]::Round($durationSeconds, 3)
	}
}

function Test-AceStepCudaProcess {
	$command = Get-Command nvidia-smi -ErrorAction SilentlyContinue
	if (!$command) { return $false }
	$previous = $ErrorActionPreference
	try {
		$ErrorActionPreference = "Continue"
		$output = & $command.Source --query-compute-apps=process_name --format=csv,noheader 2>$null
		$exitCode = $LASTEXITCODE
	} finally {
		$ErrorActionPreference = $previous
	}
	return $exitCode -eq 0 -and (($output -join "\n") -match "ace-server")
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($ModelPath)) {
	$ModelPath = Join-Path $addonRoot "ofxGgmlMusicAceStepExample\bin\data\models"
}
$serverRoot = Normalize-ServerUrl $ServerUrl
if ([string]::IsNullOrWhiteSpace($AudioOutput)) {
	$AudioOutput = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusic-acestep-inference.wav"
}
$AudioOutput = [System.IO.Path]::GetFullPath($AudioOutput)
$rawOutput = $AudioOutput + ".response"
$started = Get-Date

if ($DryRun) {
	$summary = [ordered]@{
		Passed = $false
		Ready = (Test-Path -LiteralPath $ModelPath -PathType Container)
		InferenceChecked = $false
		ModelBacked = $true
		SmokeKind = "model-backed-acestep-music-generation"
		Backend = "acestep-server"
		ModelPath = $ModelPath
		ServerUrl = $serverRoot
		Prompt = $Prompt
		DurationRequested = $Duration
		Seed = $Seed
		AudioOutput = $AudioOutput
		Error = ""
	}
	$payload = [ordered]@{ SummaryOnly = [bool]$SummaryOnly; Summary = $summary }
	Write-Report -Path $ReportPath -Payload $payload
	if ($Json) { $payload | ConvertTo-Json -Depth 6 } else { $summary }
	return
}

try {
	$health = Invoke-RestMethod -Uri "$serverRoot/health" -TimeoutSec 10
	if ([string]$health.status -ne "ok") { throw "AceStep health did not report ok" }

	$request = [ordered]@{
		caption = $Prompt
		lyrics = "[Instrumental]"
		bpm = 96
		duration = $Duration
		keyscale = "C major"
		timesignature = "4"
		vocal_language = ""
		seed = $Seed
		batch_size = 1
		lm_temperature = 0.85
		lm_cfg_scale = 2.0
		lm_top_p = 0.9
		lm_top_k = 0
		lm_negative_prompt = ""
		use_cot_caption = $true
		audio_codes = ""
		inference_steps = 0
		guidance_scale = 0.0
		shift = 0.0
		audio_cover_strength = 0.5
		repainting_start = -1
		repainting_end = -1
		lego = ""
		output_format = "wav16"
	}
	$lmResponse = Invoke-RestMethod -Uri "$serverRoot/lm" -Method Post -ContentType "application/json" -Body ($request | ConvertTo-Json) -TimeoutSec 240
	$lmJobId = [string]$lmResponse.id
	if ([string]::IsNullOrWhiteSpace($lmJobId)) { throw "AceStep /lm did not return a job id" }
	Wait-AceStepJob -JobId $lmJobId -Timeout $TimeoutSeconds | Out-Null
	$lmResult = Invoke-RestMethod -Uri "$serverRoot/job?id=$lmJobId&result=1" -TimeoutSec 60
	$lmItems = @($lmResult)
	if ($lmItems.Count -lt 1 -or [string]::IsNullOrWhiteSpace([string]$lmItems[0].audio_codes)) {
		throw "AceStep /lm result did not contain audio codes"
	}

	$synthBody = ConvertTo-Json -InputObject $lmItems -Depth 10
	$synthResponse = Invoke-RestMethod -Uri "$serverRoot/synth" -Method Post -ContentType "application/json" -Headers @{ Accept = "audio/wav" } -Body $synthBody -TimeoutSec 120
	$synthJobId = [string]$synthResponse.id
	if ([string]::IsNullOrWhiteSpace($synthJobId)) { throw "AceStep /synth did not return a job id" }
	Wait-AceStepJob -JobId $synthJobId -Timeout $TimeoutSeconds | Out-Null
	Invoke-WebRequest -Uri "$serverRoot/job?id=$synthJobId&result=1" -Headers @{ Accept = "audio/wav" } -OutFile $rawOutput -TimeoutSec 120

	$audioDirectory = Split-Path -Parent $AudioOutput
	if (!(Test-Path -LiteralPath $audioDirectory -PathType Container)) {
		New-Item -ItemType Directory -Path $audioDirectory -Force | Out-Null
	}
	$audio = Extract-Wav -RawPath $rawOutput -WavPath $AudioOutput
	$cudaDetected = Test-AceStepCudaProcess
	$summary = [ordered]@{
		Passed = $true
		Ready = $true
		InferenceChecked = $true
		ModelBacked = $true
		SmokeKind = "model-backed-acestep-music-generation"
		Backend = $(if ($cudaDetected) { "acestep-server-cuda" } else { "acestep-server" })
		ModelPath = $ModelPath
		CudaProcessDetected = $cudaDetected
		ServerUrl = $serverRoot
		Prompt = $Prompt
		DurationRequested = $Duration
		Seed = $Seed
		LmJobId = $lmJobId
		SynthJobId = $synthJobId
		AudioOutput = $AudioOutput
		AudioBytes = $audio.Bytes
		Channels = $audio.Channels
		SampleRate = $audio.SampleRate
		BitsPerSample = $audio.BitsPerSample
		DurationSeconds = $audio.DurationSeconds
		ElapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
		Error = ""
	}
	$payload = [ordered]@{ SummaryOnly = [bool]$SummaryOnly; Summary = $summary }
	Write-Report -Path $ReportPath -Payload $payload
	if ($Json) { $payload | ConvertTo-Json -Depth 6 } else { $summary }
} catch {
	$summary = [ordered]@{
		Passed = $false
		Ready = $false
		InferenceChecked = $false
		ModelBacked = $true
		SmokeKind = "model-backed-acestep-music-generation"
		Backend = "acestep-server"
		ModelPath = $ModelPath
		ServerUrl = $serverRoot
		Prompt = $Prompt
		DurationRequested = $Duration
		Seed = $Seed
		AudioOutput = $AudioOutput
		ElapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
		Error = $_.Exception.Message
	}
	$payload = [ordered]@{ SummaryOnly = [bool]$SummaryOnly; Summary = $summary }
	Write-Report -Path $ReportPath -Payload $payload
	if ($Json) { $payload | ConvertTo-Json -Depth 6 } else { $summary }
	exit 1
} finally {
	if (Test-Path -LiteralPath $rawOutput -PathType Leaf) {
		Remove-Item -LiteralPath $rawOutput -Force
	}
}
