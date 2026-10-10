# Renders the JASS application / plugin icon (Resources/Icon/jass_512.png + jass_64.png) with
# System.Drawing, so the icon is reproducible from this script instead of a binary nobody can
# edit. CMake hands the two PNGs to JUCE (ICON_BIG / ICON_SMALL); juceaide turns them into the
# .ico embedded in JASS.exe and JASS.vst3.
#
# Design: the rack's ground colour (#15181d) as a rounded tile, the name in Bahnschrift, and
# three small modules along the bottom in the rack's zone colours (generator teal, modulator
# purple, processor blue), each with one knob. Pure ASCII on purpose (PowerShell 5.1).
#
# Usage (from the repo root):  powershell -ExecutionPolicy Bypass -File tools\make_icon.ps1

Add-Type -AssemblyName System.Drawing

$repo = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $repo 'Resources\Icon'
New-Item -ItemType Directory -Force $outDir | Out-Null

function New-RoundedPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $p.AddArc($x, $y, 2*$r, 2*$r, 180, 90)
    $p.AddArc($x+$w-2*$r, $y, 2*$r, 2*$r, 270, 90)
    $p.AddArc($x+$w-2*$r, $y+$h-2*$r, 2*$r, 2*$r, 0, 90)
    $p.AddArc($x, $y+$h-2*$r, 2*$r, 2*$r, 90, 90)
    $p.CloseFigure()
    return $p
}

function Make-Icon([int]$size, [string]$path) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $g.Clear([System.Drawing.Color]::Transparent)

    $ground = [System.Drawing.Color]::FromArgb(255, 0x15, 0x18, 0x1d)   # Rack.cpp: the rack ground
    $g.FillPath((New-Object System.Drawing.SolidBrush($ground)), (New-RoundedPath 0 0 $size $size ([float]($size * 0.18))))

    # Three rack modules along the bottom (ModuleDescriptor.h typeColour: Generator / Modulator / Processor).
    $cols = @([System.Drawing.Color]::FromArgb(255,0x5e,0x9b,0x96),
              [System.Drawing.Color]::FromArgb(255,0x93,0x84,0xb6),
              [System.Drawing.Color]::FromArgb(255,0x6f,0x86,0xad))
    $m = [float]($size * 0.11); $gap = [float]($size * 0.045)
    $w = ([float]$size - 2*$m - 2*$gap) / 3; $h = [float]($size * 0.19); $y = [float]($size - $m - $h)
    for ($i = 0; $i -lt 3; $i++) {
        $x = $m + $i * ($w + $gap)
        $g.FillPath((New-Object System.Drawing.SolidBrush($cols[$i])), (New-RoundedPath $x $y $w $h ([float]($size * 0.035))))
        $d = [float]($h * 0.42)
        $g.FillEllipse((New-Object System.Drawing.SolidBrush($ground)), [float]($x + $w/2 - $d/2), [float]($y + $h/2 - $d/2), $d, $d)
    }

    # The name.
    $font = New-Object System.Drawing.Font('Bahnschrift SemiBold', [float]($size * 0.30), [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $sf = New-Object System.Drawing.StringFormat
    $sf.Alignment = 'Center'; $sf.LineAlignment = 'Center'
    $textRect = New-Object System.Drawing.RectangleF(0, [float]($size * 0.06), $size, [float]($size * 0.62))
    $g.DrawString('JASS', $font, [System.Drawing.Brushes]::White, $textRect, $sf)

    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "wrote $path"
}

Make-Icon 512 (Join-Path $outDir 'jass_512.png')
Make-Icon 64  (Join-Path $outDir 'jass_64.png')
