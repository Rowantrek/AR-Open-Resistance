param([Parameter(Mandatory=$true)][string]$ResourceDatabasePath)
# Read-only reference checks. Metadata matching is not Workbench discovery or native prefab acceptance.
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$addonRoot=Join-Path $repoRoot 'AROpenResistance/Open Resistance'
$bytes=[IO.File]::ReadAllBytes($ResourceDatabasePath)
$database=[Text.Encoding]::GetEncoding(28591).GetString($bytes)
$checked=New-Object 'Collections.Generic.HashSet[string]'
$localCount=0; $installedCount=0
$strongholds=@('Fortress_Vostok','Stronghold_Delaware')
foreach($stronghold in $strongholds){
 $relative='Prefabs/Compositions/Locations/AROR_Defence_'+$stronghold+'.et'
 $file=Join-Path $addonRoot $relative
 $meta=[IO.File]::ReadAllText($file+'.meta')
 if($meta -notmatch ('Name "\{[A-F0-9]{16}\}'+[regex]::Escape($relative)+'"')){throw "Stronghold metadata does not match its path: $relative"}
 $source=[IO.File]::ReadAllText($file)
 foreach($reference in [regex]::Matches($source,'\{([A-F0-9]{16})\}(Prefabs/[^"\r\n]+\.et)')){
  $guid=$reference.Groups[1].Value; $path=$reference.Groups[2].Value
  if(!$checked.Add($reference.Value)){continue}
  $local=Join-Path $addonRoot $path
  if(Test-Path -LiteralPath $local){
   $resourceMeta=[IO.File]::ReadAllText($local+'.meta')
   if(!$resourceMeta.Contains('Name "{'+$guid+'}'+$path+'"')){throw "Local reference mismatch: $($reference.Value)"}
   $localCount++
  } else {
   $index=$database.IndexOf($path+[char]0,[StringComparison]::Ordinal)
   if($index -lt 0){throw "Installed resource missing: $path"}
   $actual=[BitConverter]::ToUInt64($bytes,$index+$path.Length+7).ToString('X16')
   if($actual -cne $guid){throw "Installed GUID mismatch: $path ($guid / $actual)"}
   $installedCount++
  }
 }
}
$ids=@{}
foreach($metaFile in Get-ChildItem -LiteralPath $addonRoot -Recurse -Filter '*.meta'){
 $match=[regex]::Match([IO.File]::ReadAllText($metaFile.FullName),'Name "\{([A-F0-9]{16})\}([^"\r\n]+)"')
 if(!$match.Success){continue}
 $id=$match.Groups[1].Value
 if($ids.ContainsKey($id)){throw "Duplicate resource metadata GUID $id : $($ids[$id]) / $($metaFile.FullName)"}
 $ids[$id]=$metaFile.FullName
}
Write-Output "Verified both stronghold metadata paths, $localCount local references, $installedCount installed GUIDs and metadata GUID uniqueness. New stronghold GUIDs remain hand-authored; Workbench discovery and native loading are NOT_RUN."
