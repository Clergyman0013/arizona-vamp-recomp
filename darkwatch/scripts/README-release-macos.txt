Darkwatch (Xbox) recompiled - macOS debug build (Apple silicon)
================================================================

This is the xboxrecomp port: the game's own NV2A graphics and APU audio are
emulated in software. It plays to the first level, slowly (about 6 fps in the
menus, 1.5 fps in the level). It is for debugging, not for playing.

No game data is included. You need your own copy of the game.

1. Extract the disc so that a folder named game_files sits beside this file,
   with default.xbe and the .pck files in it. (Or set game_dir= in
   darkwatch.ini, or the DARKWATCH_GAME_DIR environment variable.)
2. The first time: macOS blocks unsigned downloads. In Terminal, in this
   folder, run
       xattr -dr com.apple.quarantine .
3. Double-click Darkwatch.command (or run ./Darkwatch.command in Terminal).

Logs
----
Every run writes logs/darkwatch_<date>_<time>.log beside this file: all the
console output, crash reports included. The same folder gets the kernel log
(xbox_kernel.log, overwritten each run) and the indirect-call dumps.
Send the newest .log when reporting a problem.

Controls
--------
A connected game pad, or the keyboard:
  Enter = START        Backspace = BACK       Z/X/C/V = A/B/X/Y
  WASD = left stick    IJKL = right stick     arrows = D-pad
  1 / 3 = left / right trigger                Q / E = White / Black
Options are in darkwatch.ini.
