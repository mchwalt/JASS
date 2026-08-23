<#
.SYNOPSIS
    Capture the JASS window into docs/screenshots/ - the README's pictures, reproducibly.

.DESCRIPTION
    Starts the standalone (or attaches to a running one), waits, brings the window to the front and
    writes a PNG of exactly the window: no desktop around it, no drop shadow. The frame comes from
    DWM's *extended* bounds, because GetWindowRect alone reports the invisible resize border too -
    which is why hand-taken screenshots kept coming out with a grey margin.

    The rack shot needs nothing but a delay. The MODULES panel is a call-out that exists only while
    it is open, so take that one with -Attach and a delay long enough to open it yourself; -Region
    then trims the picture down to the panel.

    ASCII only, deliberately: PowerShell 5.1 reads a BOM-less file as ANSI, and a UTF-8 dash ends in
    a byte that cp1252 calls a curly quote - which PowerShell accepts as a string delimiter. The
    script parses as garbage the moment someone writes a nice long dash in a comment.

.PARAMETER Name
    Output stem, written as <OutDir>\<Name>.png. Default 'rack'.
.PARAMETER Delay
    Seconds between the window appearing and the shutter. Default 8: the rack fades in, and a sample
    set may still be loading in the background.
.PARAMETER Attach
    Use a JASS that is already running instead of starting one, and leave it running afterwards.
.PARAMETER Region
    "x,y,w,h" INSIDE the window, for cropping to one panel. Omit for the whole window.
.PARAMETER Exe
    The standalone. Defaults to the Release build in build/.
.PARAMETER OutDir
    Where the PNG lands. Defaults to docs/screenshots/.

.EXAMPLE
    .\tools\screenshot.ps1
    Fresh start, whole window, docs/screenshots/rack.png.

.EXAMPLE
    .\tools\screenshot.ps1 -Name Modules -Attach -Delay 20 -Region "1180,150,380,760"
    Open the MODULES panel within 20 seconds; the shot is cropped to it.
#>
param(
    [string] $Name   = 'rack',
    [int]    $Delay  = 8,
    [switch] $Attach,
    [switch] $Popup,
    [string] $Region,
    [string] $Exe    = (Join-Path $PSScriptRoot '..\build\JASS_artefacts\Release\Standalone\JASS.exe'),
    [string] $OutDir = (Join-Path $PSScriptRoot '..\docs\screenshots')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

Add-Type @'
using System;
using System.Runtime.InteropServices;
public class JassWin {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    // DWMWA_EXTENDED_FRAME_BOUNDS = 9: the frame as DRAWN, without the invisible resize border.
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h, int attr, out RECT r, int size);

    // A JUCE CallOutBox (the MODULES panel) is its own top-level window, so it can be shot exactly -
    // no guessing at a crop rectangle that a layout change would invalidate anyway.
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr param);
    public delegate bool EnumProc(IntPtr h, IntPtr param);

    public static System.Collections.Generic.List<IntPtr> VisibleWindowsOf(uint wanted) {
        var found = new System.Collections.Generic.List<IntPtr>();
        EnumWindows(delegate(IntPtr h, IntPtr p) {
            uint pid; GetWindowThreadProcessId(h, out pid);
            if (pid == wanted && IsWindowVisible(h)) found.Add(h);
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@

# ---- find (or start) the window ----
$started = $null
if (-not $Attach) {
    if (-not (Test-Path $Exe)) { throw "Not built: $Exe" }
    $started = Start-Process $Exe -PassThru
    Write-Host ("started {0} (pid {1})" -f $Exe, $started.Id)
}

$deadline = (Get-Date).AddSeconds(30)
$hwnd = [IntPtr]::Zero
do {
    Start-Sleep -Milliseconds 400
    if ($started) { $proc = Get-Process -Id $started.Id -ErrorAction SilentlyContinue }
    else          { $proc = Get-Process -Name 'JASS' -ErrorAction SilentlyContinue | Select-Object -First 1 }
    if ($proc) { $hwnd = $proc.MainWindowHandle }
} while ($hwnd -eq [IntPtr]::Zero -and (Get-Date) -lt $deadline)

if ($hwnd -eq [IntPtr]::Zero) { throw "No JASS window appeared. Is it running? (use -Attach)" }

[void][JassWin]::ShowWindow($hwnd, 9)          # SW_RESTORE: never shoot a minimised window
[void][JassWin]::SetForegroundWindow($hwnd)
if ($Popup) { Write-Host "open the panel now -" -NoNewline }
Write-Host ("window is up - {0} s until the shutter" -f $Delay) -NoNewline
for ($i = 0; $i -lt $Delay; $i++) { Start-Sleep -Seconds 1; Write-Host '.' -NoNewline }
Write-Host ''

# The MODULES panel is a separate top-level window of the same process. Shooting that instead of a
# hand-measured crop keeps the picture right when the panel changes size.
if ($Popup) {
    $others = [JassWin]::VisibleWindowsOf([uint32] $proc.Id) | Where-Object { $_ -ne $hwnd }
    if (-not $others) { throw "No panel is open - nothing but the main window belongs to JASS." }
    $hwnd = $others | Select-Object -First 1
}

# ---- measure ----
$r = New-Object JassWin+RECT
if ([JassWin]::DwmGetWindowAttribute($hwnd, 9, [ref] $r, 16) -ne 0) {
    [void][JassWin]::GetWindowRect($hwnd, [ref] $r)   # ancient fallback; keeps the border
}
$x = $r.L; $y = $r.T; $w = $r.R - $r.L; $h = $r.B - $r.T

if ($Region) {
    $p = $Region -split '\s*,\s*'
    if ($p.Count -ne 4) { throw "-Region wants four numbers: x,y,w,h inside the window" }
    $x += [int]$p[0]; $y += [int]$p[1]; $w = [int]$p[2]; $h = [int]$p[3]
}
if ($w -le 0 -or $h -le 0) { throw ("Nothing to capture: {0} by {1}" -f $w, $h) }

# ---- shoot ----
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g   = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size $w, $h))
$g.Dispose()

$null = New-Item -ItemType Directory -Force -Path $OutDir
$out  = Join-Path (Resolve-Path $OutDir) ($Name + '.png')
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Write-Host ("wrote {0}  ({1} by {2} px, {3:N0} KB)" -f $out, $w, $h, ((Get-Item $out).Length / 1KB))

# A window we opened is ours to close; one we attached to belongs to whoever was using it.
if ($started) { Stop-Process -Id $started.Id -Force }
