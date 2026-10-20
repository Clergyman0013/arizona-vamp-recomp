# press.ps1 "<steps>": drive the game with the keyboard and mouse for QA, through Windows SendInput (the game reads it
# like a real keyboard and mouse; its window is brought to the front first).
#
# Steps, separated by ';':
#   KEY            tap a key (100 ms)            e.g.  Enter
#   KEY:MS         hold a key for MS ms           e.g.  W:2000
#   KEY+KEY:MS     hold several at once           e.g.  W+A:1500
#   wait:MS        do nothing for MS ms
#   look:DX,DY     move the mouse by DX,DY pixels (mouse look), spread over 200 ms
#   look:DX,DY,MS  the same over MS ms
#   f3             a frame capture (needs run_capture.bat)
# Keys: A-Z, 0-9, F1-F12, Space, Enter, Escape, Tab, Backspace, LShift, LCtrl, LAlt, Up, Down, Left, Right,
#       Mouse1 (left button), Mouse2 (right), Mouse3 (middle).
# Default bindings (settings.toml [keys]): Space/Enter A, Backspace/E B, R X, Q Y, Escape Start, Tab Back,
#   WASD left stick, mouse right stick, Mouse1 right trigger, Mouse2 left trigger, arrows d-pad.
#
# Example, title screen to the main menu:  press.ps1 "Escape; wait:3000; Enter; wait:2000"
param([Parameter(Mandatory = $true)][string]$Steps)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class Inp {
    [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx, dy; public uint data, flags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort vk, scan; public uint flags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Explicit)] public struct U { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public U u; }
    [DllImport("user32.dll")] static extern uint SendInput(uint n, INPUT[] inputs, int size);
    [DllImport("user32.dll")] static extern uint MapVirtualKey(uint code, uint type);
    public static void Key(ushort vk, bool up) {
        var i = new INPUT[1]; i[0].type = 1; i[0].u.ki.vk = vk; i[0].u.ki.scan = (ushort)MapVirtualKey(vk, 0);
        uint f = up ? 2u : 0u;
        if (vk >= 0x21 && vk <= 0x28) f |= 1u;  // arrows, page, home, end: extended keys
        i[0].u.ki.flags = f;
        SendInput(1, i, Marshal.SizeOf(typeof(INPUT)));
    }
    public static void Button(int b, bool up) {
        uint[] down = { 0x0002, 0x0008, 0x0020 }, rel = { 0x0004, 0x0010, 0x0040 };
        var i = new INPUT[1]; i[0].type = 0; i[0].u.mi.flags = up ? rel[b] : down[b];
        SendInput(1, i, Marshal.SizeOf(typeof(INPUT)));
    }
    public static void Move(int dx, int dy) {
        var i = new INPUT[1]; i[0].type = 0; i[0].u.mi.dx = dx; i[0].u.mi.dy = dy; i[0].u.mi.flags = 0x0001;
        SendInput(1, i, Marshal.SizeOf(typeof(INPUT)));
    }
}
"@

$vk = @{ space = 0x20; enter = 0x0D; escape = 0x1B; esc = 0x1B; tab = 0x09; backspace = 0x08; lshift = 0xA0;
         lctrl = 0xA2; lalt = 0xA4; up = 0x26; down = 0x28; left = 0x25; right = 0x27; pageup = 0x21; pagedown = 0x22 }
function Code([string]$k) {
    $n = $k.Trim().ToLower()
    if ($n -match '^mouse([1-3])$') { return @{ mouse = [int]$Matches[1] - 1 } }
    if ($n.Length -eq 1 -and $n -match '[a-z0-9]') { return @{ key = [int][char]$n.ToUpper() } }
    if ($n -match '^f([0-9]{1,2})$') { return @{ key = 0x70 + [int]$Matches[1] - 1 } }
    if ($vk.ContainsKey($n)) { return @{ key = $vk[$n] } }
    throw "unknown key '$k'"
}
function Hold($codes, [bool]$up) {
    foreach ($c in $codes) { if ($c.ContainsKey('mouse')) { [Inp]::Button($c.mouse, $up) } else { [Inp]::Key([uint16]$c.key, $up) } }
}

$p = Get-Process -Name Darkwatch -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { throw "Darkwatch is not running" }
$ws = New-Object -ComObject WScript.Shell
[void]$ws.AppActivate($p.Id)
Start-Sleep -Milliseconds 300

foreach ($step in ($Steps -split ';')) {
    $s = $step.Trim()
    if (-not $s) { continue }
    if ($s -match '^wait:(\d+)$') { Start-Sleep -Milliseconds ([int]$Matches[1]); continue }
    if ($s -ieq 'f3') { [Inp]::Key(0x72, $false); Start-Sleep -Milliseconds 80; [Inp]::Key(0x72, $true); Start-Sleep -Milliseconds 500; continue }
    if ($s -match '^look:(-?\d+),(-?\d+)(,(\d+))?$') {
        $dx = [int]$Matches[1]; $dy = [int]$Matches[2]; $ms = if ($Matches[4]) { [int]$Matches[4] } else { 200 }
        $n = [Math]::Max([int]($ms / 10), 1)
        for ($i = 0; $i -lt $n; $i++) {
            [Inp]::Move([int]($dx * ($i + 1) / $n) - [int]($dx * $i / $n), [int]($dy * ($i + 1) / $n) - [int]($dy * $i / $n))
            Start-Sleep -Milliseconds 10
        }
        continue
    }
    $ms = 100
    if ($s -match '^(.+):(\d+)$') { $s = $Matches[1]; $ms = [int]$Matches[2] }
    $codes = @($s -split '\+' | ForEach-Object { Code $_ })
    Hold $codes $false
    Start-Sleep -Milliseconds $ms
    Hold $codes $true
    Start-Sleep -Milliseconds 120  # the game polls about every frame: a gap between steps
}
"done: $Steps"
