# _uitools.ps1 — reusable UI-automation + screenshot helpers for documenting PcapUdpExtractor.
# Dot-source this at the top of each automation call:  . C:\GitHub\pcapfile\docs\manual\_uitools.ps1
# All coordinates are PHYSICAL pixels (the process is made DPI-aware on load).

Add-Type -AssemblyName System.Windows.Forms, System.Drawing | Out-Null

if (-not ([System.Management.Automation.PSTypeName]'UiTools.Native').Type) {
  Add-Type @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
namespace UiTools {
  public struct RECT { public int Left, Top, Right, Bottom; }
  public class Native {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern int GetWindowTextLength(IntPtr h);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h, int attr, out RECT r, int size);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    public static List<IntPtr> TopWindowsForPid(uint target) {
      var list = new List<IntPtr>();
      EnumWindows((h, l) => {
        if (!IsWindowVisible(h)) return true;
        uint p; GetWindowThreadProcessId(h, out p);
        if (p == target) list.Add(h);
        return true;
      }, IntPtr.Zero);
      return list;
    }
  }
}
'@
}

[void][UiTools.Native]::SetProcessDPIAware()

function Get-VisibleRect([IntPtr]$h) {
  # True visible frame (excludes the invisible Win10/11 drop-shadow border).
  $r = New-Object UiTools.RECT
  $ok = [UiTools.Native]::DwmGetWindowAttribute($h, 9, [ref]$r, 16)  # DWMWA_EXTENDED_FRAME_BOUNDS
  if ($ok -ne 0) { [void][UiTools.Native]::GetWindowRect($h, [ref]$r) }
  [pscustomobject]@{ X=$r.Left; Y=$r.Top; W=($r.Right-$r.Left); H=($r.Bottom-$r.Top); Handle=$h }
}

function Get-TopWindows([int]$procId) {
  $hwnds = [UiTools.Native]::TopWindowsForPid([uint32]$procId)
  $out = @()
  foreach ($h in $hwnds) {
    $len = [UiTools.Native]::GetWindowTextLength($h)
    $sb = New-Object System.Text.StringBuilder ($len + 2)
    [void][UiTools.Native]::GetWindowText($h, $sb, $sb.Capacity)
    $rc = Get-VisibleRect $h
    $out += [pscustomobject]@{ Handle=$h; Title=$sb.ToString(); X=$rc.X; Y=$rc.Y; W=$rc.W; H=$rc.H }
  }
  $out
}

function Set-Foreground([IntPtr]$h) {
  [void][UiTools.Native]::ShowWindow($h, 9)   # SW_RESTORE
  [void][UiTools.Native]::SetForegroundWindow($h)
  Start-Sleep -Milliseconds 250
}

function Move-Window([IntPtr]$h, [int]$x, [int]$y) {
  # SWP_NOSIZE | SWP_SHOWWINDOW, insert at HWND_TOP. Account for the invisible
  # shadow border so the VISIBLE frame lands exactly at (x,y).
  $wr = New-Object UiTools.RECT; [void][UiTools.Native]::GetWindowRect($h, [ref]$wr)
  $vr = Get-VisibleRect $h
  $dx = $wr.Left - $vr.X; $dy = $wr.Top - $vr.Y
  [void][UiTools.Native]::SetWindowPos($h, [IntPtr]::Zero, ($x + $dx), ($y + $dy), 0, 0, (0x0001 -bor 0x0040))
  Start-Sleep -Milliseconds 200
}

function Capture-Region([int]$x, [int]$y, [int]$w, [int]$h, [string]$path) {
  $bmp = New-Object System.Drawing.Bitmap($w, $h)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size($w, $h)))
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  Get-Item $path
}

function Capture-Window([IntPtr]$h, [string]$path, [int]$pad = 0) {
  $rc = Get-VisibleRect $h
  Capture-Region ($rc.X - $pad) ($rc.Y - $pad) ($rc.W + 2*$pad) ($rc.H + 2*$pad) $path
}

function Capture-WindowPrint([IntPtr]$h, [string]$path) {
  # Renders the FULL window (incl. off-screen / clipped parts) via PrintWindow + PW_RENDERFULLCONTENT.
  $rc = Get-VisibleRect $h
  # PrintWindow draws relative to the full GetWindowRect (with shadow border); use that size then trim.
  $wr = New-Object UiTools.RECT
  [void][UiTools.Native]::GetWindowRect($h, [ref]$wr)
  $w = $wr.Right - $wr.Left; $hgt = $wr.Bottom - $wr.Top
  $bmp = New-Object System.Drawing.Bitmap($w, $hgt)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $hdc = $g.GetHdc()
  [void][UiTools.Native]::PrintWindow($h, $hdc, 2)  # PW_RENDERFULLCONTENT
  $g.ReleaseHdc($hdc); $g.Dispose()
  # Trim the invisible shadow border so output matches the DWM visible frame.
  $padL = $rc.X - $wr.Left; $padT = $rc.Y - $wr.Top
  if ($padL -lt 0) { $padL = 0 }; if ($padT -lt 0) { $padT = 0 }
  $cw = [Math]::Min($rc.W, $w - $padL); $ch = [Math]::Min($rc.H, $hgt - $padT)
  if ($cw -le 0) { $cw = $w }; if ($ch -le 0) { $ch = $hgt }
  $crop = New-Object System.Drawing.Bitmap($cw, $ch)
  $cg = [System.Drawing.Graphics]::FromImage($crop)
  $cg.DrawImage($bmp, (New-Object System.Drawing.Rectangle(0,0,$cw,$ch)), $padL, $padT, $cw, $ch, [System.Drawing.GraphicsUnit]::Pixel)
  $cg.Dispose(); $bmp.Dispose()
  $crop.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $crop.Dispose()
  Get-Item $path
}

function Click-At([int]$x, [int]$y) {
  [void][UiTools.Native]::SetCursorPos($x, $y)
  Start-Sleep -Milliseconds 80
  [UiTools.Native]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)  # LEFTDOWN
  Start-Sleep -Milliseconds 40
  [UiTools.Native]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)  # LEFTUP
  Start-Sleep -Milliseconds 150
}

function Send-Keys([string]$keys) {
  [System.Windows.Forms.SendKeys]::SendWait($keys)
  Start-Sleep -Milliseconds 200
}
