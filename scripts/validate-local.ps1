param()

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Assert-Path {
	param(
		[string]$Path,
		[string]$Label,
		[switch]$Directory
	)

	if ($Directory) {
		if (!(Test-Path -LiteralPath $Path -PathType Container)) {
			throw "$Label was not found: $Path"
		}
	} elseif (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
		throw "$Label was not found: $Path"
	}
}

function Assert-FileContains {
	param(
		[string]$Path,
		[string]$Pattern,
		[string]$Label
	)

	$content = Get-Content -LiteralPath $Path -Raw
	if ($content -notmatch $Pattern) {
		throw "$Label did not contain expected pattern: $Pattern"
	}
}
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Split-Path -Parent $scriptRoot
$addonsRoot = Split-Path -Parent $addonRoot

Write-Step "Checking addon skeleton"
Assert-Path (Join-Path $addonRoot "addon_config.mk") "addon config"
Assert-Path (Join-Path $addonRoot "README.md") "README"
Assert-FileContains (Join-Path $addonRoot "README.md") "melody, bass, pulse, and mix stems" "README shared stem list"
Assert-Path (Join-Path $addonRoot "docs\MUSIC_WORKFLOWS.md") "music workflow docs"
Assert-FileContains (Join-Path $addonRoot "README.md") "docs/MUSIC_WORKFLOWS.md" "README"
Assert-FileContains (Join-Path $addonRoot "docs\MUSIC_WORKFLOWS.md") "Planning handoff" "music workflow docs"
Assert-FileContains (Join-Path $addonRoot "docs\MUSIC_WORKFLOWS.md") "Validation ladder" "music workflow docs"
Assert-FileContains (Join-Path $addonRoot "docs\MUSIC_WORKFLOWS.md") "generated artifacts" "music workflow docs"
Assert-Path (Join-Path $addonRoot "LICENSE") "license"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic.h") "public header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusicVersion.h") "version header"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlMusic.h") "ofxGgmlMusicVersion.h" "public header"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlMusicVersion.h") "OFXGGML_MUSIC_VERSION_STRING" "version header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicAudioUtils.h") "audio utils header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicAudioUtils.cpp") "audio utils source"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicMidiUtils.h") "midi utils header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicMidiUtils.cpp") "midi utils source"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicTypes.h") "types header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicAceStepBridge.h") "AceStep bridge header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicAceStepBridge.cpp") "AceStep bridge source"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicGenerationBackend.h") "generation backend header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicGenerationBackend.cpp") "generation backend source"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicExternalGenerationBackend.h") "external generation backend header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicExternalGenerationBackend.cpp") "external generation backend source"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicProceduralGenerationBackend.h") "procedural generation backend header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicProceduralGenerationBackend.cpp") "procedural generation backend source"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicUtils.h") "utility header"
Assert-Path (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicUtils.cpp") "utility source"
Assert-Path (Join-Path $addonRoot "tools\ofxGgmlMusicGenerate\CMakeLists.txt") "generation CLI CMakeLists"
Assert-Path (Join-Path $addonRoot "tools\ofxGgmlMusicGenerate\main.cpp") "generation CLI source"
Assert-Path (Join-Path $addonRoot "tools\ofxGgmlMusicExternalGenerate\CMakeLists.txt") "external generation CLI CMakeLists"
Assert-Path (Join-Path $addonRoot "tools\ofxGgmlMusicExternalGenerate\main.cpp") "external generation CLI source"
Assert-Path (Join-Path $addonRoot "tools\musicgen_hf_runner.py") "Hugging Face MusicGen runner"
Assert-Path (Join-Path $scriptRoot "generate-procedural-music.ps1") "procedural generation script"
Assert-Path (Join-Path $scriptRoot "generate-procedural-music.bat") "procedural generation batch script"
Assert-Path (Join-Path $scriptRoot "generate-procedural-music.sh") "procedural generation shell script"
Assert-Path (Join-Path $scriptRoot "generate-external-music.ps1") "external generation script"
Assert-Path (Join-Path $scriptRoot "generate-external-music.bat") "external generation batch script"
Assert-Path (Join-Path $scriptRoot "generate-external-music.sh") "external generation shell script"
Assert-Path (Join-Path $scriptRoot "generate-musicgen-hf.ps1") "Hugging Face MusicGen generation script"
Assert-Path (Join-Path $scriptRoot "generate-musicgen-hf.bat") "Hugging Face MusicGen batch script"
Assert-Path (Join-Path $scriptRoot "generate-musicgen-hf.sh") "Hugging Face MusicGen shell script"
Assert-Path (Join-Path $scriptRoot "start-acestep-server.ps1") "AceStep server launch script"
Assert-Path (Join-Path $scriptRoot "start-acestep-server.bat") "AceStep server launch batch script"
Assert-Path (Join-Path $scriptRoot "start-acestep-server.sh") "AceStep server launch shell script"
Assert-Path (Join-Path $scriptRoot "test-acestep-server-dry-run.ps1") "AceStep server dry-run script"
Assert-Path (Join-Path $scriptRoot "test-acestep-server-dry-run.bat") "AceStep server dry-run batch script"
Assert-Path (Join-Path $scriptRoot "test-acestep-server-dry-run.sh") "AceStep server dry-run shell script"
Assert-Path (Join-Path $scriptRoot "setup-acestep-server.ps1") "AceStep server setup script"
Assert-Path (Join-Path $scriptRoot "setup-acestep-server.bat") "AceStep server setup batch script"
Assert-Path (Join-Path $scriptRoot "setup-acestep-server.sh") "AceStep server setup shell script"
Assert-Path (Join-Path $scriptRoot "test-acestep-setup-dry-run.ps1") "AceStep setup dry-run script"
Assert-Path (Join-Path $scriptRoot "test-acestep-setup-dry-run.bat") "AceStep setup dry-run batch script"
Assert-Path (Join-Path $scriptRoot "test-acestep-setup-dry-run.sh") "AceStep setup dry-run shell script"
Assert-Path (Join-Path $scriptRoot "run-musicgen-hf.bat") "Hugging Face MusicGen Windows runner"
Assert-Path (Join-Path $scriptRoot "run-musicgen-hf.sh") "Hugging Face MusicGen shell runner"
Assert-Path (Join-Path $scriptRoot "doctor-music.ps1") "Music doctor script"
Assert-Path (Join-Path $scriptRoot "doctor-music.bat") "Music doctor Windows wrapper"
Assert-Path (Join-Path $scriptRoot "doctor-music.sh") "Music doctor shell wrapper"
Assert-Path (Join-Path $scriptRoot "test-doctor-music.ps1") "Music doctor smoke test"
Assert-Path (Join-Path $scriptRoot "run-music-runtime-smoke.ps1") "Music runtime smoke script"
Assert-Path (Join-Path $scriptRoot "run-music-runtime-smoke.bat") "Music runtime smoke Windows wrapper"
Assert-Path (Join-Path $scriptRoot "run-music-runtime-smoke.sh") "Music runtime smoke shell wrapper"
Assert-Path (Join-Path $scriptRoot "test-music-runtime-smoke.ps1") "Music runtime smoke contract test"
Assert-Path (Join-Path $scriptRoot "test-external-generation-contract.ps1") "external generation contract script"
Assert-Path (Join-Path $scriptRoot "test-external-generation-contract.bat") "external generation contract batch script"
Assert-Path (Join-Path $scriptRoot "test-external-generation-contract.sh") "external generation contract shell script"
Assert-Path (Join-Path $scriptRoot "build-music-example.ps1") "Music example build script"
Assert-Path (Join-Path $scriptRoot "build-music-example.bat") "Music example build batch script"
Assert-Path (Join-Path $scriptRoot "build-music-example.sh") "Music example build shell script"
Assert-Path (Join-Path $scriptRoot "test-example-project-repair.ps1") "Music example repair test"
Assert-FileContains (Join-Path $scriptRoot "build-music-example.ps1") "ofxGgmlMusicAceStepExample" "Music example build script default"
Assert-FileContains (Join-Path $scriptRoot "build-music-example.ps1") "Repair-VisualStudioProjectFile" "Music generated project repair"
Assert-FileContains (Join-Path $addonRoot "addon_config.mk") "libs/\*/\.source/%" "addon source cache exclusion"

Write-Step "Checking dependency layout"
Assert-Path (Join-Path $addonsRoot "ofxGgmlCore") "sibling ofxGgmlCore addon" -Directory
Assert-Path (Join-Path $addonsRoot "ofxImGui") "sibling ofxImGui addon for examples" -Directory

Write-Step "Checking example layout"
$exampleRoot = Join-Path $addonRoot "ofxGgmlMusicAnalysisExample"
Assert-Path $exampleRoot "root-level smoke example" -Directory
Assert-Path (Join-Path $exampleRoot "addons.make") "smoke example addons.make"
Assert-FileContains (Join-Path $exampleRoot "addons.make") "(?m)^ofxImGui\s*$" "smoke example addons.make"
Assert-Path (Join-Path $exampleRoot "src\main.cpp") "smoke example main.cpp"
Assert-Path (Join-Path $exampleRoot "src\ofApp.h") "smoke example ofApp.h"
Assert-Path (Join-Path $exampleRoot "src\ofApp.cpp") "smoke example ofApp.cpp"

$generationExampleRoot = Join-Path $addonRoot "ofxGgmlMusicGenerationExample"
Assert-Path $generationExampleRoot "root-level generation example" -Directory
Assert-Path (Join-Path $generationExampleRoot "addons.make") "generation example addons.make"
Assert-FileContains (Join-Path $generationExampleRoot "addons.make") "(?m)^ofxImGui\s*$" "generation example addons.make"
Assert-Path (Join-Path $generationExampleRoot "README.md") "generation example README"
Assert-Path (Join-Path $generationExampleRoot "src\main.cpp") "generation example main.cpp"
Assert-Path (Join-Path $generationExampleRoot "src\ofApp.h") "generation example ofApp.h"
Assert-Path (Join-Path $generationExampleRoot "src\ofApp.cpp") "generation example ofApp.cpp"
Assert-FileContains (Join-Path $generationExampleRoot "src\ofApp.cpp") "getGenerationPresetNames" "generation example preset source"
Assert-FileContains (Join-Path $generationExampleRoot "src\ofApp.cpp") "getGenerationStemNames" "generation example stem source"
Assert-FileContains (Join-Path $generationExampleRoot "src\ofApp.cpp") "getGenerationKeyTonics" "generation example key tonic source"
Assert-FileContains (Join-Path $generationExampleRoot "src\ofApp.cpp") "getGenerationKeyModes" "generation example key mode source"

$aceStepExampleRoot = Join-Path $addonRoot "ofxGgmlMusicAceStepExample"
Assert-Path $aceStepExampleRoot "root-level AceStep music example" -Directory
Assert-Path (Join-Path $aceStepExampleRoot "addons.make") "AceStep example addons.make"
Assert-FileContains (Join-Path $aceStepExampleRoot "addons.make") "(?m)^ofxImGui\s*$" "AceStep example addons.make"
Assert-Path (Join-Path $aceStepExampleRoot "README.md") "AceStep example README"
Assert-Path (Join-Path $aceStepExampleRoot "src\main.cpp") "AceStep example main.cpp"
Assert-Path (Join-Path $aceStepExampleRoot "src\ofApp.h") "AceStep example ofApp.h"
Assert-Path (Join-Path $aceStepExampleRoot "src\ofApp.cpp") "AceStep example ofApp.cpp"
Assert-FileContains (Join-Path $aceStepExampleRoot "src\ofApp.cpp") "ofxGgmlMusicAceStepBridge" "AceStep example bridge usage"
Assert-FileContains (Join-Path $aceStepExampleRoot "src\ofApp.cpp") "Start server" "AceStep example server launcher"
Assert-FileContains (Join-Path $aceStepExampleRoot "src\ofApp.cpp") "start-acestep-server.ps1" "AceStep example launch script"
Assert-FileContains (Join-Path $aceStepExampleRoot "src\ofApp.cpp") "bin.*data.*models" "AceStep example model lookup"
Assert-FileContains (Join-Path $aceStepExampleRoot "README.md") "AceStep-compatible local server" "AceStep example README"
Assert-FileContains (Join-Path $aceStepExampleRoot "README.md") "Start server" "AceStep example README"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicAceStepBridge.cpp") "output_format" "AceStep output format request"
Assert-FileContains (Join-Path $addonRoot "src\ofxGgmlMusic\ofxGgmlMusicAceStepBridge.cpp") "multipart data without an audio part" "AceStep multipart audio extraction"
Assert-Path (Join-Path $addonRoot "tests\CMakeLists.txt") "test CMakeLists"
Assert-Path (Join-Path $addonRoot "tests\test_main.cpp") "test source"
Assert-Path (Join-Path $addonRoot "tests\test_external_generation_contract.cpp") "external generation contract source"

$nestedExamples = Join-Path $addonRoot "examples"
if (Test-Path -LiteralPath $nestedExamples -PathType Container) {
	throw "Examples should live at the addon root, not under: $nestedExamples"
}

Write-Step "Checking generated artifact hygiene"
$forbidden = @(
	"build",
	".vs",
	"ofxGgmlMusicAnalysisExample\bin",
	"ofxGgmlMusicAnalysisExample\obj",
	"ofxGgmlMusicAnalysisExample\.vs",
	"ofxGgmlMusicGenerationExample\bin",
	"ofxGgmlMusicGenerationExample\obj",
	"ofxGgmlMusicGenerationExample\.vs",
	"ofxGgmlMusicAceStepExample\bin",
	"ofxGgmlMusicAceStepExample\obj",
	"ofxGgmlMusicAceStepExample\.vs",
	"models"
)

$trackedFiles = @(& git -C $addonRoot ls-files 2>$null)
if ($LASTEXITCODE -ne 0) {
	throw "Could not inspect tracked files for artifact hygiene."
}
foreach ($relative in $forbidden) {
	$prefix = ($relative -replace '\\', '/').TrimEnd('/') + "/"
	$exact = ($relative -replace '\\', '/').TrimEnd('/')
	$trackedMatch = $trackedFiles | Where-Object {
		$normalized = $_ -replace '\\', '/'
		$normalized -eq $exact -or $normalized.StartsWith($prefix)
	} | Select-Object -First 1
	if ($trackedMatch) {
		throw "Generated or local-only path is tracked and should not be committed here: $trackedMatch"
	}
}

Write-Step "Checking Music doctor"
& (Join-Path $scriptRoot "test-doctor-music.ps1")
if (!$?) {
	throw "Music doctor smoke test failed"
}

Write-Step "Checking Music runtime smoke contract"
& (Join-Path $scriptRoot "test-music-runtime-smoke.ps1")

Write-Step "Running headless tests"
& (Join-Path $scriptRoot "test-addon.ps1")
if ($LASTEXITCODE -ne 0) {
	throw "Headless tests failed with exit code $LASTEXITCODE"
}

Write-Step "Checking external generation contract dry run"
& (Join-Path $scriptRoot "test-external-generation-contract.ps1") -DryRun
if ($LASTEXITCODE -ne 0) {
	throw "External generation contract dry run failed with exit code $LASTEXITCODE"
}

Write-Step "Checking external generation script dry runs"
$externalDryRun = & (Join-Path $scriptRoot "generate-external-music.ps1") `
	-Executable "mock-generator.exe" `
	-Model "mock-model-id" `
	-AllowModelId `
	-DryRun 2>&1 6>&1 | Out-String
if (!$externalDryRun.Contains("External music generation plan") -or
	!$externalDryRun.Contains("allow model id: ON") -or
	!$externalDryRun.Contains("Dry run complete; no files were changed")) {
	throw "External generation dry-run output was unexpected:`n$externalDryRun"
}
$musicGenDryRun = & (Join-Path $scriptRoot "generate-musicgen-hf.ps1") -DryRun 2>&1 6>&1 | Out-String
if (!$musicGenDryRun.Contains("External music generation plan") -or
	!$musicGenDryRun.Contains("facebook/musicgen-small") -or
	!$musicGenDryRun.Contains("Dry run complete; no files were changed")) {
	throw "MusicGen HF dry-run output was unexpected:`n$musicGenDryRun"
}
$acestepDryRun = & (Join-Path $scriptRoot "start-acestep-server.ps1") `
	-ServerExecutable "mock-acestep-server.exe" `
	-ServerUrl "http://127.0.0.1:8085" `
	-DryRun 2>&1 6>&1 | Out-String
if (!$acestepDryRun.Contains("OFXGGML_ACESTEP_SERVER_DRY_RUN=1") -or
	!$acestepDryRun.Contains("OFXGGML_ACESTEP_SERVER_URL=http://127.0.0.1:8085") -or
	!$acestepDryRun.Contains("OFXGGML_ACESTEP_SERVER_COMMAND")) {
	throw "AceStep server dry-run output was unexpected:`n$acestepDryRun"
}

Write-Step "Checking AceStep setup dry-run contract"
& (Join-Path $scriptRoot "test-acestep-setup-dry-run.ps1")
if ($LASTEXITCODE -ne 0) {
	throw "AceStep setup dry-run contract failed with exit code $LASTEXITCODE"
}

Write-Step "Checking AceStep server dry-run contract"
& (Join-Path $scriptRoot "test-acestep-server-dry-run.ps1")
if ($LASTEXITCODE -ne 0) {
	throw "AceStep server dry-run contract failed with exit code $LASTEXITCODE"
}

Write-Step "Checking generated example project repair"
& (Join-Path $scriptRoot "test-example-project-repair.ps1")
if ($LASTEXITCODE -ne 0) {
	throw "Music example project repair failed with exit code $LASTEXITCODE"
}

Write-Step "Checking procedural generation CLI"
$scratchDir = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusic-cli-smoke"
if (Test-Path -LiteralPath $scratchDir) {
	Remove-Item -LiteralPath $scratchDir -Recurse -Force
}
New-Item -ItemType Directory -Path $scratchDir | Out-Null
$cliOutput = Join-Path $scratchDir "procedural.wav"
& (Join-Path $scriptRoot "generate-procedural-music.ps1") `
	-Preset lofi `
	-Prompt "loopable validation motif" `
	-Output $cliOutput `
	-Duration 1.0 `
	-Tempo 100 `
	-Key D `
	-Mode major `
	-Stem @("melody", "bass") `
	-BuildDir (Join-Path $scratchDir "build") `
	-Clean
if ($LASTEXITCODE -ne 0) {
	throw "Procedural generation CLI failed with exit code $LASTEXITCODE"
}
Assert-Path $cliOutput "procedural generation CLI wav"
Assert-Path ($cliOutput + ".json") "procedural generation CLI manifest"
$historyPath = Join-Path $scratchDir "ofxGgmlMusic-history.json"
Assert-Path $historyPath "procedural generation CLI history"
Assert-Path (Join-Path $scratchDir "procedural-melody.mid") "procedural generation CLI melody midi"
Assert-Path (Join-Path $scratchDir "procedural-chords.mid") "procedural generation CLI chord midi"
Assert-Path (Join-Path $scratchDir "procedural-arrangement.mid") "procedural generation CLI arrangement midi"
Assert-Path (Join-Path $scratchDir "procedural-melody.wav") "procedural generation CLI melody stem"
Assert-Path (Join-Path $scratchDir "procedural-bass.wav") "procedural generation CLI bass stem"
$cliExe = if (!($IsLinux -or $IsMacOS)) {
	Join-Path $scratchDir "build\ofxGgmlMusicGenerate.exe"
} else {
	Join-Path $scratchDir "build\ofxGgmlMusicGenerate"
}
& $cliExe --list-presets
if ($LASTEXITCODE -ne 0) {
	throw "Procedural generation preset listing failed with exit code $LASTEXITCODE"
}
$presetJson = & $cliExe --list-presets --json
if ($LASTEXITCODE -ne 0 -or ($presetJson -join "`n") -notmatch '"presets"' -or ($presetJson -join "`n") -notmatch '"ambient"') {
	throw "Procedural generation JSON preset listing failed"
}
& $cliExe --describe-preset lofi
if ($LASTEXITCODE -ne 0) {
	throw "Procedural generation preset description failed with exit code $LASTEXITCODE"
}
$presetDescriptionJson = & $cliExe --describe-preset lofi --json
if ($LASTEXITCODE -ne 0 -or ($presetDescriptionJson -join "`n") -notmatch '"preset": "lofi"' -or ($presetDescriptionJson -join "`n") -notmatch '"tempoBpm": 76') {
	throw "Procedural generation JSON preset description failed"
}
& $cliExe --list-stems
if ($LASTEXITCODE -ne 0) {
	throw "Procedural generation stem listing failed with exit code $LASTEXITCODE"
}
$stemJson = & $cliExe --list-stems --json
if ($LASTEXITCODE -ne 0 -or ($stemJson -join "`n") -notmatch '"stems"' -or ($stemJson -join "`n") -notmatch '"mix"') {
	throw "Procedural generation JSON stem listing failed"
}
& $cliExe --list-keys
if ($LASTEXITCODE -ne 0) {
	throw "Procedural generation key listing failed with exit code $LASTEXITCODE"
}
$keyJson = & $cliExe --list-keys --json
if ($LASTEXITCODE -ne 0 -or ($keyJson -join "`n") -notmatch '"tonics"' -or ($keyJson -join "`n") -notmatch '"modes"' -or ($keyJson -join "`n") -notmatch '"minor"') {
	throw "Procedural generation JSON key listing failed"
}
& $cliExe --preset ambient --prompt "invalid duration" --output (Join-Path $scratchDir "invalid-duration.wav") --duration not-a-number
if ($LASTEXITCODE -eq 0) {
	throw "Procedural generation CLI accepted an invalid duration"
}
& $cliExe --preset ambient --prompt "invalid tempo" --output (Join-Path $scratchDir "invalid-tempo.wav") --tempo not-a-number
if ($LASTEXITCODE -eq 0) {
	throw "Procedural generation CLI accepted an invalid tempo"
}
& $cliExe --preset ambient --prompt "zero tempo" --output (Join-Path $scratchDir "zero-tempo.wav") --tempo 0
if ($LASTEXITCODE -eq 0) {
	throw "Procedural generation CLI accepted a zero tempo"
}
& $cliExe --preset ambient --prompt "zero duration" --output (Join-Path $scratchDir "zero-duration.wav") --duration 0
if ($LASTEXITCODE -eq 0) {
	throw "Procedural generation CLI accepted a zero duration"
}
& $cliExe --preset ambient --prompt "invalid seed" --output (Join-Path $scratchDir "invalid-seed.wav") --seed not-an-int
if ($LASTEXITCODE -eq 0) {
	throw "Procedural generation CLI accepted an invalid seed"
}
& $cliExe --preset ambient --prompt "invalid stem" --output (Join-Path $scratchDir "invalid-stem.wav") --stem not-a-stem
if ($LASTEXITCODE -eq 0) {
	throw "Procedural generation CLI accepted an invalid stem"
}
& $cliExe --preset ambient --prompt "invalid key" --output (Join-Path $scratchDir "invalid-key.wav") --key H
if ($LASTEXITCODE -eq 0) {
	throw "Procedural generation CLI accepted an invalid key"
}
& $cliExe --preset ambient --prompt "invalid mode" --output (Join-Path $scratchDir "invalid-mode.wav") --mode dorian
if ($LASTEXITCODE -eq 0) {
	throw "Procedural generation CLI accepted an invalid mode"
}
& $cliExe --prune-history $historyPath --keep not-an-int
if ($LASTEXITCODE -eq 0) {
	throw "Procedural generation CLI accepted an invalid keep count"
}
& $cliExe --inspect ($cliOutput + ".json")
if ($LASTEXITCODE -ne 0) {
	throw "Procedural generation manifest inspection failed with exit code $LASTEXITCODE"
}
$inspectJson = & $cliExe --inspect ($cliOutput + ".json") --json
if ($LASTEXITCODE -ne 0 -or ($inspectJson -join "`n") -notmatch '"outputPath"') {
	throw "Procedural generation JSON inspection failed"
}
& $cliExe --history $historyPath
if ($LASTEXITCODE -ne 0) {
	throw "Procedural generation history inspection failed with exit code $LASTEXITCODE"
}
$historyJson = & $cliExe --history $historyPath --json
if ($LASTEXITCODE -ne 0 -or ($historyJson -join "`n") -notmatch '"manifests"') {
	throw "Procedural generation JSON history inspection failed"
}
$secondOutput = Join-Path $scratchDir "procedural-two.wav"
& $cliExe `
	--preset pulse `
	--prompt "second validation motif" `
	--output $secondOutput `
	--duration 1.0 `
	--tempo 110 `
	--key A `
	--mode minor
if ($LASTEXITCODE -ne 0) {
	throw "Second procedural generation CLI run failed with exit code $LASTEXITCODE"
}
Assert-Path $secondOutput "second procedural generation CLI wav"
Assert-Path ($secondOutput + ".json") "second procedural generation CLI manifest"
& $cliExe --prune-history $historyPath --keep 1
if ($LASTEXITCODE -ne 0) {
	throw "Procedural generation history prune failed with exit code $LASTEXITCODE"
}
if (Test-Path -LiteralPath $cliOutput) {
	throw "Procedural generation prune kept an older wav unexpectedly: $cliOutput"
}
Assert-Path $secondOutput "newest procedural generation CLI wav after prune"
& $cliExe --history $historyPath
if ($LASTEXITCODE -ne 0) {
	throw "Procedural generation history inspection after prune failed with exit code $LASTEXITCODE"
}
$pruneJson = & $cliExe --prune-history $historyPath --keep 1 --json
if ($LASTEXITCODE -ne 0 -or ($pruneJson -join "`n") -notmatch '"kept"') {
	throw "Procedural generation JSON prune failed"
}

Write-Step "Checking external generation CLI with procedural generator"
$externalCliOutput = Join-Path $scratchDir "external-cli.wav"
& (Join-Path $scriptRoot "generate-external-music.ps1") `
	-Executable $cliExe `
	-Prompt "external bridge validation motif" `
	-Output $externalCliOutput `
	-Duration 1.0 `
	-Seed 321 `
	-ExtraArgument @("--preset", "ambient", "--tempo", "100", "--key", "C", "--mode", "major") `
	-BuildDir (Join-Path $scratchDir "external-build") `
	-Clean
if ($LASTEXITCODE -ne 0) {
	throw "External generation CLI failed with exit code $LASTEXITCODE"
}
Assert-Path $externalCliOutput "external generation CLI wav"
Assert-Path ($externalCliOutput + ".json") "external generation CLI manifest"
Remove-Item -LiteralPath $scratchDir -Recurse -Force

Write-Step "Running external generation contract"
& (Join-Path $scriptRoot "test-external-generation-contract.ps1") `
	-BuildRoot (Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusic-external-contract") `
	-Clean
if ($LASTEXITCODE -ne 0) {
	throw "External generation contract failed with exit code $LASTEXITCODE"
}

Write-Step "ofxGgmlMusic local validation passed"
