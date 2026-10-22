Arizona Vamp Recomp - Darkwatch (Xbox), native Windows port - beta
==================================================================

A native PC port of Darkwatch for the original Xbox, made by static
recompilation: the game's Direct3D 8 runs on Direct3D 12 and its DirectSound
on XAudio2. This is a beta: the front end and the first level are tested,
the rest of the game is not yet. https://github.com/Clergyman0013/arizona-vamp-recomp

Needs Windows 10 or 11, 64-bit, a GPU with Direct3D 12, and about 3 GB of
free disk space. No game data or game code is included: you need your own
copy of Darkwatch for the original Xbox (USA release).

1. Put the game in the assets folder beside Darkwatch.exe: either your disc
   image (.iso), or default.xbe with the game's other files beside it.
2. Run Darkwatch.exe. Windows SmartScreen may warn about an unknown app:
   More info, Run anyway.
3. The first start builds the game's code for your PC from your own copy
   (the compiler folder), behind a progress window. This takes a while and
   happens only once; if it is stopped, the next start carries on. When it
   is done the game starts by itself after 10 seconds (Start, A, Enter: at
   once; Exit, B, Esc: not now). After that the game starts straight away.
4. Windows Firewall may ask whether Darkwatch.exe may use the network (the
   game's system link). Cancel is fine for single player.
5. The first time you play, shaders compile as the game meets them (short
   stutters). They are kept for your graphics driver; after a driver update
   they compile again at launch behind a progress bar. Skip (B, Esc) goes
   straight to the game; "Don't show this again" (X, Space) turns this off,
   and PC settings, Graphics, "Precompile shaders" turns it back on.

Two-player co-op
----------------
The PS2 version's split-screen campaign, left off the Xbox release, is back.
Main menu, MULTIPLAYER, CO-OP: both players press A on the profile screen,
then pick a chapter. Player 1 is the first controller (or the keyboard and
mouse), player 2 the second controller. Both players look like the
single-player hero for now.

Controls
--------
A controller works as on the console (Xbox, PlayStation and Switch pads).
The game's button prompts show the buttons of the controller in use:
PlayStation shapes, Switch letters, Xbox buttons for Xbox pads and the
keyboard (PC settings, Controls, Button prompts).
In the game's menus the mouse points: left click selects, right click goes
back, the wheel steps through the rows.
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
  logs\                       the game's log, and compile.log from the
                              first start's build
  CrashDumps\Darkwatch.*.dmp  written when it crashes, with the log of that run beside it (.log)
Both folders are created beside Darkwatch.exe. Send the newest log, and the
.dmp if there is one. Logs can hold your Windows user name in file paths:
edit it out if you prefer.

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
  resolution, frame rate cap, V-Sync), Graphics (PS2 alpha blending,
  anisotropic filtering: 16x by default, MSAA anti-aliasing, precompile
  shaders, field of view, weapon field of view, widescreen framing, reticle
  size, cut-scene borders), Controls (button prompts, keyboard and mouse,
  sensitivity, direct mouse aiming, mouse in menus, key bindings), Game
  (frame rate counter, War Train and Splitter grenades).
  Changes apply at once; a new display mode asks to keep it and goes back
  after 15 s. Settings are saved when the menu closes. Exit to Desktop is at
  the bottom right. The game gets no input while it is open.

  The same settings in settings.toml: fov and weapon_fov (degrees across a
  4:3 frame, 0 the game's 55), widescreen_fov, reticle_size (percent),
  cutscene_borders, mouse_direct_aim, bonus_content (War Train and the
  Splitter grenades; read at start), msaa (0 off, 2, 4, 8), button_prompts
  (0 the controller in use, 1 Xbox, 2 PlayStation, 3 Switch), mouse_menus.

  The main menu also has QUIT TO DESKTOP. Saves are kept in hdd\E beside
  Darkwatch.exe: copy that folder to keep them.
