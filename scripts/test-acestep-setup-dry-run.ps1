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
Assert-Contains $defaultOutput "mode: Auto" "default dry-run"
if (Test-Path -LiteralPath $coreRoot -PathType Container) {
	Assert-Contains $defaultOutput "ggml: ofxGgmlCore source" "default dry-run"
} else {
	Assert-Contains $defaultOutput "ggml: bundled" "default dry-run"
}
Assert-Contains $defaultOutput "cmake --build" "default dry-run"
Assert-Contains $defaultOutput "output:" "default dry-run"
Assert-Contains $defaultOutput "Dry run complete; no files were changed" "default dry-run"

Write-Step "acestep CPU-only dry-run"
$cpuOutput = & $script -DryRun -CpuOnly 2>&1 6>&1 | Out-String
Assert-Contains $cpuOutput "mode: CpuOnly" "CPU-only dry-run"
Assert-Contains $cpuOutput "CPU=ON CUDA=OFF" "CPU-only dry-run"
Assert-Contains $cpuOutput "Vulkan=OFF Metal=OFF" "CPU-only dry-run"
Assert-Contains $cpuOutput "Dry run complete; no files were changed" "CPU-only dry-run"

Write-Step "acestep bundled ggml fallback dry-run"
$fakeCore = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlMusic-acestep-missing-core"
if (Test-Path -LiteralPath $fakeCore) {
	Remove-Item -LiteralPath $fakeCore -Recurse -Force
}
$bundleOutput = & $script -DryRun -BundledGgml -OfxGgmlCorePath $fakeCore 2>&1 6>&1 | Out-String
Assert-Contains $bundleOutput "ggml: bundled" "bundled fallback dry-run"
Assert-Contains $bundleOutput "cmake --build" "bundled fallback dry-run"
Assert-Contains $bundleOutput "Dry run complete; no files were changed" "bundled fallback dry-run"

Write-Step "acestep setup dry-run coverage passed"
