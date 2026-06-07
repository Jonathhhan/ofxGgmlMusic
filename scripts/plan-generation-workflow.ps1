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
	[string]$ServerExecutable = $(if ($env:OFXGGML_ACESTEP_SERVER_EXE) { $env:OFXGGML_ACESTEP_SERVER_EXE } else { "" }),
	[string]$ModelPath = $(if ($env:OFXGGML_ACESTEP_MODEL_PATH) { $env:OFXGGML_ACESTEP_MODEL_PATH } else { "" }),
	[string]$ServerUrl = $(if ($env:OFXGGML_ACESTEP_SERVER_URL) { $env:OFXGGML_ACESTEP_SERVER_URL } else { "http://127.0.0.1:8085" }),
	[switch]$Json
)

$ErrorActionPreference = "Stop"

function Test-WindowsHost {
	return !($IsLinux -or $IsMacOS)
}

function Convert-ToPlanArgument {
	param([string]$Value)
	if ($null -eq $Value) {
		return '""'
	}
	if ($Value -notmatch '[\s"]') {
		return $Value
	}
	return '"' + ($Value -replace '"', '\"') + '"'
}

function Join-PlanCommand {
	param([string[]]$Parts)
	return ($Parts | ForEach-Object { Convert-ToPlanArgument $_ }) -join " "
}

function Get-WrapperPath {
	param([string]$BaseName)
	$extension = if (Test-WindowsHost) { ".bat" } else { ".sh" }
	return Join-Path $scriptRoot ($BaseName + $extension)
}

function Normalize-Backend {
	param([string]$Value)
	$normalized = $Value.ToLowerInvariant()
	if ($normalized -eq "musicgenhf") {
		return "musicgen-hf"
	}
	return $normalized
}

function New-MusicGenPlan {
	$musicGenOutput = if ([string]::IsNullOrWhiteSpace($Output)) {
		Join-Path $addonRoot "ofxGgmlMusicGenerationExample\bin\data\outputs\musicgen-hf.wav"
	} else {
		$Output
	}
	$script = Get-WrapperPath "generate-musicgen-hf"
	$readiness = @(
		(Join-PlanCommand @($script, "-SmokeTest", "-Json", "-AllowMissingDeps")),
		(Join-PlanCommand @($script, "-SmokeTest", "-LoadModel", "-Json"))
	)
	$generation = Join-PlanCommand @(
		$script,
		"-Prompt", $Prompt,
		"-Duration", ([string]$Duration),
		"-Output", $musicGenOutput,
		"-Model", $Model,
		"-Seed", ([string]$Seed),
		"-Guidance", ([string]$Guidance)
	)
	if (![string]::IsNullOrWhiteSpace($NegativePrompt)) {
		$generation += " " + (Join-PlanCommand @("-NegativePrompt", $NegativePrompt))
	}
	if ($Tempo -gt 0.0) {
		$generation += " " + (Join-PlanCommand @("-Tempo", ([string]$Tempo)))
	}
	if (![string]::IsNullOrWhiteSpace($Key)) {
		$generation += " " + (Join-PlanCommand @("-Key", $Key))
	}
	if (![string]::IsNullOrWhiteSpace($Mode)) {
		$generation += " " + (Join-PlanCommand @("-Mode", $Mode))
	}

	return [pscustomobject]@{
		backend = "musicgen-hf"
		name = "Hugging Face MusicGen"
		summary = "Optional Transformers MusicGen runner through the external generation bridge."
		model = $Model
		output = $musicGenOutput
		environment = @(
			"OFXGGML_MUSIC_PYTHON"
		)
		commands = [pscustomobject]@{
			readiness = $readiness
			generate = $generation
			dryRun = Join-PlanCommand @($script, "-DryRun", "-Model", $Model)
		}
		artifacts = @(
			$musicGenOutput,
			($musicGenOutput + ".json"),
			(Join-Path (Split-Path -Parent $musicGenOutput) "ofxGgmlMusic-history.json")
		)
		nextSteps = @(
			"Run the readiness command before loading a model.",
			"Use -LoadModel only when model cache access or downloads are acceptable.",
			"Generated audio and manifests stay outside git."
		)
	}
}

function New-AceStepPlan {
	$aceOutput = if ([string]::IsNullOrWhiteSpace($Output)) {
		Join-Path $addonRoot "ofxGgmlMusicAceStepExample\bin\data\generated\acestep"
	} else {
		$Output
	}
	$setupScript = Get-WrapperPath "setup-acestep-server"
	$startScript = Get-WrapperPath "start-acestep-server"
	$exampleBuild = Get-WrapperPath "build-music-example"
	$startParts = @($startScript, "-ServerUrl", $ServerUrl)
	if (![string]::IsNullOrWhiteSpace($ServerExecutable)) {
		$startParts += @("-ServerExecutable", $ServerExecutable)
	}
	if (![string]::IsNullOrWhiteSpace($ModelPath)) {
		$startParts += @("-ModelPath", $ModelPath)
	}

	return [pscustomobject]@{
		backend = "acestep"
		name = "ACE-Step local server"
		summary = "Local ACE-Step server workflow used by ofxGgmlMusicAceStepExample."
		serverUrl = $ServerUrl
		serverExecutable = $ServerExecutable
		modelPath = $ModelPath
		output = $aceOutput
		environment = @(
			"OFXGGML_ACESTEP_SERVER_EXE",
			"OFXGGML_ACESTEP_SERVER_URL",
			"OFXGGML_ACESTEP_MODEL_PATH",
			"OFXGGML_ACESTEP_SERVER_ARGS",
			"OFXGGML_ACESTEP_AUTOSTART"
		)
		commands = [pscustomobject]@{
			setupDryRun = Join-PlanCommand @($setupScript, "-DryRun")
			startDryRun = Join-PlanCommand ($startParts + "-DryRun")
			startServer = Join-PlanCommand $startParts
			buildExample = Join-PlanCommand @($exampleBuild, "-Example", "ofxGgmlMusicAceStepExample", "-RepairOnly")
		}
		artifacts = @(
			$aceOutput,
			($aceOutput + "\*.wav"),
			($aceOutput + "\*.wav.json")
		)
		nextSteps = @(
			"Run the setup dry-run to inspect source/build choices.",
			"Start the server before opening ofxGgmlMusicAceStepExample.",
			"Set OFXGGML_ACESTEP_AUTOSTART=0 to keep the example from launching a server."
		)
	}
}

function Write-PlanText {
	param([object[]]$Plans)
	Write-Host "Generation workflow plan"
	foreach ($plan in $Plans) {
		Write-Host ""
		Write-Host "Backend: $($plan.name) [$($plan.backend)]"
		Write-Host "  summary: $($plan.summary)"
		if ($plan.model) {
			Write-Host "  model: $($plan.model)"
		}
		if ($plan.serverUrl) {
			Write-Host "  server url: $($plan.serverUrl)"
		}
		if ($plan.serverExecutable) {
			Write-Host "  server executable: $($plan.serverExecutable)"
		}
		if ($plan.modelPath) {
			Write-Host "  model path: $($plan.modelPath)"
		}
		Write-Host "  output: $($plan.output)"
		Write-Host "  environment:"
		foreach ($name in $plan.environment) {
			Write-Host "    - $name"
		}
		Write-Host "  commands:"
		foreach ($property in $plan.commands.PSObject.Properties) {
			$value = $property.Value
			if ($value -is [array]) {
				foreach ($entry in $value) {
					Write-Host "    $($property.Name): $entry"
				}
			} else {
				Write-Host "    $($property.Name): $value"
			}
		}
		Write-Host "  artifacts:"
		foreach ($artifact in $plan.artifacts) {
			Write-Host "    - $artifact"
		}
		Write-Host "  next steps:"
		foreach ($step in $plan.nextSteps) {
			Write-Host "    - $step"
		}
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$backendName = Normalize-Backend $Backend
$plans = @()

if ($backendName -eq "all" -or $backendName -eq "musicgen-hf") {
	$plans += New-MusicGenPlan
}
if ($backendName -eq "all" -or $backendName -eq "acestep") {
	$plans += New-AceStepPlan
}

if ($Json) {
	[pscustomobject]@{
		name = "ofxGgmlMusic generation workflow plan"
		prompt = $Prompt
		duration = $Duration
		seed = $Seed
		plans = $plans
	} | ConvertTo-Json -Depth 8
	return
}

Write-PlanText $plans
