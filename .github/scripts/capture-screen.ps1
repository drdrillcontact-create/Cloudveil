# Photographs the screen into a PNG file: all of the main screen, or the given part of it.
# (Called by capture-screen.sh.)
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$X = 0,
    [int]$Y = 0,
    [int]$W = 0,
    [int]$H = 0
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds

if ($W -le 0 -or $H -le 0) {
    $X = $screen.X; $Y = $screen.Y; $W = $screen.Width; $H = $screen.Height
}

# (not beyond the screen's edge)
if ($X + $W -gt $screen.Right)  { $W = $screen.Right - $X }
if ($Y + $H -gt $screen.Bottom) { $H = $screen.Bottom - $Y }

$bitmap = New-Object System.Drawing.Bitmap $W, $H
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)

try {
    $graphics.CopyFromScreen($X, $Y, 0, 0, $bitmap.Size)
    $bitmap.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $graphics.Dispose()
    $bitmap.Dispose()
}
