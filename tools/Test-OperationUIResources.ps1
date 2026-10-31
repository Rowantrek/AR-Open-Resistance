param([string]$ResourceDatabasePath)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$addonRoot=Join-Path $repoRoot 'AROpenResistance/Open Resistance'
if(!$ResourceDatabasePath){$ResourceDatabasePath=Join-Path $addonRoot 'resourceDatabase.rdb'}
$bytes=[IO.File]::ReadAllBytes($ResourceDatabasePath)
$text=[Text.Encoding]::GetEncoding(28591).GetString($bytes)
$sources=@(Get-ChildItem -LiteralPath (Join-Path $addonRoot 'Scripts/Game') -Recurse -Filter '*.c';Get-ChildItem -LiteralPath (Join-Path $addonRoot 'Configs') -Recurse -Filter '*.conf')
$bindings=($sources | ForEach-Object {[IO.File]::ReadAllText($_.FullName)}) -join "`n"
foreach($resource in @('UI/Layouts/Phone/AROR_Mars.layout','UI/Layouts/Phone/AROR_BlackMarket.layout','UI/Layouts/Setup/AROR_SetupMenu.layout')){
 $metadata=[IO.File]::ReadAllText((Join-Path $addonRoot ($resource+'.meta')))
 $guid=[regex]::Match($metadata,'\{([A-F0-9]{16})\}').Groups[1].Value
 $index=$text.IndexOf($resource+[char]0)
 if($index -lt 0){throw "Layout not registered in the local Workbench database: $resource"}
 $actual=[BitConverter]::ToUInt64($bytes,$index+$resource.Length+7).ToString('X16')
 if($guid -cne $actual){throw "Registered layout GUID differs from metadata: $resource"}
 $references=@([regex]::Matches($bindings,'\{([A-F0-9]{16})\}'+[regex]::Escape($resource)))
 if(!$references.Count -or @($references | Where-Object {$_.Groups[1].Value -cne $guid}).Count){throw "Source layout binding differs from metadata: $resource"}
}
Write-Output 'Verified metadata, script/config bindings and local Workbench registration for MARS, Black Market and setup layouts. Native widget loading/rendering remain unverified.'
