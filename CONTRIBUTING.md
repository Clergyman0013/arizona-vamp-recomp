# Contributing

Thanks for helping. The project is early, so bug reports from real play are the most useful thing right now.

## Reporting a bug

1. Press **F4** in the game when something looks or sounds wrong. Type what you saw and what you expected, then
   Save (Ctrl+Enter).
2. A folder appears in `reports\` beside `Darkwatch.exe`. Zip it.
3. Open an issue with the zip, the level and spot, and your Windows version and GPU.

If the game crashes, attach the newest `logs\` file and the `CrashDumps\` file, if there is one.

**Before you attach anything, look inside it.** Logs and reports can hold your Windows user name in file paths,
and your PC's hardware details; edit those out if you prefer. Never attach game files, disc images or anything
extracted from them.

### What counts as a bug

The Xbox version has richer lighting and effects than the PS2 version, so compare with Xbox footage where you can.
- **Bug:** missing or black geometry, stretched or smeared pixels, flicker, wrong colours, broken HUD or text,
  missing sound, crashes and freezes.
- **Probably not a bug:** an effect the PS2 version lacks, or a sharper image.

## Code

- Game-specific fixes go in `darkwatch-hle/src/game/` (hooks on single recompiled functions); PC menus and settings
  in `darkwatch-hle/src/common/`.
- Changes to the SDK go into `patches/xboxxberecomp-darkwatch.patch`, with a row in `patches/README.md` saying what
  changed and why. Regenerate the patch from `third_party/XboxXBERecomp` with `git diff`, using LF line endings.
- Keep commits to original code only: no generated code, no game data, no captures with game textures.
