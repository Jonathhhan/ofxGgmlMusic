param(
	[string]$Configuration = "Release",
	[string]$Platform = "x64",
	[string]$Example = "ofxGgmlMusicAceStepExample",
	[int]$Jobs = 1,
	[switch]$Clean,
	[switch]$RepairOnly,
	[switch]$ResetOfBuildState
)

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	Write-Host "==> $Message"
}

function Test-WindowsHost {
	return !($IsLinux -or $IsMacOS)
}

function Normalize-WindowsPathEnvironment {
	if (!(Test-WindowsHost)) {
		return
	}
	$variables = [Environment]::GetEnvironmentVariables("Process")
	$pathNames = New-Object System.Collections.Generic.List[string]
	foreach ($key in $variables.Keys) {
		$name = [string]$key
		if ($name.Equals("Path", [System.StringComparison]::OrdinalIgnoreCase)) {
			$pathNames.Add($name)
		}
	}
	if ($pathNames.Count -le 1) {
		return
	}

	$preferredName = if ($pathNames.Contains("Path")) { "Path" } else { $pathNames[0] }
	$pathValue = [string]$variables[$preferredName]
	if ([string]::IsNullOrWhiteSpace($pathValue)) {
		foreach ($name in $pathNames) {
			$value = [string]$variables[$name]
			if (![string]::IsNullOrWhiteSpace($value)) {
				$pathValue = $value
				break
			}
		}
	}
	foreach ($name in $pathNames) {
		if (!$name.Equals("Path", [System.StringComparison]::Ordinal)) {
			[Environment]::SetEnvironmentVariable($name, $null, "Process")
		}
	}
	[Environment]::SetEnvironmentVariable("Path", $pathValue, "Process")
}

function Invoke-CheckedNative {
	param(
		[string]$Step,
		[scriptblock]$Command
	)
	& $Command
	if ($LASTEXITCODE -ne 0) {
		throw "$Step failed with exit code $LASTEXITCODE"
	}
}

function Get-StableNameFragment {
	param([string]$Text)
	$sha1 = [System.Security.Cryptography.SHA1]::Create()
	try {
		$bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
		$hash = $sha1.ComputeHash($bytes)
		return [System.BitConverter]::ToString($hash).Replace("-", "")
	} finally {
		$sha1.Dispose()
	}
}

function Invoke-WithNamedMutex {
	param(
		[string]$Name,
		[scriptblock]$Command
	)
	$mutex = New-Object System.Threading.Mutex($false, $Name)
	$locked = $false
	try {
		$locked = $mutex.WaitOne([TimeSpan]::FromMinutes(30))
		if (!$locked) {
			throw "Timed out waiting for build lock: $Name"
		}
		& $Command
	} finally {
		if ($locked) {
			$mutex.ReleaseMutex()
		}
		$mutex.Dispose()
	}
}

function Get-MsBuild {
	$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
	if (Test-Path -LiteralPath $vswhere) {
		$installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath 2>$null
		if ($installPath) {
			$candidate = Join-Path $installPath "MSBuild\Current\Bin\MSBuild.exe"
			if (Test-Path -LiteralPath $candidate) {
				return $candidate
			}
		}
	}

	foreach ($version in @("18", "17", "16")) {
		foreach ($edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
			$candidate = "C:\Program Files\Microsoft Visual Studio\$version\$edition\MSBuild\Current\Bin\MSBuild.exe"
			if (Test-Path -LiteralPath $candidate) {
				return $candidate
			}
		}
	}
	return ""
}

function Resolve-BuildJobs {
	param([int]$RequestedJobs)
	if ($RequestedJobs -lt 0) {
		throw "-Jobs must be 0 or greater."
	}
	if ($RequestedJobs -eq 0) {
		return [Environment]::ProcessorCount
	}
	return $RequestedJobs
}

function Get-MsBuildParallelArguments {
	param([int]$BuildJobs)
	if ($BuildJobs -gt 1) {
		return @("/p:MultiProcessorCompilation=true", "/m:$BuildJobs")
	}
	return @("/p:MultiProcessorCompilation=false", "/m:1")
}

function Get-OfBuildStateDirectory {
	param(
		[string]$OfRoot,
		[string]$Platform,
		[string]$Configuration
	)
	return Join-Path $OfRoot "libs\openFrameworksCompiled\project\vs\obj\$Platform\$Configuration"
}

function Get-ActiveVisualStudioBuildProcesses {
	$names = @("MSBuild", "devenv")
	$processes = @()
	foreach ($name in $names) {
		$processes += @(Get-Process -Name $name -ErrorAction SilentlyContinue)
	}
	return @($processes | Sort-Object -Property ProcessName, Id)
}

function Write-OfBuildStatePreflightWarning {
	param([string]$BuildStateDir)
	if (!(Test-Path -LiteralPath $BuildStateDir -PathType Container)) {
		return
	}
	$processes = @(Get-ActiveVisualStudioBuildProcesses)
	if ($processes.Count -eq 0) {
		return
	}
	$summary = ($processes | ForEach-Object { "$($_.ProcessName):$($_.Id)" }) -join ", "
	Write-Warning "Active Visual Studio/MSBuild processes were found: $summary"
	Write-Warning "Shared openFrameworks build state may be locked: $BuildStateDir"
	Write-Warning "If MSBuild reports MSB3491/lastbuildstate access denied, close competing builds or rerun with -ResetOfBuildState."
}

function Reset-OfBuildState {
	param([string]$BuildStateDir)
	if (!(Test-Path -LiteralPath $BuildStateDir -PathType Container)) {
		Write-Step "No openFrameworks build-state directory to reset: $BuildStateDir"
		return
	}

	$resolvedStateDir = (Resolve-Path -LiteralPath $BuildStateDir).Path
	$targets = @(Get-ChildItem -LiteralPath $resolvedStateDir -Directory -Filter "openfram*.tlog" -ErrorAction SilentlyContinue)
	if ($targets.Count -eq 0) {
		Write-Step "No openFrameworks tlog folders matched for reset"
		return
	}

	foreach ($target in $targets) {
		if (!$target.FullName.StartsWith($resolvedStateDir, [System.StringComparison]::OrdinalIgnoreCase)) {
			throw "Refusing to reset path outside openFrameworks build state: $($target.FullName)"
		}
		Write-Step "Resetting openFrameworks build-state folder $($target.Name)"
		Remove-Item -LiteralPath $target.FullName -Recurse -Force
	}
}

function Invoke-MsBuildProject {
	param(
		[string]$MsBuild,
		[string]$Project,
		[string]$Target,
		[string]$Configuration,
		[string]$Platform,
		[string[]]$ParallelArguments
	)
	$arguments = @(
		$Project,
		"/t:$Target",
		"/p:Configuration=$Configuration",
		"/p:Platform=$Platform",
		"/p:TrackFileAccess=false"
	) + $ParallelArguments + @("/nr:false")
	$output = @(& $MsBuild @arguments 2>&1)
	$exitCode = $LASTEXITCODE
	foreach ($line in $output) {
		Write-Host $line
	}
	return [pscustomobject]@{
		ExitCode = $exitCode
		Output = ($output | ForEach-Object { [string]$_ }) -join "`n"
	}
}

function Test-OfBuildStateLockFailure {
	param([string]$Output)
	return $Output -match "MSB3491" -and
		$Output -match "lastbuildstate" -and
		$Output -match "openFrameworksCompiled" -and
		($Output -match "Access.*denied" -or $Output -match "Zugriff.*verweigert")
}

function Get-OfBuildStateLockFailureMessage {
	param([string]$BuildStateDir)
	return @(
		"MSBuild could not write the shared openFrameworks build state.",
		"Build-state directory: $BuildStateDir",
		"Close other Visual Studio/MSBuild builds that use this openFrameworks tree, then retry.",
		"To reset only ignored openFrameworks tlog folders for this platform/configuration, rerun with -ResetOfBuildState."
	) -join "`n"
}

function Ensure-GeneratedVisualStudioProject {
	param(
		[string]$ExampleName,
		[string]$ExamplePath,
		[string]$OfRoot
	)
	if (!(Test-WindowsHost)) {
		return
	}
	$project = Join-Path $ExamplePath "$ExampleName.vcxproj"
	if (Test-Path -LiteralPath $project -PathType Leaf) {
		return
	}

	$templateRoot = Join-Path $OfRoot "scripts\templates\winvs"
	$templateProject = Join-Path $templateRoot "emptyExample.vcxproj"
	if (!(Test-Path -LiteralPath $templateProject -PathType Leaf)) {
		throw "Visual Studio project not found and openFrameworks winvs template is missing: $templateProject"
	}
	$copies = @(
		@{ Source = "emptyExample.vcxproj"; Target = "$ExampleName.vcxproj" },
		@{ Source = "emptyExample.vcxproj.filters"; Target = "$ExampleName.vcxproj.filters" },
		@{ Source = "emptyExample.vcxproj.user"; Target = "$ExampleName.vcxproj.user" },
		@{ Source = "emptyExample.sln"; Target = "$ExampleName.sln" },
		@{ Source = "icon.rc"; Target = "icon.rc" }
	)
	foreach ($copy in $copies) {
		$source = Join-Path $templateRoot $copy.Source
		$target = Join-Path $ExamplePath $copy.Target
		if (!(Test-Path -LiteralPath $source -PathType Leaf)) {
			continue
		}
		$text = Get-Content -LiteralPath $source -Raw
		$text = $text.Replace("emptyExample", $ExampleName)
		Set-Content -LiteralPath $target -Value $text -NoNewline
	}
	Write-Step "Initialized generated Visual Studio project metadata from openFrameworks template"
}

function Get-RelativeProjectPath {
	param(
		[string]$ProjectDir,
		[string]$FilePath
	)
	$projectUri = [System.Uri]((Resolve-Path -LiteralPath $ProjectDir).Path.TrimEnd("\") + "\")
	$fileUri = [System.Uri](Resolve-Path -LiteralPath $FilePath).Path
	return [System.Uri]::UnescapeDataString(
		$projectUri.MakeRelativeUri($fileUri).ToString()).Replace("/", "\")
}

function Get-ExampleAddons {
	param([string]$ExampleRoot)
	$addonsMake = Join-Path $ExampleRoot "addons.make"
	if (!(Test-Path -LiteralPath $addonsMake -PathType Leaf)) {
		return @()
	}
	return Get-Content -LiteralPath $addonsMake |
		ForEach-Object { $_.Trim() } |
		Where-Object { $_ -and -not $_.StartsWith("#") }
}

function Get-AddonConfigValues {
	param([string]$AddonRoot)

	$values = @{}
	$configPath = Join-Path $AddonRoot "addon_config.mk"
	if (!(Test-Path -LiteralPath $configPath -PathType Leaf)) {
		return $values
	}
	$section = ""
	Get-Content -LiteralPath $configPath | ForEach-Object {
		$line = ([string]$_ -replace "\s+#.*$", "").Trim()
		if ([string]::IsNullOrWhiteSpace($line)) {
			return
		}
		if ($line -match '^([A-Za-z0-9_/]+):\s*$') {
			$section = $matches[1]
			return
		}
		if ($section -ne "common" -and $section -ne "vs") {
			return
		}
		if ($line -match '^(ADDON_[A-Z_]+)\s*(?:\+)?=\s*(.+)$') {
			$name = $matches[1]
			foreach ($part in @($matches[2] -split '\s+' | Where-Object { $_ })) {
				if (!$values.ContainsKey($name)) {
					$values[$name] = New-Object System.Collections.Generic.List[string]
				}
				if (!$values[$name].Contains($part)) {
					$values[$name].Add($part)
				}
			}
		}
	}
	return $values
}

function Get-AddonSourceExcludes {
	param([string]$AddonRoot)
	$configValues = Get-AddonConfigValues -AddonRoot $AddonRoot
	$excludePaths = New-Object System.Collections.Generic.List[string]
	foreach ($key in @("ADDON_SOURCES_EXCLUDE", "ADDON_INCLUDES_EXCLUDE")) {
		foreach ($path in @($configValues[$key])) {
			$normPath = ([string]$path).Trim() -replace '/', '\' -replace '%', '*'
			if ($normPath -and -not $excludePaths.Contains($normPath)) {
				$excludePaths.Add($normPath)
			}
		}
	}
	return @($excludePaths)
}

function Test-AddonSourceExcluded {
	param(
		[string]$RelativePath,
		[string[]]$ExcludePatterns
	)
	$normalized = $RelativePath -replace '/', '\'
	foreach ($pattern in $ExcludePatterns) {
		if ($normalized -like $pattern) {
			return $true
		}
	}
	return $false
}

function Get-AddonSourceRoots {
	param([string]$AddonRoot)
	$roots = New-Object System.Collections.Generic.List[string]
	foreach ($name in @("src", "libs")) {
		$candidate = Join-Path $AddonRoot $name
		if (Test-Path -LiteralPath $candidate -PathType Container) {
			$roots.Add($candidate)
		}
	}
	return @($roots)
}

function Get-AddonIncludes {
	param([string]$AddonRoot)
	$includePaths = New-Object System.Collections.Generic.List[string]
	$config = Get-AddonConfigValues -AddonRoot $AddonRoot
	$excludePaths = Get-AddonSourceExcludes -AddonRoot $AddonRoot
	foreach ($path in @($config["ADDON_INCLUDES"])) {
		$tempPath = ([string]$path).Trim()
		if ([string]::IsNullOrWhiteSpace($tempPath)) {
			continue
		}
		$fullPath = Join-Path $AddonRoot ($tempPath -replace '/', '\')
		if ((Test-Path -LiteralPath $fullPath -PathType Container) -and
			!(Test-AddonSourceExcluded -RelativePath $tempPath -ExcludePatterns $excludePaths)) {
			$includePaths.Add($fullPath)
		}
	}

	$srcRoot = Join-Path $AddonRoot "src"
	if (Test-Path -LiteralPath $srcRoot -PathType Container) {
		$includePaths.Add($srcRoot)
	}
	$libsRoot = Join-Path $AddonRoot "libs"
	if (Test-Path -LiteralPath $libsRoot -PathType Container) {
		Get-ChildItem -LiteralPath $libsRoot -Directory | ForEach-Object {
			$relative = Get-RelativeProjectPath -ProjectDir $AddonRoot -FilePath $_.FullName
			if (Test-AddonSourceExcluded -RelativePath $relative -ExcludePatterns $excludePaths) {
				return
			}
			$includePaths.Add($_.FullName)
			foreach ($subName in @("src", "include", "backends", "extras")) {
				$subDir = Join-Path $_.FullName $subName
				if (Test-Path -LiteralPath $subDir -PathType Container) {
					$includePaths.Add($subDir)
				}
			}
		}
	}
	$hash = New-Object "System.Collections.Generic.HashSet[string]" ([StringComparer]::OrdinalIgnoreCase)
	foreach ($candidate in $includePaths) {
		[void]$hash.Add([System.IO.Path]::GetFullPath($candidate))
	}
	return @($hash)
}

function Get-AddonLibs {
	param([string]$AddonRoot)
	$config = Get-AddonConfigValues -AddonRoot $AddonRoot
	return @($config["ADDON_LIBS"] | Where-Object { $_ })
}

function ConvertTo-ProjectLibraryReference {
	param(
		[string]$AddonRoot,
		[string]$ProjectDir,
		[string]$Library
	)

	$value = ([string]$Library).Trim().Trim('"')
	if ([string]::IsNullOrWhiteSpace($value)) {
		return $null
	}

	$normalized = $value -replace "/", "\"
	$parent = Split-Path -Parent $normalized
	$name = [System.IO.Path]::GetFileName($normalized)
	$directory = ""
	if ($parent) {
		if ($parent -match '^\$\(' -or [System.IO.Path]::IsPathRooted($parent)) {
			$directory = $parent
		} else {
			$libraryPath = Join-Path $AddonRoot $parent
			if (Test-Path -LiteralPath $libraryPath -PathType Container) {
				$directory = Get-RelativeProjectPath -ProjectDir $ProjectDir -FilePath $libraryPath
			}
		}
	}

	return [pscustomobject]@{
		Dependency = $name
		Directory = $directory
	}
}

function Get-AddonDefines {
	param([string[]]$AddonRoots)
	$defines = New-Object System.Collections.Generic.List[string]
	foreach ($root in $AddonRoots) {
		$config = Get-AddonConfigValues -AddonRoot $root
		foreach ($flag in @($config["ADDON_CFLAGS"])) {
			if ($flag -match '^-D(.+)$' -and !$defines.Contains($matches[1])) {
				$defines.Add($matches[1])
			}
		}
	}
	return @($defines)
}

function Get-FirstItemGroup {
	param(
		[xml]$Doc,
		[System.Xml.XmlNamespaceManager]$Namespace,
		[string]$PreferredTag
	)
	$itemGroups = @($Doc.SelectNodes("//msb:ItemGroup", $Namespace))
	foreach ($group in $itemGroups) {
		if ($group.SelectSingleNode("msb:$PreferredTag", $Namespace)) {
			return $group
		}
	}
	if ($itemGroups.Count -gt 0) {
		return $itemGroups[0]
	}
	return $null
}

function Add-VisualStudioProjectItem {
	param(
		[xml]$Doc,
		[System.Xml.XmlNamespaceManager]$Namespace,
		[string]$Tag,
		[string]$Include,
		[string]$Filter = ""
	)
	$normalizedInclude = ($Include -replace "/", "\")
	foreach ($existing in @($Doc.SelectNodes("//msb:$Tag[@Include]", $Namespace))) {
		if (([string]$existing.Include -replace "/", "\").Equals(
			$normalizedInclude,
			[System.StringComparison]::OrdinalIgnoreCase)) {
			return $false
		}
	}
	$itemGroup = Get-FirstItemGroup -Doc $Doc -Namespace $Namespace -PreferredTag $Tag
	if (!$itemGroup) {
		return $false
	}
	$item = $Doc.CreateElement($Tag, $Doc.DocumentElement.NamespaceURI)
	$item.SetAttribute("Include", $Include)
	if (![string]::IsNullOrWhiteSpace($Filter)) {
		$filterNode = $Doc.CreateElement("Filter", $Doc.DocumentElement.NamespaceURI)
		$filterNode.InnerText = $Filter
		[void]$item.AppendChild($filterNode)
	}
	[void]$itemGroup.AppendChild($item)
	return $true
}

function Add-SemicolonNodeValue {
	param(
		[xml]$Doc,
		[System.Xml.XmlNamespaceManager]$Namespace,
		[string]$NodeName,
		[string]$Value
	)
	if ([string]::IsNullOrWhiteSpace($Value)) {
		return $false
	}
	$changed = $false
	foreach ($node in @($Doc.SelectNodes("//msb:$NodeName", $Namespace))) {
		$parts = New-Object 'System.Collections.Generic.List[string]'
		foreach ($part in @($node.InnerText -split ";" | Where-Object { $_ })) {
			$parts.Add([string]$part)
		}
		if (!$parts.Contains($Value)) {
			$parts.Add($Value)
			$node.InnerText = ($parts.ToArray() -join ";")
			$changed = $true
		}
	}
	return $changed
}

function Remove-SemicolonNodeValues {
	param(
		[xml]$Doc,
		[System.Xml.XmlNamespaceManager]$Namespace,
		[string]$NodeName,
		[string[]]$Values
	)
	$remove = New-Object "System.Collections.Generic.HashSet[string]" ([StringComparer]::OrdinalIgnoreCase)
	foreach ($value in @($Values)) {
		if (![string]::IsNullOrWhiteSpace($value)) {
			[void]$remove.Add($value)
		}
	}
	if ($remove.Count -eq 0) {
		return $false
	}

	$changed = $false
	foreach ($node in @($Doc.SelectNodes("//msb:$NodeName", $Namespace))) {
		$parts = New-Object -TypeName 'System.Collections.ArrayList'
		foreach ($part in @($node.InnerText -split ";" | Where-Object { $_ })) {
			if (!$remove.Contains([string]$part)) {
				[void]$parts.Add([string]$part)
			} else {
				$changed = $true
			}
		}
		if ($changed) {
			$node.InnerText = ($parts.ToArray() -join ";")
		}
	}
	return $changed
}

function Remove-CompilerOptions {
	param(
		[xml]$Doc,
		[System.Xml.XmlNamespaceManager]$Namespace,
		[string[]]$Options
	)
	$remove = New-Object "System.Collections.Generic.HashSet[string]" ([StringComparer]::OrdinalIgnoreCase)
	foreach ($option in @($Options)) {
		if (![string]::IsNullOrWhiteSpace($option)) {
			[void]$remove.Add($option)
		}
	}
	if ($remove.Count -eq 0) {
		return $false
	}

	$changed = $false
	foreach ($node in @($Doc.SelectNodes("//msb:ClCompile/msb:AdditionalOptions", $Namespace))) {
		$keptOptions = New-Object -TypeName 'System.Collections.ArrayList'
		foreach ($option in @($node.InnerText -split "\s+" | Where-Object { $_ })) {
			if (!$remove.Contains([string]$option)) {
				[void]$keptOptions.Add([string]$option)
			} else {
				$changed = $true
			}
		}
		if ($changed) {
			$node.InnerText = ($keptOptions.ToArray() -join " ")
		}
	}
	return $changed
}

function Add-CompilerOption {
	param(
		[xml]$Doc,
		[System.Xml.XmlNamespaceManager]$Namespace,
		[string]$Option
	)
	if ([string]::IsNullOrWhiteSpace($Option)) {
		return $false
	}
	$changed = $false
	foreach ($node in @($Doc.SelectNodes("//msb:ClCompile/msb:AdditionalOptions", $Namespace))) {
		$options = @($node.InnerText -split "\s+" | Where-Object { $_ })
		if ($options -notcontains $Option) {
			$node.InnerText = (($options + $Option) -join " ")
			$changed = $true
		}
	}
	return $changed
}

function Test-GeneratedAddonPath {
	param([string]$Path)
	if ([string]::IsNullOrWhiteSpace($Path)) {
		return $false
	}

	$normalized = $Path -replace "/", "\"
	return ($normalized -match '(^|\\)build[^\\]*(\\|$)') -or
		($normalized -match '(^|\\)libs\\[^\\]+\\\.source(\\|$)') -or
		($normalized -match '(^|\\)libs\\[^\\]+\\build[^\\]*(\\|$)')
}

function Test-ProjectPathUnderAddon {
	param(
		[string]$Path,
		[string]$AddonName
	)
	if ([string]::IsNullOrWhiteSpace($Path)) {
		return $false
	}
	$normalized = $Path -replace "/", "\"
	return $normalized -match "(^|\\)$([regex]::Escape($AddonName))(\\|$)" -or
		$normalized -match "\.\.\\\.\.\\$([regex]::Escape($AddonName))\\"
}

function Get-ExampleAddonRoots {
	param(
		[string]$ExampleRoot,
		[string]$AddonRoot,
		[string]$AddonsRoot
	)
	$addonNames = New-Object System.Collections.Generic.List[string]
	foreach ($name in @(Get-ExampleAddons -ExampleRoot $ExampleRoot)) {
		if (!$addonNames.Contains($name)) {
			$addonNames.Add($name)
		}
	}
	foreach ($required in @("ofxGgmlCore", "ofxGgmlMusic")) {
		if (!$addonNames.Contains($required)) {
			$addonNames.Add($required)
		}
	}

	$roots = New-Object System.Collections.Generic.List[string]
	foreach ($name in $addonNames) {
		$root = Join-Path $AddonsRoot $name
		if (Test-Path -LiteralPath $root -PathType Container) {
			$roots.Add($root)
		} else {
			Write-Step "Skipping missing addon path for $name"
		}
	}
	return @($roots)
}

function Repair-VisualStudioAddonItems {
	param(
		[xml]$Doc,
		[System.Xml.XmlNamespaceManager]$Namespace,
		[string]$Path,
		[string[]]$AddonRoots
	)
	$changed = $false
	$projectDir = Split-Path -Parent $Path
	$isFilters = $Path.EndsWith(".vcxproj.filters", [System.StringComparison]::OrdinalIgnoreCase)
	if (!$Path.EndsWith(".vcxproj", [System.StringComparison]::OrdinalIgnoreCase) -and !$isFilters) {
		return $false
	}

	foreach ($root in $AddonRoots) {
		$entryName = Split-Path -Leaf $root
		$excludePaths = Get-AddonSourceExcludes -AddonRoot $root
		foreach ($sourceRoot in Get-AddonSourceRoots -AddonRoot $root) {
			Get-ChildItem -LiteralPath $sourceRoot -Recurse -File | ForEach-Object {
				$relativeToAddon = Get-RelativeProjectPath -ProjectDir $root -FilePath $_.FullName
				if (Test-AddonSourceExcluded -RelativePath $relativeToAddon -ExcludePatterns $excludePaths) {
					return
				}
				$relative = Get-RelativeProjectPath -ProjectDir $projectDir -FilePath $_.FullName
				$filter = if ($isFilters) {
					"addons\" + $entryName + "\" +
						(Split-Path -Parent $relative).TrimStart(".\").Replace("..\", "")
				} else {
					""
				}
				if ($_.Extension -in @(".cpp", ".cxx", ".cc")) {
					if (Add-VisualStudioProjectItem -Doc $Doc -Namespace $Namespace -Tag "ClCompile" -Include $relative -Filter $filter) {
						$changed = $true
					}
				} elseif ($_.Extension -in @(".h", ".hpp")) {
					if (Add-VisualStudioProjectItem -Doc $Doc -Namespace $Namespace -Tag "ClInclude" -Include $relative -Filter $filter) {
						$changed = $true
					}
				}
			}
		}
	}
	return $changed
}

function Repair-VisualStudioProjectFile {
	param(
		[string]$Path,
		[string[]]$AddonRoots
	)
	if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
		return
	}

	[xml]$doc = Get-Content -LiteralPath $Path -Raw
	$namespace = New-Object System.Xml.XmlNamespaceManager($doc.NameTable)
	$namespace.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")
	$changed = $false
	$projectDir = Split-Path -Parent $Path
	$isAceStepProject = (Split-Path -Leaf $projectDir) -eq "ofxGgmlMusicAceStepExample"
	$selectedAddonNames = New-Object "System.Collections.Generic.HashSet[string]" ([StringComparer]::OrdinalIgnoreCase)
	foreach ($root in @($AddonRoots)) {
		[void]$selectedAddonNames.Add((Split-Path -Leaf $root))
	}
	$staleAddonNames = @("ofxGgmlCore") | Where-Object { !$selectedAddonNames.Contains($_) }

	foreach ($tag in @("ClCompile", "ClInclude", "None", "CustomBuild", "CudaCompile", "Filter")) {
		$seenIncludes = @{}
		foreach ($node in @($doc.SelectNodes("//msb:$tag[@Include]", $namespace))) {
			$normalizedInclude = ([string]$node.Include -replace "/", "\")
			$extension = [System.IO.Path]::GetExtension($normalizedInclude)
			$headerCompiledAsSource = $tag -eq "ClCompile" -and $extension -in @(".h", ".hpp")
			$duplicateInclude = $seenIncludes.ContainsKey($normalizedInclude.ToLowerInvariant())
			$staleAddonItem = $false
			foreach ($addonName in @($staleAddonNames)) {
				if (Test-ProjectPathUnderAddon -Path $node.Include -AddonName $addonName) {
					$staleAddonItem = $true
					break
				}
			}
			if ((Test-GeneratedAddonPath $node.Include) -or $headerCompiledAsSource -or $duplicateInclude -or $staleAddonItem) {
				[void]$node.ParentNode.RemoveChild($node)
				$changed = $true
			} else {
				$seenIncludes[$normalizedInclude.ToLowerInvariant()] = $true
			}
		}
	}

	if ($Path.EndsWith(".vcxproj", [System.StringComparison]::OrdinalIgnoreCase)) {
		$includeDirs = New-Object System.Collections.Generic.List[string]
		foreach ($root in $AddonRoots) {
			foreach ($include in Get-AddonIncludes -AddonRoot $root) {
				$relativeInclude = Get-RelativeProjectPath -ProjectDir $projectDir -FilePath $include
				if (!$includeDirs.Contains($relativeInclude)) {
					$includeDirs.Add($relativeInclude)
				}
			}
		}
		foreach ($node in @($doc.SelectNodes("//msb:AdditionalIncludeDirectories", $namespace))) {
			$parts = New-Object System.Collections.Generic.List[string]
			foreach ($part in @($node.InnerText -split ";" | Where-Object { $_ })) {
				$staleAddonInclude = $false
				foreach ($addonName in @($staleAddonNames)) {
					if (Test-ProjectPathUnderAddon -Path $part -AddonName $addonName) {
						$staleAddonInclude = $true
						break
					}
				}
				if (!(Test-GeneratedAddonPath $part) -and !$staleAddonInclude -and !$parts.Contains($part)) {
					$parts.Add([string]$part)
				} elseif ($staleAddonInclude) {
					$changed = $true
				}
			}
			foreach ($includeDir in $includeDirs) {
				if (!$parts.Contains($includeDir)) {
					$parts.Add($includeDir)
					$changed = $true
				}
			}
			$updated = $parts.ToArray() -join ";"
			if ($updated -ne $node.InnerText) {
				$node.InnerText = $updated
				$changed = $true
			}
		}

		foreach ($define in Get-AddonDefines -AddonRoots $AddonRoots) {
			foreach ($node in @($doc.SelectNodes("//msb:ClCompile/msb:AdditionalOptions", $namespace))) {
				$options = @($node.InnerText -split "\s+" | Where-Object { $_ })
				$option = "-D$define"
				if ($options -notcontains $option) {
					$node.InnerText = (($options + $option) -join " ")
					$changed = $true
				}
			}
		}
		if ($isAceStepProject) {
			if (Remove-CompilerOptions -Doc $doc -Namespace $namespace -Options @("-DOFXIMGUI_GLFW_EVENTS_REPLACE_OF_CALLBACKS=1", "-DOFXIMGUI_GLFW_FIX_MULTICONTEXT_PRIMARY_VP=0", "-DOFXIMGUI_GLFW_FIX_MULTICONTEXT_SECONDARY_VP=1")) {
				$changed = $true
			}
			if (Add-CompilerOption -Doc $doc -Namespace $namespace -Option "-DOFXIMGUI_GLFW_EVENTS_REPLACE_OF_CALLBACKS=0") {
				$changed = $true
			}
		}
		if ($staleAddonNames -contains "ofxGgmlCore") {
			if (Remove-CompilerOptions -Doc $doc -Namespace $namespace -Options @("-DGGML_MAX_NAME=128", "-DOFXGGML_WITH_CUDA", "-DOFXIMGUI_GLFW_EVENTS_REPLACE_OF_CALLBACKS=1", "-DOFXIMGUI_GLFW_FIX_MULTICONTEXT_PRIMARY_VP=0", "-DOFXIMGUI_GLFW_FIX_MULTICONTEXT_SECONDARY_VP=1")) {
				$changed = $true
			}
			if (Add-CompilerOption -Doc $doc -Namespace $namespace -Option "-DOFXIMGUI_GLFW_EVENTS_REPLACE_OF_CALLBACKS=0") {
				$changed = $true
			}
			if (Remove-SemicolonNodeValues -Doc $doc -Namespace $namespace -NodeName "AdditionalDependencies" -Values @("ggml.lib", "ggml-base.lib", "ggml-cpu.lib", "ggml-cuda.lib", "cublas.lib", "cudart.lib", "cuda.lib")) {
				$changed = $true
			}
			if (Remove-SemicolonNodeValues -Doc $doc -Namespace $namespace -NodeName "AdditionalLibraryDirectories" -Values @("..\..\ofxGgmlCore\libs\ggml\lib", '$(CUDA_PATH)\lib\x64')) {
				$changed = $true
			}
		}

		foreach ($root in $AddonRoots) {
			foreach ($library in Get-AddonLibs -AddonRoot $root) {
				$reference = ConvertTo-ProjectLibraryReference -AddonRoot $root -ProjectDir $projectDir -Library $library
				if (!$reference) {
					continue
				}
				if (Add-SemicolonNodeValue -Doc $doc -Namespace $namespace -NodeName "AdditionalDependencies" -Value $reference.Dependency) {
					$changed = $true
				}
				if (Add-SemicolonNodeValue -Doc $doc -Namespace $namespace -NodeName "AdditionalLibraryDirectories" -Value $reference.Directory) {
					$changed = $true
				}
			}
		}
	}

	if (Repair-VisualStudioAddonItems -Doc $doc -Namespace $namespace -Path $Path -AddonRoots $AddonRoots) {
		$changed = $true
	}

	foreach ($node in @($doc.SelectNodes("//msb:PostBuildEvent/msb:Command", $namespace))) {
		if ($node.InnerText -match '\$\(ProjectDir\)dll\\([^\\]+)\\\*\.dll') {
			$platformName = $matches[1]
			$guardedCommand = "if exist `"`$(ProjectDir)dll\$platformName\*.dll`" xcopy /Y /E `"`$(ProjectDir)dll\$platformName\*.dll`" `"`$(TargetDir)`""
			if ($node.InnerText -ne $guardedCommand) {
				$node.InnerText = $guardedCommand
				$changed = $true
			}
		}
	}

	if ($changed) {
		$doc.Save($Path)
		Write-Step "Updated generated project metadata in $(Split-Path -Leaf $Path)"
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$addonsRoot = Resolve-Path (Join-Path $addonRoot "..")
$ofRoot = Resolve-Path (Join-Path $addonsRoot "..")
$exampleDir = Join-Path $addonRoot $Example
if (!(Test-Path -LiteralPath $exampleDir -PathType Container)) {
	throw "Example directory not found: $exampleDir"
}
Normalize-WindowsPathEnvironment

if (Test-WindowsHost) {
	Ensure-GeneratedVisualStudioProject -ExampleName $Example -ExamplePath $exampleDir -OfRoot $ofRoot
	$project = Join-Path $exampleDir "$Example.vcxproj"
	if (!(Test-Path -LiteralPath $project -PathType Leaf)) {
		throw "Visual Studio project not found: $project. Generate it with the openFrameworks projectGenerator first."
	}
	$addonRoots = Get-ExampleAddonRoots -ExampleRoot $exampleDir -AddonRoot $addonRoot -AddonsRoot $addonsRoot
	Repair-VisualStudioProjectFile -Path $project -AddonRoots $addonRoots
	Repair-VisualStudioProjectFile -Path "$project.filters" -AddonRoots $addonRoots
	if ($RepairOnly) {
		Write-Step "Repair-only project metadata check completed for $Example"
		return
	}

	$msbuild = Get-MsBuild
	if ([string]::IsNullOrWhiteSpace($msbuild)) {
		throw "MSBuild.exe was not found."
	}

	$target = if ($Clean) { "Rebuild" } else { "Build" }
	$buildJobs = Resolve-BuildJobs -RequestedJobs $Jobs
	$parallelArgs = Get-MsBuildParallelArguments -BuildJobs $buildJobs
	$ofBuildStateDir = Get-OfBuildStateDirectory -OfRoot $ofRoot.Path -Platform $Platform -Configuration $Configuration
	Write-OfBuildStatePreflightWarning -BuildStateDir $ofBuildStateDir
	Write-Step "Building $Example $Configuration $Platform with MSBuild ($buildJobs jobs)"
	$lockName = "Local\ofxGgml-msbuild-" + (Get-StableNameFragment $ofRoot.Path)
	Invoke-WithNamedMutex -Name $lockName -Command {
		$result = Invoke-MsBuildProject `
			-MsBuild $msbuild `
			-Project $project `
			-Target $target `
			-Configuration $Configuration `
			-Platform $Platform `
			-ParallelArguments $parallelArgs
		if ($result.ExitCode -eq 0) {
			return
		}
		if (Test-OfBuildStateLockFailure -Output $result.Output) {
			if (!$ResetOfBuildState) {
				throw (Get-OfBuildStateLockFailureMessage -BuildStateDir $ofBuildStateDir)
			}
			Reset-OfBuildState -BuildStateDir $ofBuildStateDir
			Write-Step "Retrying $Example after resetting openFrameworks build state"
			$retryResult = Invoke-MsBuildProject `
				-MsBuild $msbuild `
				-Project $project `
				-Target $target `
				-Configuration $Configuration `
				-Platform $Platform `
				-ParallelArguments $parallelArgs
			if ($retryResult.ExitCode -eq 0) {
				return
			}
			if (Test-OfBuildStateLockFailure -Output $retryResult.Output) {
				Write-Warning "MSBuild still reports the shared openFrameworks build state is locked after -ResetOfBuildState."
				throw (Get-OfBuildStateLockFailureMessage -BuildStateDir $ofBuildStateDir)
			}
			throw "MSBuild $Example failed after resetting openFrameworks build state with exit code $($retryResult.ExitCode)"
		}
		throw "MSBuild $Example failed with exit code $($result.ExitCode)"
	}
	return
}

$makefile = Join-Path $exampleDir "Makefile"
if (Test-Path -LiteralPath $makefile -PathType Leaf) {
	$target = if ($Clean) { "clean Release" } else { "Release" }
	Write-Step "Building $Example with make"
	Invoke-CheckedNative "make $Example" {
		make -C $exampleDir $target
	}
	return
}

if ($IsMacOS) {
	$xcodeProject = Get-ChildItem -LiteralPath $exampleDir -Filter "*.xcodeproj" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
	if ($xcodeProject) {
		Write-Step "Building $Example $Configuration with xcodebuild"
		Invoke-CheckedNative "xcodebuild $Example" {
			xcodebuild -project $xcodeProject.FullName -configuration $Configuration
		}
		return
	}
}

throw "No supported generated project was found for $Example. Generate the example project with openFrameworks projectGenerator first."
