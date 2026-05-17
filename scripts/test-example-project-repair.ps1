param(
	[string]$Configuration = "Release",
	[string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Assert-IncludeDirectory {
	param(
		[string]$Project,
		[string]$IncludeDirectory
	)
	[xml]$doc = Get-Content -LiteralPath $Project -Raw
	$namespace = New-Object System.Xml.XmlNamespaceManager($doc.NameTable)
	$namespace.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")
	foreach ($node in @($doc.SelectNodes("//msb:AdditionalIncludeDirectories", $namespace))) {
		$parts = @($node.InnerText -split ";" | Where-Object { $_ })
		if ($parts -contains $IncludeDirectory) {
			return
		}
	}
	throw "$Project is missing include directory: $IncludeDirectory"
}

function Assert-CompileItem {
	param(
		[string]$Project,
		[string]$CompileItem
	)
	[xml]$doc = Get-Content -LiteralPath $Project -Raw
	$namespace = New-Object System.Xml.XmlNamespaceManager($doc.NameTable)
	$namespace.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")
	foreach ($node in @($doc.SelectNodes("//msb:ClCompile[@Include]", $namespace))) {
		if (([string]$node.Include).Equals($CompileItem, [System.StringComparison]::OrdinalIgnoreCase)) {
			return
		}
	}
	throw "$Project is missing compile item: $CompileItem"
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$examples = @(
	"ofxGgmlMusicAnalysisExample",
	"ofxGgmlMusicGenerationExample",
	"ofxGgmlMusicAceStepExample"
)

foreach ($example in $examples) {
	Write-Step "Repairing $example generated metadata"
	& (Join-Path $scriptRoot "build-music-example.ps1") `
		-Example $example `
		-Configuration $Configuration `
		-Platform $Platform `
		-RepairOnly
	if (!$?) {
		throw "$example project repair failed"
	}
	$project = Join-Path $addonRoot "$example\$example.vcxproj"
	Assert-IncludeDirectory -Project $project -IncludeDirectory "..\src"
	Assert-IncludeDirectory -Project $project -IncludeDirectory "..\..\ofxGgmlCore\src"
	Assert-IncludeDirectory -Project $project -IncludeDirectory "..\..\ofxImGui\src"
	Assert-CompileItem -Project $project -CompileItem "..\src\ofxGgmlMusic\ofxGgmlMusicGenerationBackend.cpp"
}

$aceProject = Join-Path $addonRoot "ofxGgmlMusicAceStepExample\ofxGgmlMusicAceStepExample.vcxproj"
Assert-CompileItem -Project $aceProject -CompileItem "..\src\ofxGgmlMusic\ofxGgmlMusicAceStepBridge.cpp"

Write-Step "Music example project repair coverage passed"
