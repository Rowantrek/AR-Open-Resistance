# Shared source-data helpers. Handles both generated entities and Workbench's
# grouped prefab syntax without treating quoted resource GUIDs as block braces.
function Get-DefencePoseKey($row) {
 $culture=[Globalization.CultureInfo]::InvariantCulture
 $parts=@($row.DefencePreset)
 foreach($field in @('X','Y','Z','DefenceYaw','FlagX','FlagY','FlagZ')) {
  $value=[double]$row.$field
  if($field -eq 'DefenceYaw'){$value=(($value%360)+360)%360}
  $parts+=$value.ToString('R',$culture)
 }
 $parts -join '|'
}
function Read-CampaignLayerLocations([string]$text,[switch]$RootsOnly) {
 $result=@{}
 foreach($match in [regex]::Matches($text,'(?m)^([ \t]*)(?:GenericEntity[ \t]+)?AROR_Location_(everon_\w+)[^\r\n]*\{[ \t]*\r?$')) {
  $depth=0;$quoted=$false;$opened=$false;$end=-1
  for($cursor=$match.Index;$cursor -lt $text.Length;$cursor++) {
   $char=$text[$cursor]
   if($quoted -and $char -eq '\'){$cursor++;continue}
   if($char -eq '"'){$quoted=!$quoted;continue}
   if($quoted){continue}
   if($char -eq '{'){$depth++;$opened=$true}
   if($char -eq '}'){$depth--;if($opened -and $depth -eq 0){$end=$cursor+1;break}}
  }
  if($end -lt 0){throw 'Incomplete saved location block'}
  $block=$text.Substring($match.Index,$end-$match.Index)
  $entityName=$match.Groups[2].Value
  $id=$entityName
  $stableId=[regex]::Match($block,'(?m)^\s+m_sId "([^"]+)"')
  if($stableId.Success){$id=$stableId.Groups[1].Value}
  if($result.ContainsKey($id)){throw "Duplicate saved location: $id"}
  $indent=[regex]::Escape($match.Groups[1].Value)
  $coords=[regex]::Match($block,('(?m)^'+$indent+' coords ([\d.eE+\-]+) ([\d.eE+\-]+) ([\d.eE+\-]+)'))
  if(!$coords.Success){throw "Missing saved root transform: $id"}
  $origin=@(1..3 | ForEach-Object {[double]$coords.Groups[$_].Value})
  $angles=[regex]::Match($block,('(?m)^'+$indent+' angles ([\d.\-]+) ([\d.\-]+) ([\d.\-]+)'))
  $yaw=0
  if($angles.Success){if([double]$angles.Groups[1].Value -ne 0 -or [double]$angles.Groups[3].Value -ne 0){throw "Unexpected tilted location root: $id"};$yaw=[double]$angles.Groups[2].Value}
  $children=@{}
  if(!$RootsOnly){foreach($child in [regex]::Matches($block,('(?ms)^'+$indent+'  GenericEntity[^\r\n]*\r?\n(.*?)^'+$indent+'  \}'))) {
   $childId=[regex]::Match($child.Groups[1].Value,'ID "([A-F0-9]{16})"').Groups[1].Value
   if(!$childId -or $children.ContainsKey($childId)){throw "Missing/duplicate saved child ID: $id"}
   $p=[regex]::Match($child.Groups[1].Value,('(?m)^'+$indent+'   coords ([\d.eE+\-]+) ([\d.eE+\-]+) ([\d.eE+\-]+)'))
   $position=$null
   if($p.Success){$position=@(1..3 | ForEach-Object {[double]$p.Groups[$_].Value})}
   $children[$childId]=$position
  }}
  # Workbench saves instances sharing a prefab as named children of one $grp header, which carries the prefab reference
  $prefab=[regex]::Match($match.Value,'"(\{[0-9A-F]{16}\}[^"]+)"').Groups[1].Value
  if(!$prefab){$group=[regex]::Matches($text.Substring(0,$match.Index),'(?m)^\$grp GenericEntity : "(\{[0-9A-F]{16}\}[^"]+)"');if($group.Count){$prefab=$group[$group.Count-1].Groups[1].Value}}
  $result[$id]=[pscustomobject]@{Origin=$origin;Yaw=$yaw;Children=$children;Text=$block;EntityName=$entityName;Prefab=$prefab}
 }
 return $result
}
