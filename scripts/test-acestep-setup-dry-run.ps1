param()

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Assert-Contains {
	param(
		[string]$Text,
		[string]$Needle,
		[string]$Label
	)
	if (!$Text.Contains($Needle)) {
		throw "$Label did not contain expected text: $Needle`n$Text"
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$script = Join-Path $scriptRoot "setup-acestep-server.ps1"
$addonRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptRoot ".."))
$addonsRoot = [System.IO.Path]::GetFullPath((Join-Path $addonRoot ".."))
$coreRoot = Join-Path $addonsRoot "ofxGgmlCore"

Write-Step "acestep default dry-run"
$defaultOutput = & $script -DryRun 2>&1 6>&1 | Out-String
Assert-Contains $defaultOutput "Dry run: acestep.cpp server setup plan" "default dry-run"
Assert-Contains $defaultOutput "revision: remote default" "default dry-run"
Assert-Contains $defaultOutput "git clone --recurse-submodules --depth 1" "default dry-run"
Assert-Contains $defaultOutput "mode: Auto" "default dry-run"
Assert-Contains $defaultOutput "external BLAS: OFF" "default dry-run"
Assert-Contains $defaultOutput "-DGGML_BLAS=OFF" "default dry-run"
Assert-Contains $defaultOutput "ggml: bundled" "default dry-run"
Assert-Contains $defaultOutput "cmake --build" "default dry-run"
Assert-Contains $defaultOutput "output:" "default dry-run"
Assert-Contains $defaultOutput "Dry run complete; no files were changed" "default dry-run"
if ($defaultOutput.Contains("--branch main")) {
	throw "default dry-run should not hardcode the removed upstream main branch:`n$defaultOutput"
}

Write-Step "acestep pinned revision dry-run"
$revisionOutput = & $script -DryRun -Revision "master" 2>&1 6>&1 | Out-String
Assert-Contains $revisionOutput "revision: master" "pinned revision dry-run"
Assert-Contains $revisionOutput "--branch master" "pinned revision dry-run"
Assert-Contains $revisionOutput "Dry run complete; no files were changed" "pinned revision dry-run"

Write-Step "acestep CPU-only dry-run"
$cpuOutput = & $script -DryRun -CpuOnly 2>&1 6>&1 | Out-String
Assert-Contains $cpuOutput "mode: CpuOnly" "CPU-only dry-run"
Assert-Contains $cpuOutput "CPU=ON CUDA=OFF" "CPU-only dry-run"
Assert-Contains $cpuOutput "Vulkan=OFF Metal=OFF" "CPU-only dry-run"
Assert-Contains $cpuOutput "external BLAS: OFF" "CPU-only dry-run"
Assert-Contains $cpuOutput "Dry run complete; no files were changed" "CPU-only dry-run"

Write-Step "acestep BLAS opt-in dry-run"
$blasOutput = & $script -DryRun -CpuOnly -Blas 2>&1 6>&1 | Out-String
Assert-Contains $blasOutput "external BLAS: ON" "BLAS opt-in dry-run"
Assert-Contains $blasOutput "-DGGML_BLAS=ON" "BLAS opt-in dry-run"
Assert-Contains $blasOutput "Dry run complete; no files were changed" "BLAS opt-in dry-run"

Write-Step "acestep bundled ggml dry-run"
$fakeCore = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusic-acestep-missing-core"
if (Test-Path -LiteralPath $fakeCore) {
	Remove-Item -LiteralPath $fakeCore -Recurse -Force
}
$bundleOutput = & $script -DryRun -BundledGgml -OfxGgmlCorePath $fakeCore 2>&1 6>&1 | Out-String
Assert-Contains $bundleOutput "ggml: bundled" "bundled ggml dry-run"
Assert-Contains $bundleOutput "-BundledGgml requested" "bundled ggml dry-run"
Assert-Contains $bundleOutput "cmake --build" "bundled ggml dry-run"
Assert-Contains $bundleOutput "Dry run complete; no files were changed" "bundled ggml dry-run"

if (Test-Path -LiteralPath $coreRoot -PathType Container) {
	Write-Step "acestep ofxGgmlCore ggml opt-in dry-run"
	$coreOutput = & $script -DryRun -UseCoreGgml 2>&1 6>&1 | Out-String
	Assert-Contains $coreOutput "ggml: ofxGgmlCore source" "ofxGgmlCore ggml dry-run"
	Assert-Contains $coreOutput "ofxGgmlCore:" "ofxGgmlCore ggml dry-run"
	Assert-Contains $coreOutput "Dry run complete; no files were changed" "ofxGgmlCore ggml dry-run"
}

Write-Step "acestep setup dry-run coverage passed"
