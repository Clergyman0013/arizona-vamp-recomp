# goto_level1.ps1 - scripted boot from a closed game to the first level (THE WRONG TRAIN).
#
# Usage:  powershell -ExecutionPolicy Bypass -File darkwatch-hle\tools\goto_level1.ps1
#
# What it does:
#   1. Starts the Release game through run_capture.bat if it is not running, waits for the window.
#   2. Waits (F3 polls) until the intro movie or the title screen shows.
#   3. Plays the path with press.ps1: movie -Escape-> title -Escape-> PROFILES -A-> main menu
#      -A-> CHAPTER -A-> DIFFICULTY -A-> briefing -A-> briefing 2 (CONTINUE) -A-> level.
#   4. After every press it takes an F3 capture and classifies the newest captures\<HHMMSS>\frame.txt:
#      the texture-format/size multiset and the draw count tell the screens apart (see the checks in
#      Get-Screen below; markers: title = F@512x128 and no fmt-6 texture, PROFILES = fmt 6 + B@256x128 +
#      C@256x256 + F@512x128, main menu = fmt 6 without those, CHAPTER = + F@32x128, DIFFICULTY = + F@256x32,
#      briefing 2 = C@512x256 and no fmt 6, level = >50 draws with vertex program B0101EC1).
#      Every press is retried up to 3 times; then it stops with "stuck at <screen>".
#   5. If the game process is gone (the random CRI movie crash), it restarts the game once and starts
#      again from the top, and counts that.
#   6. Ends in the level with an F3 and prints the total time from launch to the level.
#
# Known limits:
#   - Needs the saved profile "PROFILE 1" (chapter THE WRONG TRAIN in progress, difficulty COWBOY); the
#     PROFILE screen keeps the last selection, so a different profile state changes the screens.
#   - Menu screens can ignore the first key for a while after a load (seen after the movie skip); the
#     retries cover that; boot to title can take ~90 s.
#   - One restart only: two crashes in one run stop the script.
#   - Captures need run_capture.bat (XBE_CAPTURE_DIR=captures); this script always starts the game that way.

param([int]$MaxRestarts = 1)

$ErrorActionPreference = 'Stop'
$tools = $PSScriptRoot
$rel = Join-Path $tools "..\out\build\release"
$captures = Join-Path $rel "captures"
$press = Join-Path $tools "press.ps1"
$runBat = Join-Path $tools "run_capture.bat"

function Log($msg) { Write-Host ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $msg) }

function Get-LatestCapture([string]$notThis) {
    $d = Get-ChildItem $captures -Directory -ErrorAction SilentlyContinue |
         Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $d) { return $null }
    if ($d.Name -eq $notThis) { return $null }
    if (-not (Test-Path (Join-Path $d.FullName "frame.txt"))) { return $null }
    return $d.Name
}

function Start-GameOnce {
    Log "starting the game through run_capture.bat"
    Start-Process -FilePath $runBat
    for ($i = 0; $i -lt 120; $i++) {
        Start-Sleep -Seconds 1
        $p = Get-Process -Name Darkwatch -ErrorAction SilentlyContinue
        if ($p -and $p.MainWindowHandle -ne 0) { Log "window up (pid $($p.Id))"; return $p }
    }
    throw "game window did not appear in 120 s"
}

function Press([string]$steps) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $press $steps | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "press.ps1 failed (game gone?)" }
}

function New-Capture([int]$timeoutSec = 20) {
    $before = Get-ChildItem $captures -Directory -ErrorAction SilentlyContinue |
              Sort-Object LastWriteTime | Select-Object -Last 1
    $beforeName = if ($before) { $before.Name } else { "" }
    Press "f3"
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalSeconds -lt $timeoutSec) {
        Start-Sleep -Milliseconds 500
        $new = Get-LatestCapture $beforeName
        if ($new) { return $new }
    }
    return $null
}

function Get-Screen([string]$captureName) {
    $f = Join-Path $captures "$captureName\frame.txt"
    $draws = 0; $tex = @(); $b101 = 0
    foreach ($l in [System.IO.File]::ReadAllLines($f)) {
        if ($l -match '^\d{4} draw') {
            $draws++
            if ($l -match 'vs B0101EC1') { $b101++ }
            if ($l -match '\| t0 \S+ fmt (\S+) (\d+x\d+)') { $tex += "$($Matches[1])@$($Matches[2])" }
            else { $tex += "none" }
        }
    }
    $has6   = $tex -contains "6@1024x512"
    $hasF512x128 = $tex -contains "F@512x128"
    $hasB256x128 = $tex -contains "B@256x128"
    $hasC256x256 = $tex -contains "C@256x256"
    $hasC512x256 = $tex -contains "C@512x256"
    $hasF32x128  = $tex -contains "F@32x128"
    $hasF256x32  = $tex -contains "F@256x32"
    $hasC256x32  = $tex -contains "C@256x32"
    $b256x256 = @($tex | Where-Object { $_ -eq "B@256x256" }).Count
    $only6 = ($draws -le 3) -and $has6 -and (@($tex | Where-Object { $_ -ne "6@1024x512" }).Count -eq 0)

    $name =
        if ($b101 -ge 50)            { "level" }        # B0101EC1 RenderWare world draws
        elseif ($only6)              { "fullscreen" }   # movie or briefing page 1 (context tells)
        elseif (-not $has6 -and $hasF512x128 -and $draws -le 8) { "title" }
        elseif ($has6 -and $hasF512x128 -and $b256x256 -ge 2 -and $b256x256 -le 3 -and -not $hasB256x128) { "demo" }
        elseif ($has6 -and $hasB256x128 -and $hasF512x128 -and $hasC256x256) { "profiles" }
        elseif ($has6 -and $hasB256x128 -and $hasF32x128) { "chapter" }
        elseif ($has6 -and $hasF256x32 -and $hasC256x32) { "difficulty" }
        elseif ($has6 -and -not $hasB256x128 -and -not $hasC256x256 -and $draws -ge 4) { "menu" }
        elseif (-not $has6 -and $hasC512x256 -and $draws -ge 3) { "briefing2" }
        else                         { "unknown" }
    return @{ Name = $name; Draws = $draws; Tex = ($tex | Group-Object | ForEach-Object { "$($_.Name)x$($_.Count)" }) -join " " }
}

function Wait-ForScreen([string[]]$want, [int]$timeoutSec) {
    $t0 = Get-Date; $last = $null
    while (((Get-Date) - $t0).TotalSeconds -lt $timeoutSec) {
        if (-not (Get-Process -Name Darkwatch -ErrorAction SilentlyContinue)) { throw "crash: game process gone (waiting for $($want -join '/'))" }
        $cap = New-Capture 15
        if (-not $cap) { continue }
        $last = Get-Screen $cap
        if ($want -contains $last.Name) { return @{ Cap = $cap; Screen = $last } }
        Log "  saw '$($last.Name)' (draws $($last.Draws)), wanted $($want -join '/')"
        Start-Sleep -Seconds 2
    }
    $seen = if ($last) { $last.Name } else { "nothing" }
    throw "stuck at $seen (wanted $($want -join '/'))"
}

function Do-Step([string]$label, [string]$keys, [string[]]$targets, [int]$timeoutSec = 12, [int]$retries = 3) {
    for ($try = 1; $try -le $retries; $try++) {
        $pre = New-Capture 15
        if ($pre) {
            $s = Get-Screen $pre
            if ($targets -contains $s.Name) { Log "$label -> already $($s.Name) (capture $pre)"; return @{ Cap = $pre; Screen = $s } }
        }
        Log "$label -> press '$keys' (try $try)"
        Press $keys
        $r = $null
        try { $r = Wait-ForScreen $targets $timeoutSec } catch { }
        if ($r) { Log "$label -> $($r.Screen.Name)  ok  (capture $($r.Cap))"; return $r }
    }
    throw "stuck at $label (last screen not $($targets -join '/'))"
}

# ---- the run ---------------------------------------------------------------------------------------

$launchAt = Get-Date
$restarts = 0
$crashNote = ""
$levelCapture = $null

while ($true) {
    $fatal = $null
    try {
        $p = Get-Process -Name Darkwatch -ErrorAction SilentlyContinue
        if (-not $p) { Start-GameOnce | Out-Null } else { Log "game already running (pid $($p.Id))" }

        $r = Wait-ForScreen @("fullscreen","title") 120     # boot logos/no signal -> movie or title
        if ($r.Screen.Name -eq "fullscreen") {
            Do-Step "movie->title" "Escape" @("title") 90 | Out-Null   # the title can take ~1 min to load
        } else {
            Log "already at title"
        }

        Do-Step "title->profiles"  "Escape" @("profiles") 12 | Out-Null
        Do-Step "profiles->menu"   "Enter"  @("menu") 12 | Out-Null
        Do-Step "menu->chapter"    "Enter"  @("chapter") 12 | Out-Null
        Do-Step "chapter->difficulty" "Enter" @("difficulty") 12 | Out-Null
        Do-Step "difficulty->briefing" "Enter" @("fullscreen") 12 | Out-Null
        Do-Step "briefing->briefing2" "Enter" @("briefing2") 12 | Out-Null

        Log "briefing2 -> press 'Enter' (load the level)"
        Press "Enter"
        $r = Wait-ForScreen @("level") 45
        $levelCapture = $r.Cap
        Log "in the level, capture $levelCapture"
    }
    catch { $fatal = $_.Exception.Message }

    if ($fatal -and $fatal -like "crash:*") {
        $crashNote += "restart at '$fatal'; "
        if ($restarts -lt $MaxRestarts) {
            $restarts++
            Log "GAME GONE: $fatal"
            Log "restart #$restarts -> starting again from the top"
            continue
        }
        Log "GAME GONE again, no restarts left"
        Write-Output "FAILED: $fatal (restarts: $restarts)"
        exit 1
    }
    if ($fatal) { Write-Output "FAILED: $fatal (restarts: $restarts)"; exit 1 }
    break
}

$total = [int]((Get-Date) - $launchAt).TotalSeconds
Write-Output ("DONE: in the level at capture {0}; total {1} s ({2} min); restarts: {3} {4}" -f `
    $levelCapture, $total, [int]($total/60), $restarts, $crashNote)
