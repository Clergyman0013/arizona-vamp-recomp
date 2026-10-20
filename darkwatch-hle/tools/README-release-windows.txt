Darkwatch (Xbox) recompiled - Windows x64 debug build
=====================================================

This is the HLE port: the game's Direct3D 8 runs on Direct3D 12 and its
DirectSound on XAudio2. It is the first Windows build: it has been compiled
but not yet run on Windows. On macOS it reaches the PRESS START screen.
Expect problems; the logs are what is needed to fix them.

Needs Windows 10 or 11, 64-bit, and a GPU with Direct3D 12.
No game data is included. You need your own copy of the game.

1. Put the game in the assets folder beside Darkwatch.exe: either the disc
   image (.iso), or default.xbe with the game's other files beside it.
2. Run Darkwatch.exe. Windows SmartScreen may warn about an unknown app:
   More info, Run anyway.

Controls
--------
A controller works as on the console (Xbox, PlayStation and Switch pads).
The keyboard and mouse also work, as player 1:

  W A S D           move (left stick)
  mouse             look (right stick); I J K L also look
  left click        fire (right trigger)
  right click       left trigger
  Space / Enter     A          (jump, menu select)
  E / Backspace     B          (menu back)
  R                 X
  Q                 Y
  C / F             black / white
  Left Shift        left stick click
  middle click, V   right stick click
  arrow keys        d-pad
  Escape            Start (pause)
  Tab               Back
  F1                PC settings menu (also Back+Start on a controller)
  F2                free the mouse / take it back for looking
  F3                capture a frame (see below)
  F4                report a problem (see below)

Change the keys in the PC settings menu (F1, Controls), or in settings.toml
(written beside the exe on the first run),
under [keys]: one line per control, keys separated by commas, e.g.
  right_trigger = 'Mouse1,Ctrl'
Key names: letters, digits, F1-F12, Num0-Num9, Space, Enter, Escape, Tab,
Backspace, Shift/LShift/RShift, Ctrl/LCtrl/RCtrl, Alt/LAlt/RAlt, Up, Down,
Left, Right, Insert, Delete, Home, End, PageUp, PageDown, Comma, Period,
Mouse1-Mouse5. Also in settings.toml: keyboard = false turns keyboard and
mouse off, mouse_look = false keeps the mouse free, mouse_sensitivity is in
percent (100 default).

Logs
----
  logs\darkwatch_NNN.log      one per run, numbered, newest 30 kept; verbose
                              (debug build)
  CrashDumps\Darkwatch.*.dmp  written when it crashes
Both folders are created beside Darkwatch.exe. Send the newest log, and the
.dmp if there is one (with Darkwatch.pdb, which is in this zip).

If the game stops moving, press F3: every guest thread's stack goes into the
log. F3 saves the frame it captures to captures\ beside the exe (or the
XBE_CAPTURE_DIR folder, if that is set). Settings go in settings.toml
beside the exe.

Report a problem
----------------
  Press F4 anywhere: a box opens in the middle of the screen. Type what you
  saw (the game gets no input while it is open), then Save (or Ctrl+Enter)
  or Cancel (or Esc). Save writes reports\<date_time>\ beside the exe: the
  note with the date, time and seconds since launch, the frame from when
  you pressed F4, the last 300 lines of the log, and settings.toml. Send
  that folder.

PC settings
-----------
  F1 (or Back+Start on a controller) opens the PC settings menu anywhere;
  it is also PC SETTINGS in the pause menu and in the main menu's OPTIONS
  (where Xbox Live used to be). Pages: Display (mode, resolution, render
  resolution, frame rate cap, V-Sync), Graphics (PS2 alpha blending, anisotropic filtering: 16x by default),
  Controls (keyboard and mouse, sensitivity, key bindings), Game (frame rate
  counter). Changes apply at once; a new display mode asks to keep it and
  goes back after 15 s. Settings are saved when the menu closes. Exit to
  Desktop is at the bottom right. The game gets no input while it is open.
