# Reference images

- `title_xbox.png` – DARKWATCH title (skull emblem, logo, PRESS START). Original Xbox capture, video https://www.youtube.com/watch?v=9b5AlngJvSw @ 01:28 (2026-10-17). **Correct.**
- `new-profile_xbox.png` – NEW PROFILE name-entry (letter grid 0-9 / A-J / K-T / U-Z, SPACE / DELETE / DONE). Original Xbox capture, same video @ 01:34 (2026-10-17). **Correct.**
- `current/new-profile_hle_2026-10-17.webp` – NEW PROFILE as the HLE port shows it: scrambled letters (`O Z Y Z ...`), blocky dotted font, black boxes behind text. HLE release build, 2026-10-17. **Current / wrong.**
- `current/new-profile_hle_2026-10-17.png` – same screen, full window screenshot. HLE release build, 2026-10-17. **Current / wrong.**
- `loading_xbox.png` – LOADING screen (burning face, "LOADING"). Original Xbox capture, video 9b5AlngJvSw @ 01:31 (2026-10-17). **Correct.**
- `mainmenu_xbox.png` – main menu (SINGLE PLAYER / MULTIPLAYER / OPTIONS / PROFILES / EXTRAS), same video @ 01:36. **Correct.**
- `chapter_xbox.png` – CHAPTER select (THE WRONG TRAIN). Same video @ 01:43. **Correct.**
- `difficulty_xbox.png` – DIFFICULTY (GREENHORN / COWBOY / SHOOTIST / DEADEYE). Same video @ 01:49. **Correct.**
- `briefing_xbox.png` – briefing text scroll ("You are Jericho Cross..."). Same video @ 02:01. **Correct.**
- `briefing-continue_xbox.png` – "PRESS A TO CONTINUE" caption screen. Same video @ 02:51. **Correct** (HLE crashes on A here, task-004).
- `level1-start_xbox.png` – first gameplay view, THE WRONG TRAIN (HUD, revolver, CHECKPOINT). Longplay capture, 2026-10-18. **Correct.**
- `level1-door_xbox.png` – the door with "USE Ⓑ TO MELEE ATTACK THE DOOR LOCK". Same source. **Correct.**
- `level1-firing_xbox.png` – firing, muzzle flash (ammo 36). Same source. **Correct.**
- `level1-enemy_xbox.png`, `level1-enemy-2_xbox.png` – visible enemies (HLE renders these invisible/clipping). Same source. **Correct.**
- `level1-meatlocker-fog_xbox.png` – the meat-locker fog corridor (hanging carcasses, fog on the floor). Owner's Xbox hardware capture, 2026-10-18 11:50. **Correct** – no vertical lines; the reference for the open HLE streak bug.
- `level1-meatlocker-fog_ps2.png` – the same spot on the PS2 emulator (owner capture, 11:49). **Correct** – no vertical lines either.
- `current/new-profile_hle_2026-10-17_text-fixed.png` – NEW PROFILE after the vertex-buffer fence fix: letters right, P8 font still blocky with black glyph boxes and a broken A icon. HLE release build F3 capture, 2026-10-17. **Current / partly wrong.**
- `current/new-profile_hle_2026-10-17_font-fixed.png` – NEW PROFILE after the SetPalette fix: matches `new-profile_xbox.png`. HLE release build F3 capture, 2026-10-17. **Correct.**
- `current/gameplay_hle_2026-10-18.png` – first level (THE WRONG TRAIN) after the BeginPush/EndPush fix: HUD, health, ammo 36, crosshair. HLE release build F3 capture, 2026-10-18. **Current.**
- `current/level1-gun_hle_2026-10-18_black.png` – gun and hand drawn black (vertex constants the title pushes itself were lost). HLE release, 2026-10-18. **Wrong.**
- `current/level1-enemy_hle_2026-10-18_lit.png` – after the pushed-constants fix: lit gun, hand and enemy, close to the longplay. HLE release, 2026-10-18. **Current.**
- `current/level1-streaks_hle_2026-10-18.png` – vertical streaks in the fog, meat-locker corridor. HLE release, 2026-10-18. **Wrong.**
- `current/level1-streaks_hle_2026-10-18_haze-skipped.png` – same view with the five previous-frame haze draws skipped (drawskip.txt): no streaks. Diagnostic, 2026-10-18.
- `current/task-011/ghost-fog-crosshair_hle.png` – meat-locker fog after the streak fix: faint crosshair ghost below-right. HLE release, 2026-10-18. **Suspect** (task-011).
- `current/task-011/ghost-door-hit_hle.png` – first door melee hit: whole screen doubled by the fog pass. HLE release, 2026-10-18. **Suspect** (task-011).

Frame-capture folders (F3 dumps with the game's textures and shaders) are kept locally and not published.
