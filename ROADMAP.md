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
- The random crash when a movie ends or is skipped (a CRI movie-player race) is fixed.
- Shader pre-compile at launch, checked against the graphics driver: a new driver recompiles behind a progress
  bar (Skip, Don't show again) instead of stuttering in the game.
- In-game PC settings menu (F1, Back + Start, or PC SETTINGS in the game's own menus).
- F4 bug-report box and F3 frame captures.
- Releases ship no game code: the first start builds it from the player's own disc image or files.
- Optional PS2 alpha blending (no previous-frame ghosting); anisotropic filtering, 16x by default.
- Field of view and weapon field of view, widescreen framing (Hor+), reticle size, cut-scene borders on or off.
- Direct mouse aiming on foot (no stick curve, no aim assist).
- Actor scripts run above 100 fps (the game skipped frames shorter than 10 ms).
- QUIT TO DESKTOP in the main menu.
- The War Train chapter and the Splitter grenades the Xbox disc leaves off (selectors swapped as the packs are
  read).
- **Two-player co-op campaign** (the PS2 version's split screen, switched off on the Xbox disc).
- Button prompts for the controller in use (PlayStation shapes, Switch letters).
- Mouse in the game's menus: point, click, right click to go back, the wheel.
- MSAA 2x / 4x / 8x.
- The front end's Xbox Live status line ("Sign-in failed.") is gone.

## Next

1. **Play-through QA.** Play every level, file F4 reports, fix what turns up. Compare against console footage.
   Includes War Train and the Splitter pickups, which have not been played yet.
2. **Co-op:** player 2's own model (the PS2 shows them as a Darkwatch Regulator), a full co-op play-through.
3. Real PlayStation and Switch pads checked on the prompts; pictures for their triggers and shoulder buttons too.
4. Direct mouse aiming on the horse (it uses the mouse as the stick for now).
5. Keep the HUD and menus in proportion on a wide screen (they are stretched from 4:3).
6. Exit to Desktop in the pause menu too.
7. F1 in gameplay also pauses the game.

## PC features (the BLACK list)

| Area | Feature | State |
|---|---|---|
| Display | Display mode (fullscreen / borderless / windowed) | done |
| Display | Resolution, render resolution 25-200% | done |
| Display | Frame-rate cap, V-Sync | done |
| Display | Frame-rate counter | done |
| Display | Field of view (gameplay), viewmodel FOV | done |
| Display | Widescreen framing (Hor+), reticle size, cut-scene borders | done |
| Display | Monitor choice | planned |
| Display | FMV scaling (fill / 4:3) | planned |
| Display | HDR10 output | planned |
| Display | Ultrawide | planned, after the first release |
| Graphics | Anisotropic filtering | done |
| Graphics | MSAA 2x / 4x / 8x | done |
| Graphics | SMAA | planned |
| Graphics | Draw distance | planned (game patch) |
| Graphics | Ambient occlusion | maybe (needs depth; hard) |
| Input | Keyboard and mouse, raw input, rebinding | done |
| Input | Mouse invert | planned |
| Input | Direct mouse aiming | done (on foot; the horse later) |
| Input | Controllers through SDL3 (Xbox, PlayStation, Switch, Joy-Cons) | done, with their button prompts (face buttons) |
| Input | Gyro aiming | planned |
| Input | Mouse-driven game menus | done |
| General | Two-player co-op campaign | done (player 2's model later) |
| General | Skip movies with Start / Escape | partly (the intro skips) |
| General | Pause on focus loss (game and sound) | planned |
| General | Custom menu cursor | planned |

## Performance and polish

- Compile shaders in the background so even the very first run does not stutter (later runs and driver
  updates are covered by the launch pre-compile).
- 3D sound: per-bin mix attenuation (`GetVoiceProperties` reports no mix bins yet).

## Platforms

- **Windows 10/11 x64:** main target, now.
- **macOS (Apple silicon):** after Windows. The SDK's headless harness already builds and reaches PRESS START;
  it needs a Metal or Vulkan renderer path and an audio backend.
- **Android:** after macOS.
