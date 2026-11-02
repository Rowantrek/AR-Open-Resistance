# Shared rule for gun embrasures in defence compositions. Static guns sit on low tripods, so even a low sandbag wall in
# front of one blocks its fire. A wall piece is cut when its centre lies ahead of a gun, within the gun's +/-15 degree arc
# (plus half a segment) and within reach of the perimeter. Dot-source this file; coordinates are composition-local.

$script:EmbrasureRange = 30
$script:EmbrasureSlack = 1.5

# guns: array of @(x, z, yaw). Returns true when a wall piece centred at x/z sits in any gun's firing lane.
function Test-InEmbrasure([double]$x, [double]$z, $guns) {
 foreach ($gun in $guns) {
  $angle = [double]$gun[2] * [Math]::PI / 180
  $dx = $x - [double]$gun[0]; $dz = $z - [double]$gun[1]
  $forward = $dx * [Math]::Sin($angle) + $dz * [Math]::Cos($angle)
  $lateral = $dx * [Math]::Cos($angle) - $dz * [Math]::Sin($angle)
  if ($forward -gt 0 -and $forward -lt $script:EmbrasureRange -and [Math]::Abs($lateral) -lt ($forward * [Math]::Tan(15 * [Math]::PI / 180) + $script:EmbrasureSlack)) { return $true }
 }
 return $false
}
