param(
	[string]$WorkbenchPath = 'D:\SteamLibrary\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe',
	[string]$GameAddonsPath,
	[string]$BaseGameAddonsPath,
	[switch]$PrintOnly
)

$ErrorActionPreference = 'Stop'
$repoPath = Split-Path -Parent $PSScriptRoot
$projectPath = Join-Path $repoPath 'OptionalAddons/OpenResistanceREAPER/addon.gproj'
$corePath = Join-Path $repoPath 'AROpenResistance/Open Resistance/addon.gproj'
$coreAddonsPath = Join-Path $repoPath 'AROpenResistance'

if (!$BaseGameAddonsPath) {
	$toolsInstallPath = Split-Path -Parent (Split-Path -Parent $WorkbenchPath)
	$steamCommonPath = Split-Path -Parent $toolsInstallPath
	$BaseGameAddonsPath = Join-Path $steamCommonPath 'Arma Reforger/addons'
}
$baseGameProject = Join-Path $BaseGameAddonsPath 'data/ArmaReforger.gproj'
$engineProject = Join-Path $BaseGameAddonsPath 'core/core.gproj'

if (!$GameAddonsPath) {
	$documentCandidates = @([Environment]::GetFolderPath('MyDocuments'))
	foreach ($oneDriveRoot in @($env:OneDrive, $env:OneDriveConsumer, $env:OneDriveCommercial, (Join-Path $env:USERPROFILE 'OneDrive'))) {
		if ($oneDriveRoot) { $documentCandidates += Join-Path $oneDriveRoot 'Documents' }
	}
	foreach ($documentsPath in ($documentCandidates | Select-Object -Unique)) {
		$addonsCandidate = Join-Path $documentsPath 'My Games/ArmaReforger/addons'
		if (!(Test-Path -LiteralPath $addonsCandidate -PathType Container)) { continue }
		$installedReaper = Get-ChildItem -LiteralPath $addonsCandidate -Directory | Where-Object Name -Like '*69A4D664A6284E06*' | Select-Object -First 1
		if ($installedReaper) { $GameAddonsPath = $addonsCandidate; break }
	}
	if (!$GameAddonsPath) { throw 'REAPER was not found in the Documents/OneDrive game addon folders. Supply -GameAddonsPath explicitly.' }
}

foreach ($requiredFile in @($WorkbenchPath, $projectPath, $corePath, $baseGameProject, $engineProject)) {
	if (!(Test-Path -LiteralPath $requiredFile -PathType Leaf)) { throw "Required file not found: $requiredFile" }
}
if (!(Test-Path -LiteralPath $GameAddonsPath -PathType Container)) { throw "Game addons folder not found: $GameAddonsPath" }
$reaperProject = Get-ChildItem -LiteralPath $GameAddonsPath -Directory |
	Where-Object Name -Like '*69A4D664A6284E06*' |
	ForEach-Object { Join-Path $_.FullName 'addon.gproj' } |
	Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
	Select-Object -First 1
if (!$reaperProject) { throw "REAPER project not found beneath $GameAddonsPath" }
if (![IO.File]::ReadAllText($corePath).Contains('6AA282336705EED2')) { throw 'The repository core project has an unexpected GUID' }
if (![IO.File]::ReadAllText($reaperProject).Contains('69A4D664A6284E06')) { throw 'The installed REAPER project has an unexpected GUID' }
if (![IO.File]::ReadAllText($baseGameProject).Contains('58D0FB3206B6F859')) { throw 'The installed base-game project has an unexpected GUID' }
if (![IO.File]::ReadAllText($engineProject).Contains('5614BBCCBB55ED1C')) { throw 'The installed engine-core project has an unexpected GUID' }

# Explicit search roots resolve the real repository core and downloaded backend,
# without changing global project lists or adding a dependency to the core addon.
$searchPaths = $BaseGameAddonsPath + ',' + $coreAddonsPath + ',' + $GameAddonsPath
if ($projectPath.Contains('"') -or $searchPaths.Contains('"')) { throw 'Unexpected quote in project/search path' }
$launchArguments = @('-gproj', ('"{0}"' -f $projectPath), '-addonsDir', ('"{0}"' -f $searchPaths))
Write-Output "Project: $projectPath"
Write-Output "Core: $corePath"
Write-Output "REAPER: $reaperProject"
Write-Output "Base game: $baseGameProject"
Write-Output "Search roots: $searchPaths"
if ($PrintOnly) { return }

# This launches the visible interactive editor, not a build or automated Workbench action.
Start-Process -FilePath $WorkbenchPath -WorkingDirectory (Split-Path -Parent $WorkbenchPath) -ArgumentList $launchArguments -WindowStyle Normal
