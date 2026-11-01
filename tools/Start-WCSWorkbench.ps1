param(
	[string]$WorkbenchPath = 'D:\SteamLibrary\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe',
	[string]$GameAddonsPath,
	[string]$BaseGameAddonsPath,
	[switch]$PrintOnly
)

# Opens the Open Resistance - WCS extender in Workbench, resolving the repository core and the WCS/RHS addons
# downloaded by the game, without registering them globally.

$ErrorActionPreference = 'Stop'
$repoPath = Split-Path -Parent $PSScriptRoot
$projectPath = Join-Path $repoPath 'OptionalAddons/OpenResistanceWCS/addon.gproj'
$corePath = Join-Path $repoPath 'AROpenResistance/Open Resistance/addon.gproj'
$coreAddonsPath = Join-Path $repoPath 'AROpenResistance'
$coreGuid = '6C68A5E5654D7210'

if (!$BaseGameAddonsPath) {
	$toolsInstallPath = Split-Path -Parent (Split-Path -Parent $WorkbenchPath)
	$steamCommonPath = Split-Path -Parent $toolsInstallPath
	$BaseGameAddonsPath = Join-Path $steamCommonPath 'Arma Reforger/addons'
}
$baseGameProject = Join-Path $BaseGameAddonsPath 'data/ArmaReforger.gproj'
$engineProject = Join-Path $BaseGameAddonsPath 'core/core.gproj'

foreach ($requiredFile in @($WorkbenchPath, $projectPath, $corePath, $baseGameProject, $engineProject)) {
	if (!(Test-Path -LiteralPath $requiredFile -PathType Leaf)) { throw "Required file not found: $requiredFile" }
}
if (![IO.File]::ReadAllText($corePath).Contains($coreGuid)) { throw 'The repository core project has an unexpected GUID' }

if (!$GameAddonsPath) {
	$documentCandidates = @([Environment]::GetFolderPath('MyDocuments'))
	foreach ($oneDriveRoot in @($env:OneDrive, $env:OneDriveConsumer, $env:OneDriveCommercial, (Join-Path $env:USERPROFILE 'OneDrive'))) {
		if ($oneDriveRoot) { $documentCandidates += Join-Path $oneDriveRoot 'Documents' }
	}
	foreach ($documentsPath in ($documentCandidates | Select-Object -Unique)) {
		$addonsCandidate = Join-Path $documentsPath 'My Games/ArmaReforger/addons'
		if (Test-Path -LiteralPath $addonsCandidate -PathType Container) { $GameAddonsPath = $addonsCandidate; break }
	}
	if (!$GameAddonsPath) { throw 'The game addon folder was not found in Documents/OneDrive. Supply -GameAddonsPath explicitly.' }
}

# Resolve the actual project GUIDs and all transitive dependencies, including engine data.
# A folder suffix is not proof that the matching addon is installed or that its dependencies exist.
. (Join-Path $PSScriptRoot 'WCSDependencyGraph.ps1')
$dependencyProjects = @(Resolve-ARORAddonDependencies -RootProject $projectPath -SearchRoots @($GameAddonsPath) -ExplicitProjects @($corePath, $baseGameProject, $engineProject))
$dependencies = @($dependencyProjects | Where-Object { $_.Guid -ne '1CAE49450A83DEAB' })

$searchPaths = $BaseGameAddonsPath + ',' + $coreAddonsPath + ',' + $GameAddonsPath
if ($projectPath.Contains('"') -or $searchPaths.Contains('"')) { throw 'Unexpected quote in project/search path' }
$launchArguments = @('-gproj', ('"{0}"' -f $projectPath), '-addonsDir', ('"{0}"' -f $searchPaths))
Write-Output "Project: $projectPath"
Write-Output "Core: $corePath"
Write-Output "Dependencies validated (including transitive): $($dependencies.Count)"
Write-Output "Search roots: $searchPaths"
if ($PrintOnly) { return }

# This launches the visible interactive editor, not a build or automated Workbench action.
Start-Process -FilePath $WorkbenchPath -WorkingDirectory (Split-Path -Parent $WorkbenchPath) -ArgumentList $launchArguments -WindowStyle Normal
