# Roadmap

Where Arizona Vamp Recomp is going. The quality bar is
[BLACK-XBERecomp](https://github.com/TGP482/BLACK-XBERecomp), built on the same SDK. Day-to-day detail is in
[STATUS.md](STATUS.md).

## Done

- Native Windows build of the HLE port (Direct3D 8 on Direct3D 12, DirectSound on XAudio2).
- Boot, movies, front end, profiles, briefing and the first level, at 60 fps with sound.
- Graphics fixes: fonts and palettes, vertex-buffer fences, pushed vertex constants (lit models), fog streaks.
- Crash fixes: DirectSound after START, the LOADING screen, briefing CONTINUE.
- Keyboard and mouse with raw mouse look and rebinding.
- Shader disk cache: no compile hitches after the first run.
- In-game PC settings menu (F1, Back + Start, or PC SETTINGS in the game's own menus).
- F4 bug-report box and F3 frame captures.
- Releases ship no game code: the first start builds it from the player's own disc image or files.
- Optional PS2 alpha blending (no previous-frame ghosting); anisotropic filtering, 16x by default.

## Next

1. **Play-through QA.** Play every level, file F4 reports, fix what turns up. Compare against console footage.
2. **Field of view** and **viewmodel FOV** settings (in progress).
3. **Exit to Desktop** as a native entry in the game's own pause and quit menus.
4. Hide the leftover Xbox Live notices (the "SIGN-IN FAILED." line in levels).
5. F1 in gameplay also pauses the game.

## PC features (the BLACK list)

| Area | Feature | State |
|---|---|---|
| Display | Display mode (fullscreen / borderless / windowed) | done |
| Display | Resolution, render resolution 25-200% | done |
| Display | Frame-rate cap, V-Sync | done |
| Display | Frame-rate counter | done |
| Display | Field of view (gameplay), viewmodel FOV | in progress |
| Display | Monitor choice | planned |
| Display | FMV scaling (fill / 4:3) | planned |
| Display | HDR10 output | planned |
| Display | Ultrawide | planned, after the first release |
| Graphics | Anisotropic filtering | done |
| Graphics | MSAA 2x / 4x / 8x, SMAA | planned |
| Graphics | Draw distance | planned (game patch) |
| Graphics | Ambient occlusion | maybe (needs depth; hard) |
| Input | Keyboard and mouse, raw input, rebinding | done |
| Input | Mouse invert | planned |
| Input | Controllers through SDL3, gyro aiming | planned |
| General | Skip movies with Start / Escape | partly (the intro skips) |
| General | Pause on focus loss (game and sound) | planned |
| General | Custom menu cursor | planned |

## Performance and polish

- Compile shaders in the background (or ahead of time) so even the first run does not stutter.
- 3D sound: per-bin mix attenuation (`GetVoiceProperties` reports no mix bins yet).
- Track down the rare CRI movie-player crash (0 in 100 test runs, seen before).

## Platforms

- **Windows 10/11 x64:** main target, now.
- **macOS (Apple silicon):** after Windows. The SDK's headless harness already builds and reaches PRESS START;
  it needs a Metal or Vulkan renderer path and an audio backend.
- **Android:** after macOS.
