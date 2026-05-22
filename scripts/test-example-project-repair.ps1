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

function Get-MsBuildNodeParts {
	param(
		[string]$Project,
		[string]$NodeName
	)
	[xml]$doc = Get-Content -LiteralPath $Project -Raw
	$namespace = New-Object System.Xml.XmlNamespaceManager($doc.NameTable)
	$namespace.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")
	$parts = New-Object System.Collections.Generic.List[string]
	foreach ($node in @($doc.SelectNodes("//msb:$NodeName", $namespace))) {
		foreach ($part in @($node.InnerText -split ";" | Where-Object { $_ })) {
			$parts.Add([string]$part)
		}
	}
	return @($parts)
}

function Assert-SemicolonNodeContains {
	param(
		[string]$Project,
		[string]$NodeName,
		[string]$Value
	)
	$parts = @(Get-MsBuildNodeParts -Project $Project -NodeName $NodeName)
	foreach ($part in $parts) {
		if ($part.Equals($Value, [System.StringComparison]::OrdinalIgnoreCase)) {
			return
		}
	}
	throw "$Project is missing $NodeName value: $Value"
}

function Assert-SemicolonNodePreservesMacro {
	param(
		[string]$Project,
		[string]$NodeName,
		[string]$Macro
	)
	$parts = @(Get-MsBuildNodeParts -Project $Project -NodeName $NodeName)
	if ($parts -contains $Macro) {
		return
	}
	throw "$Project does not preserve inherited $NodeName macro: $Macro"
}

function Get-CompilerOptions {
	param([string]$Project)
	[xml]$doc = Get-Content -LiteralPath $Project -Raw
	$namespace = New-Object System.Xml.XmlNamespaceManager($doc.NameTable)
	$namespace.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")
	$options = New-Object System.Collections.Generic.List[string]
	foreach ($node in @($doc.SelectNodes("//msb:ClCompile/msb:AdditionalOptions", $namespace))) {
		foreach ($option in @($node.InnerText -split "\s+" | Where-Object { $_ })) {
			$options.Add([string]$option)
		}
	}
	return @($options)
}

function Assert-CompilerOptionAbsent {
	param(
		[string]$Project,
		[string]$Option
	)
	foreach ($existingOption in @(Get-CompilerOptions -Project $Project)) {
		if ($existingOption.Equals($Option, [System.StringComparison]::OrdinalIgnoreCase)) {
			throw "$Project still contains compiler option: $Option"
		}
	}
}

function Assert-CompilerOptionOncePerNode {
	param(
		[string]$Project,
		[string]$Option
	)
	[xml]$doc = Get-Content -LiteralPath $Project -Raw
	$namespace = New-Object System.Xml.XmlNamespaceManager($doc.NameTable)
	$namespace.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")
	$nodes = @($doc.SelectNodes("//msb:ClCompile/msb:AdditionalOptions", $namespace))
	if ($nodes.Count -eq 0) {
		throw "$Project has no ClCompile AdditionalOptions nodes"
	}
	foreach ($node in $nodes) {
		$count = 0
		foreach ($existing in @($node.InnerText -split "\s+" | Where-Object { $_ })) {
			if ($existing.Equals($Option, [System.StringComparison]::OrdinalIgnoreCase)) {
				$count++
			}
		}
		if ($count -ne 1) {
			throw "$Project expected one $Option per AdditionalOptions node but found $count in: $($node.InnerText)"
		}
	}
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
Assert-IncludeDirectory -Project $aceProject -IncludeDirectory "..\..\ofxGgmlCore\libs\ggml\include"
Assert-SemicolonNodeContains -Project $aceProject -NodeName "AdditionalLibraryDirectories" -Value "..\..\ofxGgmlCore\libs\ggml\lib"
Assert-SemicolonNodeContains -Project $aceProject -NodeName "AdditionalLibraryDirectories" -Value '$(CUDA_PATH)\lib\x64'
foreach ($library in @("ggml.lib", "ggml-base.lib", "ggml-cpu.lib", "ggml-cuda.lib", "cublas.lib", "cudart.lib", "cuda.lib")) {
	Assert-SemicolonNodeContains -Project $aceProject -NodeName "AdditionalDependencies" -Value $library
}
Assert-SemicolonNodePreservesMacro -Project $aceProject -NodeName "AdditionalDependencies" -Macro "%(AdditionalDependencies)"
Assert-CompilerOptionAbsent -Project $aceProject -Option "-DOFXIMGUI_GLFW_EVENTS_REPLACE_OF_CALLBACKS=1"
Assert-CompilerOptionOncePerNode -Project $aceProject -Option "-DOFXIMGUI_GLFW_EVENTS_REPLACE_OF_CALLBACKS=0"

Write-Step "Music example project repair coverage passed"
