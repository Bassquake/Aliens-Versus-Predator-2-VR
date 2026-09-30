# Launches AVP2 straight into a level, screenshots the primary monitor after a delay, then closes it.
# Usage: shot.ps1 -Out <png> [-VrRez] [-World Worlds\SinglePlayer\m1s1] [-Delay 35]
param([string]$Out, [switch]$VrRez, [string]$World = "Worlds\SinglePlayer\m1s1", [int]$Delay = 35)

Add-Type -AssemblyName System.Windows.Forms, System.Drawing
$game = "C:\Program Files (x86)\Fox\Aliens vs. Predator 2"
$args = "-cmdfile avp2cmds.txt"
if ($VrRez) { $args += ' -rez "K:\Coding\Aliens-Versus-Predator-2-VR\proj\avp2xr\deploy\vrrez"' }
$args += " +runworld $World"

# No XR runtime for these runs: the game plays on the monitor only.
$env:XR_RUNTIME_JSON = "C:\nonexistent_runtime.json"
$p = Start-Process -FilePath "$game\lithtech.exe" -WorkingDirectory $game -ArgumentList $args -PassThru
Start-Sleep -Seconds $Delay

$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
"saved $Out"
