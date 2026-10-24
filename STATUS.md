# Status

The one-page truth: where the project is, what blocks it, what is next.
Evidence and history: [BRINGUP.md](BRINGUP.md). Decisions and order: [PLAN.md](PLAN.md).
Task briefs and run logs are kept out of the public repository; their results are summarised here.

Last checked: 2026-10-20, on a Windows 11 PC.

## Focus

**Windows first.** The main line is the HLE port, `darkwatch-hle/` on
[TGP482/XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp) (D3D8 on D3D12,
DirectSound on XAudio2), the same toolkit as the BLACK recomp that sets the
quality bar. macOS waits until Windows is done.

The xboxrecomp port, `darkwatch/`, stays as the reference only: it reaches the
first level, but at ~1.6 fps in a software rasteriser. Use it to see what
"right" looks like; do not spend new work on it.

## Road to BLACK quality (owner goal, 2026-10-18)

Target: the level of [BLACK-XBERecomp](https://github.com/TGP482/BLACK-XBERecomp),
including **in-game PC menus** (video, key bindings, mouse, audio). Feasible:
same SDK, and the SDK already draws an ImGui layer over the game through a
per-game callback (`app::kGame.ui`, `runtime/gpu/d3d12.cpp` `RenderUi`);
most settings already live in `settings.toml`. Widescreen already works;
ultrawide is an optional goodie after the first release.

**Order (owner, 2026-10-18): features first, then the play-through.**

1. ~~**Shader cache** (task-010)~~ done 2026-10-19 (warm runs; a first run still compiles): disk cache, pipeline library, then
   precompile at start / background compile so even a first run does not hitch.
   Pre-compile at launch with driver check done 2026-10-20 (see below); the very first run still compiles.
2. ~~**Bug-report hotkey** (task-012)~~ done 2026-10-19: one key opens an in-game note box;
   Enter saves an F3 capture, the note, the time and the log tail to a
   `reports\` folder. Game input is blocked while typing.
3. ~~**In-game menu frame** (senior)~~ done 2026-10-19, see "PC settings menu"
   below. Left: a game-native **EXIT TO DESKTOP** entry in the pause and main
   menus (today it is the menu's own button, one step in); F1 in gameplay does
   not pause the game; the controller path is untested.
4. **Feature pages**, the BLACK list (below), as junior tasks one by one.
5. **Play-through QA** with the hotkey: the owner plays, files reports;
   junior and senior fix as they come. Ghosting (task-011) and the CRI
   crash watch fit in where convenient.

**BLACK feature list and where we stand** (have = in `settings.toml` now,
menu still needed):

| Area | Feature | Now |
|---|---|---|
| Display | Monitor pick | no |
| Display | Display mode (fullscreen/borderless/windowed) | have (`display_mode`) |
| Display | Resolution (monitor modes, desktop default) | have (`width`/`height`, 0 = desktop) |
| Display | Render resolution 20-200% | have (`render_percent`) |
| Display | Field of view 45-140 (gameplay) | no; game patch (camera) |
| Display | Viewmodel FOV | no; game patch |
| Display | Max frame rate (refresh / custom / off) | have (`fps_cap`, 0 = off) |
| Display | V-Sync | have (`vsync`) |
| Display | HDR10 output | no |
| Display | FPS counter | have (`show_fps`, Game page) |
| Display | FMV scale (fill / 4:3) | no; check how movies draw |
| Graphics | MSAA 2x/4x/8x | have (`msaa`, Graphics page) |
| Graphics | SMAA | no |
| Graphics | Anisotropic filtering | have (`anisotropic_filtering`, default 16x) |
| Graphics | Draw distance 2x-16x | no; game patch (LOD / far clip) |
| Graphics | Ambient occlusion (SSAO/GTAO) | no; needs depth, hard |
| Input | Mouse and keyboard, raw input, rebinding | have, in the menu (no invert yet) |
| Input | Controller via SDL3 | check what the SDK does now |
| Input | Gyro aiming | no (SDL3 sensors) |
| General | Skip videos with Start / Escape | check (Escape skips the intro now) |
| General | Pause on focus loss (game + sound) | no |
| Misc | Custom menu cursor (`cursor.png`) | no |

### Train boss crash, sound pool headroom (2026-10-24, v0.5.4)

- Issue #2: a crash in the first boss fight on the train, 558 s in. Two threads were inside XACT's timed-event heap
  at once: the game thread stopping a sound and XACT's timer DPC. XACT guards that heap by raising to dispatch
  level, which on the console's single processor keeps every DPC out; the SDK only recorded the level and runs DPCs
  on their own host thread. Now one recursive lock is held from a raise to dispatch level to the lower below it, and
  by every DPC and the vertical blank callback while they run; a wait that can block lets it go (SDK patch,
  `runtime/kernel`, `runtime/xdk/d3d8.cpp`). About 7000 raises a second, nearly all on XACT's thread, at no
  measurable cost; no deadlock and no blocking wait at dispatch level in 5 runs. The boss crash itself was not
  reproduced here.
- The streamed 3D sound pool has 32 voices (93 XACT 3D sources, one more per added voice). At most 8 were in use at
  once in testing, so this is headroom; 7 already turned none away.

### Horse cry, menu music, PC SETTINGS in the game's menus (2026-10-24, v0.5.3)

- The horse in THE WRONG TRAIN went quiet again, only some of the time. The sound manager has three voices for
  streamed 3D sounds (`sub_00206044` hands them out by priority, then distance). Two loops hold two of them for the
  whole level from its start, so every other streamed one-shot shared the third, and one farther away than the sound
  on it was turned away: 16 of 17 at the level start. The horse asks once, so it played only when the third voice was
  free. The pool has six now, with three more XACT voices and streams (`src/game/sound_pool.cpp`): 2 of 126 turned
  away. Darkwatch: Darker Edition found the same pool and enlarged it the same way; our code is our own.
- The menu music crackled every minute: the vertical blank wait slept a fixed 16 ms, so the movie's sound decoder,
  which waits on it, fell behind until its buffer ran dry. The wait keeps a 60 Hz grid now (SDK patch,
  `runtime/xdk/d3d8.cpp`).
- PC SETTINGS is the game's own page in OPTIONS and the pause menu (`src/game/settings_menu.cpp`), with left and right
  from the arrow keys, the d-pad and the left stick. Fixed with it: key rebinding took the key still held from the
  menu, the page did not follow F1 changes, and the mouse picked the wrong row on short pages and ran long lists to
  their end. Both field of view settings run 55 (the game's) to 110.

### Black screen under Proton (2026-10-22, issue #1)

- Under Proton (Sarek, CachyOS, Steam Deck) the game played sound over a black screen. Proton's `D3DCompile` is
  Wine's, built on vkd3d-shader, and it takes less HLSL than Microsoft's; the same report on the DOA3 recomp was its
  compiler rejecting `isfinite` and `SV_ClipDistance`. Ours wrote infinity as `1.#INF`, a literal only Microsoft's
  compiler reads, in the helpers every vertex shader includes, so no vertex shader would build there. The helpers
  now use `asfloat(0x7F800000)`, and three other risks went the same way: vector conditions in `?:` (`mac_mul`,
  `sbyte`), `fwidth` in the final blit and `reflect` in fixed function texture generation, each written out with
  the same result (SDK patch, `runtime/gpu/shaders.cpp`, `d3d12.cpp`). Windows renders as before. Not tested under
  Proton here; the reporter is asked to test.

### Short streamed sounds (2026-10-21)

- The horse in THE WRONG TRAIN made no sound. The game asks for it (`HORSE_WAKE01`, from the horse's wake animation)
  and XACT takes the cue, but its wave is in a streamed bank (`EMITTER_BASE_S`; the `_S` banks stream from disc, the
  `_M` banks sit in memory). XACT starts a stream once a read is pending; our file reads finished inside the call,
  so a wave that fits in one read (under 36 KB) never started. At the level start 65 of 164 streams never played:
  short emitter sounds, voice barks, the horse. Polled reads now stay pending for 2 ms (SDK patch,
  `runtime/kernel/files.cpp`). All streams start now; the owner heard the horse, music and voices unchanged.
- Junior task-018's "the cue never fires" and "melee does no damage" were wrong: the cue fires (the junior only
  logged buffers, not streams), and melee was the automated walk missing the lock. Notes are local.

### Field of view fix, mouse look logging (2026-10-21, v0.5.1)

- The field of view setting widened the view across only, which squeezed the world sideways. It now widens width
  and height together (as BLACK-XBERecomp does); Hor+ still adds width only. Checked at 90 in THE WRONG TRAIN.
- F4 reports now log window focus changes (with the class of the window that took the foreground), mouse look
  on/off and the mouse-menu pointer, to find why mouse look sometimes stops until a click. Open: a window that takes
  the foreground (a toast, an overlay) stops mouse look until the game is clicked; the game cannot take it back.
- Junior task-014 (a Regulator body for player 2) stopped at its off-ramp: `CoOpChaz` is never used at spawn, and the
  co-op resource lists hang LOADING. Notes are local.

### PC features after Darkwatch: Darker Edition (2026-10-20)

Its feature list (another port on the same SDK and the same USA disc), less its HD artwork, done our own way:

- **100 fps script gate:** LuaComponent's update (`sub_0002A359`) resumed actor scripts only for a frame of at least
  0.01 s; a shorter frame now counts as 0.01. Seen firing at 144 fps (8 ms frames). WrongTrain's first door at 144,
  which Darker Edition reports stayed shut, not walked to yet.
- **War Train and Splitter grenades:** three 2-byte selectors (`src/common/content.cpp`), swapped as the packs
  are read (SDK `GameInfo::read`). The level list and pickup templates are in packfile.pck *and* SinglStr.pck,
  and the campaign reads SinglStr.pck's: both are patched. The patch is seen applying; War Train itself is mid-game
  and the chapter screen shows unlocked chapters only, so it has not been seen in the list or played yet.
- **Field of view, weapon FOV, Hor+:** the player camera (made by `sub_000F9B1D`) gets its RenderWare view window
  set again at each BeginUpdate (`sub_00216AF7`) from its own angle, times the setting's tan ratio and, on a wide
  screen, 4/3 more across. The game's own window is 4:3 (0.52 x 0.39 at 55 degrees). The first-person pass
  (`sub_000DEDD0`) gets the weapon setting. Checked at 75, 85 and 90. At wide angles a wall right beside the player
  can show cut by the near clip plane (black). Menus and the HUD stay stretched.
- **Direct mouse aiming:** yaw `sub_000A2725` and pitch `sub_000A28AE` add the raw mouse counts (0.06 degrees a
  count at 100%) after the game's own turn; aim assist (look+0x5C) and pitch leveling (+0x74) are off while the
  mouse aims. Single player only; the horse (`sub_000BE0A9`, a matrix path) keeps the mouse as the stick.
  Only the on-foot controller's look calls (`sub_000A83DD`, return 0xA84A1 / 0xA84B0) aim directly: the other
  movement controllers (`sub_000A1AC5`, `sub_000A1042`) call the same look, and taking the mouse there left a
  player with no mouse aim on the horse (2026-10-22 report; the horse ride itself not replayed here). The log names
  each controller that turns the view once (`mouse: look turned from`).
  Vampire jumps not tried.
- **Reticle size:** ReticleDisplay (vtable `0x00428378`) through the HUD sprite draw `sub_000DCCDB`. Checked at 150%.
- **Cut-scene borders:** the cinematic manager's letterbox effect (`[0x004B7A68]`, draw `sub_00326CD1`) is skipped
  when off. Not seen on screen yet.
- **QUIT TO DESKTOP** in the main menu (`src/game/menu_hooks.cpp`): a row made in code after the main page builds
  (`sub_000EA7F6`). Gotcha: a text element made in code is 0x80 bytes; copying one field past that from a
  front-end row corrupted the heap and crashed when the main page was freed.
- **Controllers:** already through SDL3 (PlayStation, Switch, Joy-Cons paired by SDL's default). Button prompts,
  mouse menus and MSAA followed the same day (below).

### Co-op public, button prompts, mouse menus, MSAA (2026-10-20, v0.5.0)

- **Co-op** (`src/game/coop_hooks.cpp`, was a local branch): MULTIPLAYER takes the PS2 link (VERSUS / CO-OP popup,
  `sub_000EA939`); co-op levels use the single-player `_StreamFile.str` and `_resources` lists (`sub_000CC4C4`);
  ports 3 and 4 report plugged in once CO-OP is picked. Checked again on master: profile screen, chapter, WrongTrain
  split screen with two players (player 2 a test pad, `XBE_COOP_P2`). Both players use the single-player model;
  the PS2's Regulator model for player 2 is open (task-014). No full co-op play-through yet.
- **Button prompts** (`src/common/prompts.cpp`): a prompt token (`^MenuSelect`) becomes a lower-case character of
  the game font through `sub_001EA201` and the button-to-character table at `0x43BBB0` (ordinary text is made upper
  case, so a-z are free): `a` X, `b` Y, `c` A, `d` B. Both fonts (white, and red for a picked row) are 256x256 P8
  with the four pictures at x 71/93/115/138, y 35, 13x14 texels. They are found by their colours as the renderer
  decodes them (SDK `GameInfo::texture`) and drawn again: PlayStation shapes, Switch letters (south B, east A, west Y,
  north X). `button_prompts`: the controller in use (SDL's pad type for port 0, Xbox while the keyboard or mouse was
  used last), Xbox, PlayStation, Switch; a change decodes the fonts again (`gpu::MarkDirtyData`). Checked: PlayStation
  and Switch forced, and switched live from the menu; real PlayStation and Switch pads not tried. Triggers, shoulder
  buttons, Start and Back keep their Xbox pictures.
- **Mouse menus** (`src/game/mouse_menus.cpp`): a menu (vtable `0x43C8B8`) takes input in `sub_001EF55C`; its box
  (`+0x10`, centre and half size in its parent's box, -1 to 1, y up) through its parents (`+0x04`) gives the screen
  rect, and its shown rows are equal bands from the top (`sub_001EF781`). Pointing picks a row (move sound
  `sub_001E917C`, `SetSelected` `sub_001EF329`); left click is A on it, right click B, the wheel the d-pad. The cursor
  is free (SDK `input::SetPointer`) while a menu took input in the last 250 ms, or 1.5 s with the player's view
  standing (a screen change). Checked: main menu, OPTIONS, back, wheel, pause menu hover and RESUME, mouse look after.
  Screens that are not menus (CHAPTER, the briefing) keep the mouse as before.
- **MSAA** (SDK `d3d12.cpp`): the back-buffer-sized colour and depth targets get a multisampled twin; draws go there
  when all bound targets have one; anything else that reads a target resolves it first (colour `ResolveSubresource`,
  depth by a pass writing sample 0, stencil not carried); uploads, copies and depth copies mark the twin behind, and
  the next draw fills it from the plain target. Checked at 4x: front end, level, pause, co-op split screen, and the
  D3D12 debug layer through the level (no messages). The gun's seams show a few more white dots than without
  (cracks in the model, there at 1x too).
- **Xbox Live status line:** `sub_000F023A` shows one of four `XBL_LiveAware_*` texts ("Sign-in failed." here); the
  four texts in packfile.pck become spaces as it is read (`content.cpp`).

### CRI movie crash fixed (2026-10-20)

- The random crash when a movie ends or is skipped (guest null read in `0x0030xxxx`): `sub_00303DF0` hands the
  movie player's handle check `sub_0030E2B0` the handle `sub_0030DE00` finds for the current stream, which is 0
  while that stream's slot is not set up yet (CRI error `FF000401`). The host's threads run side by side, so
  the decoder sometimes gets there first, and the check read the null handle's vtable. Both of the latest dumps
  crashed there (`sub_0030DF70`).
- Hook on `sub_0030E2B0` (`src/game/hooks.cpp`): a null handle gets the answer of a handle of neither kind
  (0 and 0), as the game already gives; logged once ("cri: movie stream handle not ready yet").
- Checked: 6 boots with the intro movies skipped hard, no crash (2 crashes in about 8 runs before). The
  hook's path did not come up in those runs.

### Shader pre-compile at launch (2026-10-20)

- Every pipeline key a draw needs goes on `shadercache\pipelines.bin` (any driver). Compiled pipelines go in a
  D3D12 pipeline library named for the GPU and driver version (`pipelines-<vendor>-<device>-<version>.lib`). At
  device init every listed pipeline is loaded from the library or compiled on worker threads, behind a progress
  window (Skip with B / Esc / close, "Don't show this again" with X / Space) that shows only past 300 ms.
- A new driver finds no library of its name (or the driver rejects the old one): the list compiles again at
  launch and the old library is deleted. "Don't show this again" sets `precompile_shaders = false`; PC settings,
  Graphics, "Precompile shaders" turns it back on.
- Checked 2026-10-20 (dev build): level 1 run made 48 pipelines; next launch 48 of 48 ready in 14 ms; renamed
  library (fake old driver) logged "(changed)", recompiled in 24 ms (the driver's own cache still warm); a
  slowed test build showed the window, Space + Esc skipped at 11 of 48 and turned the setting off; the off run
  skipped the pass. Not checked: a real driver update, the menu checkbox on screen.
- The very first run still compiles as it goes: shaders come from the game's own data at run time, and the
  release ships none.

### Releases build the game code on the player's PC (2026-10-20)

- SDK moved to upstream `main` 99078b1 plus the first-start compile (b47e2e9, which upstream reverted by
  mistake a minute later; see `patches/README.md`). The game code is `Darkwatch.dll`, delay loaded by
  `Darkwatch.exe`; our hooks stay in the exe, which exports them to the DLL.
- `release.bat` zips the exe, the `compiler\` folder (xbe_recomp, clang, lld, the config and headers), the
  player README and the VC runtime: **no game code**. The first start reads `default.xbe` from the `.iso` (or the
  files) in `assets\`, builds the DLL behind a progress window (about 45 s on a 12-thread PC, 129 files), then
  offers Start.
- Checked 2026-10-20: dev build boots to the first level, PC SETTINGS from the pause menu opens the menu; release
  zip unpacked to a fresh folder with the `.iso` only: first start built the DLL and the game booted.
  Not checked since the SDK move: the meat-locker fog (upstream changed how render-target reads are offset).
- The exe links with `/pdbaltpath:%_PDB%` so it holds no local build path.

### PC settings menu (2026-10-19)

- **Open:** F1, or Back+Start on a pad, anywhere; **PC SETTINGS** in the pause
  menu (was XBOX LIVE OPTIONS) and in the main menu's OPTIONS page (was the
  greyed XBOX LIVE). The multiplayer menu's XBOX LIVE mode is left alone.
- **Pages:** Display (mode, resolution, render %, fps cap, V-Sync), Graphics
  (anisotropic), Controls (keyboard, mouse look, sensitivity, rebinding with
  Set/Add/Default keys), Game (fps counter). Live apply; a display mode change
  asks to keep it (15 s revert). `settings.toml` is written on close. Resume
  and **Exit to Desktop** (with a confirm) at the bottom. Mouse free and game
  input blocked while open, and until every key is let go after closing.
- **Code:** `darkwatch-hle/src/common/menu.cpp` (ImGui, it passes the hooks on
  to the F4 box), `darkwatch-hle/src/game/menu_hooks.cpp` (game menus). SDK:
  `input::ReloadBindings`, `input::KeyName`.
- **How the game menus are reached** (for the next menu work): menus are data
  in `packfile.pck` (`UserInterface/InGame/InGame`, FrontEnd); an entry's
  element name is its text id; a text element draws the text whose
  CRC-32 of the **upper-case** id is at its +0x48 (hash fn `sub_0022A8D1`,
  lookup `sub_001EA42B`); strings are pooled `{u16 refs, chars}`. Select
  handlers: pause `sub_000F64B3`, front-end OPTIONS `sub_000F56F3`; OPTIONS
  page build `sub_000EA2F0`; menu SetEnabled(index, on) `sub_001EF3ED`. The
  pool's "Xbox Live Options" is rewritten to "PC SETTINGS" in place. The
  QUIT GAME confirm is a message box (NO / YES): a third entry there is the
  likely spot for a native EXIT TO DESKTOP.
- **Seen, not fixed:** "SIGN-IN FAILED." stays at the top left in the level
  (an Xbox Live notice; element `live_aware`).

## Console references for QA (local only, not in git)

- `references/Youtube PS2 Recordings/`: PS2 walkthrough, one or more `.mp4`
  per level, named `NNN - Darkwatch_Walkthrough_Level_<n>_<name>...mp4`
  (levels 1-18).
- Xbox stills: `references/*_xbox.png` (README there).

**How to use them on an F4 report:** open the level's video, find the same
spot, save a frame next to the report and compare. Frame at a time:
```powershell
ffmpeg -ss 00:12:34 -i "references\Youtube PS2 Recordings\001 - Darkwatch_Walkthrough_Level_1_The_Wrong_Train_HD.mp4" -frames:v 1 ref.png
```
**PS2 is not the Xbox.** The Xbox version has richer lighting, bump maps,
shadows and effects; the PS2 has fewer. So the rule is:
- **Bug:** something looks *broken* in ours: missing or black geometry,
  stretched or smeared pixels, flicker, wrong colours on a whole surface,
  HUD or text wrong, effects that build up or leave ghosts, sound missing.
- **Not proof of a bug:** ours has *more* (an effect or light the PS2 lacks)
  or looks sharper. Check an Xbox still or ask the owner before calling it.
- Do not commit videos or full-length frame dumps; single reference frames
  go in `references/` with a README line as usual.

## Roles (owner, 2026-10-18)

- **Junior engineer** (the junior model): takes engineering tasks, code
  included, with QA in each one. Works on a branch `jr/task-NNN`, pushes it,
  never commits code to `master`. Regenerates the SDK patch when it changes
  `third_party/XboxXBERecomp`. After a fair try, if stuck, records what it
  tried and stops.
- **Senior / manager** (Claude): writes tasks, reviews each branch, builds and
  tests it with the owner, merges to `master` (final merge QA), and takes
  over what the junior cannot finish.

## Where each port is

| | HLE port `darkwatch-hle/` (main line) | xboxrecomp port `darkwatch/` (reference) |
|---|---|---|
| Furthest point | **Windows:** title → NEW PROFILE → menus → briefing → **first level (THE WRONG TRAIN), rendering with HUD on D3D12** (2026-10-18); speed, audio and play not yet checked. **macOS headless harness:** PRESS START, never driven further | first level playable, ~1.6 fps, macOS SDL window |
| Builds on this Windows PC | yes (native, build.bat, clang++ 19.1.5 / cmake 3.31.6 / ninja 1.12.1, ~3 min) | not tried |
| Builds on the Mac | yes (harness + cross exe) | yes |

## Now: the first level looks close to the console

`references/current/level1-enemy_hle_2026-10-18_lit.png`: lit gun, hand and
enemy, close to `references/level1-*_xbox.png`. The **fog streaks are fixed**
(2026-10-18, below; was `references/current/level1-streaks_hle_2026-10-18.png`).

task-008 (partial) 2026-10-18: reached the meat-locker fog corridor (turret
room comes first; play past the locker needs an owner walk); console fog
streaks: **none** (Xbox hardware + PS2 captures); top problems: the HLE fog
streaks (fixed since, see below), a suspect first-person gun-pose glitch in the turret
room, no console references past the meat locker
(task-008 notes).

**Fog streaks: fixed 2026-10-18** (`darkwatch-hle/src/game/hooks.cpp`, hook on
`sub_001CDEC7`; owner checked at the meat locker: no lines). The game's
material-stage setup (`sub_001CDEC7`) loads `c112` itself: for a screen-space
stage (`desc+8 != 0`) it uses `1/size` on a power-of-two texture, but a fixed
**2.0** (constant `0x47A114`) on any other. The haze uses the previous frame,
a 640x480 linear texture, so it got 2.0: coordinates twice the screen pixel,
past the edges, CLAMP smear. A linear texture is addressed in texels, so the
hook rewrites `c112.xy` to 1 after that function for non-power-of-two textures
(z and w, the texture's offsets at `+0x1C`/`+0x1E`, kept). Why the console's
hardware gets away with 2.0 is still not known; the hook is a game-side fix,
not an SDK one. Found by logging the `SetVertexShaderConstant1` caller for
register 112 (`0x001CDFF0`).

Left: a faint **crosshair ghost** over the fog (offset copy below-right,
`captures` F3 2026-10-18 14:09). The haze re-adds the previous frame, HUD
included, shifted, so this is expected from the effect; no console frame to
check yet. Leave it unless a reference shows otherwise.

What was known before the fix (2026-10-18, by bisection with `drawskip.txt`, below):

- The streaks come from **one effect**: five draws per frame with vertex
  program `B01034F1` and pixel shader `B0103620`, which sample the **other
  swap buffer** (the previous presented frame, `03D98000`/`03EC4000` in
  turn) as a linear 640x480 texture. Skipping only those five draws removes
  the streaks (`..._haze-skipped.png`). The textures, `texsize` and the
  sampled frame itself are fine.
- They **build up over frames**: the pass adds part of last frame back
  (blend `SRCALPHA, ONE`), with alpha = `2 × last frame's alpha × v0.a`, and
  last frame's alpha on the floor is high (143-255). So each frame re-adds a
  shifted copy of the previous one.
- Measured with F3 (2026-10-18; the capture now prints the stage 0 address
  modes and `c58`/`c59`/`c112`-`c114` per draw): the program maps the
  vertex to screen pixels with the viewport constants `c58 = (320, -240)`,
  `c59 = (320, 240)`, subtracts `v3 × c113.xy` (`c113 = (640, 480, 0, 1)`),
  then **multiplies by `c112 = (2, 2)`**. The texture is the 640x480 linear
  back buffer and the game sets **CLAMP** (`addr0 3 3`). So for most of the
  screen the coordinates lie past the texture and take its edge row and
  column: those are the stretched lines.
- Ruled out: multisampling (the game asks for `0x11`, none; logged at boot
  as `d3d: multisample type 11`), and the SDK turning WRAP into CLAMP (the
  game itself sets CLAMP).
- **Open question:** what makes the console's coordinates come out half as
  large. Candidates: the console's viewport constants `c-38`/`c-37`
  (`c58`/`c59` here) for this draw, how the NV2A scales a linear texture's
  coordinates from a vertex program, or a larger texture header on the
  console. Next step: find where the game sets `c112` (register 16 in
  console numbering) and what it derives the 2 from, then compare with the
  longplay at the same spot.

Tools added for this (SDK patch):
- `drawskip.txt` beside the exe: lines `first last` of frame draw numbers
  (as `frame.txt` counts them) to leave out, read again every 30 frames.
  `darkwatch-hle/tools/f3_skip.ps1 -Skip "110 110" -Name x` writes it, presses
  F3 in the game window, and saves `captures\x.png`.
- F3 captures now also save each **render target a draw samples**
  (`NNNN_sampled_<addr>.bmp`), as the draw sees it.

## Keyboard and mouse (2026-10-18)

Player 1 can use the keyboard and mouse, alongside or instead of a pad (SDK
patch, `runtime/core/input.cpp`). Defaults: WASD move, mouse look, left
click fire (RT), right click LT, Space/Enter A, E/Backspace B, R X, Q Y,
C/F black/white, LShift L3, middle click/V R3, arrows d-pad, Escape Start,
Tab Back. **F2** frees or takes the mouse. Alt-Tab frees it too.
`settings.toml` beside the exe is written on the first run with every
binding under `[keys]`, plus `keyboard`, `mouse_look` and
`mouse_sensitivity` (percent). The full list for players is in
`darkwatch-hle/tools/README-release-windows.txt`. Input is read only while
the game window is in front; the cursor is hidden and held at the centre
while the mouse looks.

**QA automation:** `darkwatch-hle\tools\press.ps1 "<steps>"` brings the
game to the front and plays keys, mouse buttons and mouse moves through
`SendInput`. The game reads them like real input. Steps, separated by `;`:
`Enter` (tap), `W:2000` (hold 2 s), `W+A:1500`, `wait:500`,
`look:400,0` (turn right; `look:dx,dy,ms`), `Mouse1:300` (fire),
`f3` (capture). Example, title to main menu:
`press.ps1 "Escape; wait:3000; Enter; wait:2000"`. To check that a shot
fired, look at the cylinder icons next to the ammo count: the number (36)
is the reserve and only drops after a reload (`R`).

Later (owner's notes, 2026-10-18):
- an in-game options menu (the SDK already draws an ImGui overlay) for key
  rebinding, mouse settings, resolution and ultrawide;
- short **stutters** in play, most likely shaders and pipelines compiled
  the first time each one is used; a pipeline cache saved to disk would
  remove them after the first run.

## Testing on this PC: how to get back to a spot

- **No save states.** Each guest thread is a real Windows thread inside
  nested native calls; a running game cannot be written out and reloaded
  like an emulator snapshot. To get back to a place in the game:
  - **The game's own checkpoints.** Pick the saved profile, then
    SINGLE PLAYER, then CONTINUE: you restart at the last checkpoint.
- **Scripted input:** `press.ps1` (above) can replay the menu path,
  e.g. `press.ps1 "Escape; wait:4000; Enter; wait:3000; Enter; wait:2000; Enter"`.
  The waits depend on loading times; check with `f3` captures along the way.
- goto_level1.ps1: boot to THE WRONG TRAIN, about 45 s (task-006).
- **Stutters:** set `$env:XBE_PERF = '1'` before starting the game. Every
  frame over 25 ms logs `hitch: frame N took X ms (compile Y ms, S new
  shaders, P new psos)` to `logs\darkwatch.log`, plus `perf:` lines once a
  second (Release too, SDK patch 2026-10-18). See task-009.
- **Start the game with `darkwatch-hle\tools\run_capture.bat`**, never
  `Darkwatch.exe` directly, so F3 captures work (`XBE_CAPTURE_DIR=captures`).
  Captures land in `darkwatch-hle\out\build\release\captures\<HHMMSS>\`
  (`frame.txt`, `final.bmp`, sampled targets, textures, shaders).
- **The random CRI movie crash** (when a movie ends or is skipped): just run
  `run_capture.bat` again.
- **Finding which draw makes a fault:** stand still, then run
  `powershell -File darkwatch-hle\tools\f3_skip.ps1 -Skip "110 128" -Name test`.
  It leaves those frame draw numbers out (numbers as in `frame.txt`), presses
  F3, and saves `captures\test.png`. Halve the range until one draw is left.
  Run it with an empty `-Skip ""` at the end to switch skipping off.

## How to report a problem (F4, 2026-10-19)

Press **F4** anywhere: a "Report a problem" box opens in the middle of the
screen with the text cursor already in it. Type what was seen (the game gets
no input while it is open — the polled pad is zeroed, the mouse is freed for
clicking), then **Save** (or Ctrl+Enter) or **Cancel** (or Esc). Save writes
`reports\<YYYY-MM-DD_HHMMSS>\` beside the exe: `note.txt` (the note, the date
and time, seconds since launch), the F3 capture of the frame from **when F4
was pressed** (`final.bmp`, `frame.txt` and the rest), `log.txt` (the last
300 lines of the log), and a copy of `settings.toml`; a "Report saved" line
shows for 3 s. Cancel throws the capture away and leaves nothing. Works
started with `run_capture.bat` and by double-clicking the exe (when
`XBE_CAPTURE_DIR` is unset the game now defaults it to `captures`).

## Fixed: gun, hand and enemies drew black (2026-10-18)

`references/current/level1-gun_hle_2026-10-18_black.png` shows the before:
dark with only the specular shine. The hand's vertex program computes
`oD0 = c[191] * R11`, with R11 the light sum from `c[6]` to `c[12]`. The SDK
never had those constants. RenderWare pushes them **straight into the push
buffer** with the XDK's inline macros (`0x00261020`: `CONSTANT_LOAD`, then
`SET_TRANSFORM_CONSTANT` values), so no `SetVertexShaderConstant` hook sees
them. The SDK's push-buffer walk before each draw now applies those two
methods (SDK patch, `Walk` in `d3d8.cpp`).

task-005 (2026-10-18): 60 fps typical (Debug, capped at 60; the console runs
30), sound fine apart from occasional slight crackles, ~5 min without a
fault (task-005 notes). Other open items:

- Faults the game survives: guest `0x00128C67` (19 times in a Release run;
  the symbolizer puts the host side in `kernel::kimpl_NtDuplicateObject`)
  and `0x0026D211`; neither appeared in the Debug run.
- No reload sound (task-005).
- ~~The CRI movie crash~~ fixed 2026-10-20 (see above): guest thread 28, `read 100000000`, `0x0030xxxx`, when
  a movie ends or is skipped (the intro, and once at DIFFICULTY). Random:
  a relaunch gets past it. The xboxrecomp port had a CRI worker race too
  (BRINGUP.md).

## Fixed: enemies and the first-person gun did not draw (2026-10-18)

RenderWare skins its characters and weapon with vertex programs. It loads
them with `D3DDevice_LoadVertexShaderProgram(function, 12)` (`0x00342EB0`,
not in `game.toml` before) and runs them with
`SelectVertexShader(declaration-only shader, 12)`. The SDK's
`LoadVertexShader`/`SelectVertexShader` only logged "not supported", and
`LoadVertexShaderProgram` was not hooked. Now (SDK patch, `d3d8.cpp`) the
three keep a 136-slot program memory. Select runs from the slot to the
instruction with the final bit, using the handle's declaration (one shader
object per handle and program, cached). `LoadVertexShaderProgram` is named
in `game.toml`. Result: the gun and enemies draw; lighting on other objects
improved too.

## Fixed: crash on CONTINUE at the briefing (2026-10-18)

task-004 (2026-10-17) walked title → NEW PROFILE → main menu → CHAPTER →
DIFFICULTY → briefing; A on "PRESS A TO CONTINUE" crashed (`read 100000004`,
guest thread 14, in D3D's `0x0034E540`). The Debug dump's stack (read with
`tools/mdump.py`, which now also reads the 32-bit memory list) went from
RenderWare `0x00132FA0` into `0x003485A0`, which is
**`D3DDevice_BeginPush`**: set up the vertex arrays, then reserve `count + 1`
push-buffer words. `0x003485C0` beside it is **`EndPush`**. Both were `= 0`
in `game.toml`, so the console versions ran and walked device state the SDK
never fills in (a null current vertex shader). Both are named now, the SDK's
HLE versions replace them, and CONTINUE drops into the level.

## Fixed: graphics bugs on the NEW PROFILE screen (2026-10-17)

Now: `references/current/new-profile_hle_2026-10-17_font-fixed.png`, which
matches `references/new-profile_xbox.png`. Earlier states:
`..._text-fixed.png`, `....webp`.

1. ~~**Wrong text** (`O Z Y Z ...`, `DONEE`).~~ **Fixed 2026-10-17**: the
   text is now right (`0-9`, `A-Z`, `SPACE`). It was not a string bug. Probes
   on RenderWare's draw wrapper (`0x00131950`) showed every glyph drawn from
   one of two small dynamic vertex buffers (`0x0070C39C`/`0x0070C3A8`), used
   in turn. RenderWare's pool (`0x0012AA50`) reuses a buffer when its header
   lock field (+8) holds a fence that `IsFencePending` says is done. The
   console's D3D writes that fence on every draw; the SDK never did, so both
   buffers always looked idle. They were refilled before the render thread
   drew them, and each one showed the last glyph written (`Y`, `Z`). Fix: SDK
   patch `StampStreams` in `runtime/xdk/d3d8.cpp` (see `patches/README.md`).
   Also, in `hooks.cpp`, D3D's inline lock wait `0x00348B60` now goes to the
   SDK's `D3D_BlockOnResource`; it used to call the SDK's no-op
   `D3D_BlockOnTime`.
2. ~~**Blocky dotted font, black boxes behind glyphs, garbled Ⓐ icon.**~~
   **Fixed 2026-10-17**, all one cause. The fonts and icons are **P8
   (palettized)** textures, and the SDK never knew their palette.
   `D3DDevice_SetPalette` was `= 0` in `config/game.toml` (no signature
   matches it), so the game's own copy ran as lifted code and the SDK decoded
   indices as grey with full alpha. Found by its push-buffer method
   `SET_TEXTURE_PALETTE` (`0x1B20`): `0x00344E20` stores
   `CDevice_Palettes[stage]` (`+0xF98`) and pushes it. Now named in
   `game.toml`, so the SDK's HLE `SetPalette` replaces it. (Careful:
   `0x1B24` is `BORDER_COLOR`; `0x00346050` is correctly
   `SetTextureState_BorderColor`.) The Mac's "dot pattern on the title logo"
   was probably the same bug.

Lesson for every `= 0` symbol in `game.toml`: the game's own copy runs, and
the SDK's state misses whatever it sets. The D3D ones still at 0 are listed
under Known gaps.

Reference: `references/new-profile_xbox.png`. F3 captures:
`references/current/capture-new-profile/` (before the fixes).

## Fixed: crash on the LOADING screen after START

Before the fix below, START showed LOADING, and about 25 s into the run the
game faulted on guest thread 14:

```
exception C0000005 ... on guest thread 14
  read 1D0060000, guest D0060000                <- just past the top of a guest stack (esp D005D6D8)
  eax 00000003 ecx 0086004C edx 0081007F ebx 00000523 esi FFFFD8F0 edi 00000000
  [esp+014] 001D4F16 (after call 0x001D4E69)  [esp+01C] 00409F36  [esp+044] 001D3D94
  [esp+060] 002140DC (after call 0x00213F2D, the 64-dword block allocator)  ...  [esp+08C] 003E8652 (XACT)
```

Not a DSOUND method this time: XACT (0x003E8xxx) is calling back into game code,
which walks off the end of a buffer. `esi = -10000`, a negative count or offset,
suggests data the HLE handed XACT (a stream packet, a wave bank header, a
status) has a value the console would never give. Next step: a Debug build
(info logs on), `XBE_KTRACE=1` around START, and the stack from the `.dmp`
in `darkwatch-hle\out\build\release\CrashDumps`.

Debug evidence collected 2026-10-17: task-002 notes (crash in 2 of 3
runs, same address: yes).

**What the evidence says:** the crash does not follow START, it follows
**`movies2.pck`**. In both crashing runs the last lines are the same: the three
CRI movie workers exit (2C/30/34), three new ones start (38/3C/40, at
0x002EE360/0x002EE3E0/0x002EDFF0), `open D:\movies2.pck`, then the fault
within about 0.2 s. Run A-1 sat in the `movies1.pck` attract loop for two hours
and crashed the first time it moved on to `movies2`. In the runs that did not
crash (A-3, B), START landed at the end of the first movie: the front end
loaded `audioWpn.pck`, but the game went back to the `movies1` loop and never
opened `movies2`. So the "LOADING then crash" seen by hand is the switch to the
second movie.

**Cause and fix (2026-10-17).** The `.dmp`, read with a small minidump reader
and `llvm-symbolizer` against the Debug PDB, puts the fault in XACT's
`sub_003E8688`, not in game code; the game addresses above were stale stack
words. XACT calls `GetVoiceProperties` into a `DSVOICEPROPS` stack local
(`sub_003E5BB9`), then loops over `MixBinVolumePairs` up to `dwMixBinCount`.
Both versions, buffer `B0055` (`0x003A9795 → 0x003A9165`) and stream `B0081`
(thunk `0x003A9829`), had been hooked as no-op *setters*. They never wrote the
struct, so the count was stack garbage (`ebx = 0x523`) and the loop read off
the end of the guest stack. They are getters now
(`VOICE_PROPS_GETTER` in `hooks.cpp`): 0 mix bins, pitch 0, full volumes.
Follow-up: report the voice's real bins and volumes, which needs an SDK
change in `runtime/xdk/dsound.cpp`.

Also from the Debug build: SDL asserts at boot that `SDL_PumpEvents` runs off
the main thread (`SDL_events.c:1500`). Release compiles the assert out. Debug
runs need `SDL_ASSERT=always_ignore`. This is a real runtime bug to fix later
(input/event pumping belongs on the main thread).

## Fixed: the DSOUND crash right after START

Two runs of the v0.1.0-debug exe (`Darkwatch-windows-x64-debug/logs/darkwatch_001.log`,
`_002.log`, plus two `.dmp` files) fail the same way, so it is deterministic:

```
open Z:\audioWpn.pck                          <- front end copies weapon audio to the Z: cache
exception C0000005 ... on guest thread 14
  read 100000004, guest 00000004              <- null + 4
  [esp+000] 003A9C46  [esp+00C] 003AA3C0  [esp+01C] 003AACC5  [esp+02C] 003AAE7D  [esp+040] 003E5649 (XACT)
```

Reading the generated code:

- `sub_003AAE5C` is an `IDirectSound` method thunk (`this`, 3 args, `ret 0x10`)
  that the XDK 5849 signatures did not name. The log lists it as
  `B003B at 003AAE5C` under *"dsound: exports running their original code"*.
- It runs the **lifted** DSOUND body: `sub_003AAC84` → `sub_003AA3C0` →
  `sub_003A9C25` (constructs an object with vtable `0x44D6AC`) →
  `sub_003A8F1A`, which reads through a null hardware-voice pointer.
- Same class of bug as the eight `STREAM_VOICE_SETTER` hooks already in
  `darkwatch-hle/src/game/hooks.cpp`: XACT reaches a DSOUND export the HLE did
  not replace, and the original code needs APU hardware the HLE does not have.
Fix, in `darkwatch-hle/src/game/hooks.cpp` (three crashes, one after another):

1. `B003B` is `IDirectSound_CreateSoundStream`. XDK export ids are
   alphabetical: it sits between `CreateSoundBuffer` (`B003A`) and
   `DownloadEffectsImage` (`B003C`). It now forwards to the HLE
   `DirectSoundCreateStream`.
2. `B0098` (`sub_003AA08A`) is `IDirectSoundStream_SetOutputBuffer`. It is a
   no-op now, as the SDK's buffer version already is.
3. **Five of the eight old `STREAM_VOICE_SETTER` hooks never ran.** A body
   reached only by a `jmp` from its thunk gets folded into the thunk's
   function, so `sub_003A924D`, `92F1`, `948B`, `94DD` and `952F` do not
   exist and their hooks replaced nothing. The thunks (`003A97EE`, `97F3`,
   `9829`, `982E`, `9833`) are hooked now. The crash was in `9833`'s copy.
   Watch for this with every new hook: check that `X86_FUNC(__imp__sub_<va>)`
   exists in `generated/`.

Also no-op'd, ahead of a crash: buffer `SetPitch`/`SetLFO`/`SetEG`
(`B0076`/`B006D`/`B0067`). Left running original code, on purpose:
`B002F` `DirectSoundUseFullHRTF` (the SDK patch keeps it), `B00A3`-`A5`
(XAudio pitch/format math, no device), `B0080` (calls the HLE `FlushEx`).
The startup warning still lists them all, because the SDK's audit only knows
its own list.

Reproduced with the native build on 2026-10-17: same crash (identical guest
thread 14 stack in the task-001 run log; the release build logs only the
exception — info lines such as `open Z:\audioWpn.pck` are compiled out under
`NDEBUG`, `runtime/core/log.h`).

## Known gaps (HLE port)

- Stream and buffer pitch not applied (the `SetPitch` hooks, thunk
  `0x003A97EE` and `0x003A95E5`, are no-ops).
- D3D symbols that are `= 0` in `game.toml` (`SetPalette`, `BeginPush`,
  `EndPush` were named by hand, 2026-10-17/18): `BlockOnFence`, `InsertFence`, `EnableOverlay`, `FlushVertexCache`,
  `GetVertexShaderFunction`, `PersistDisplay`, `SetTile`, `SwitchTexture`,
  `D3DResource_IsBusy`, some DS3D. They do **not** trap. If the game has its
  own copy, that copy runs as lifted code and the SDK never sees what it sets
  (that was `SetPalette`). If a screen looks wrong, check this list first:
  find the function by the NV2A method it pushes and name it in `game.toml`.
- On a PC with **Mandatory ASLR** on (Exploit protection, system-wide),
  Git for Windows' shell scripts and Git Bash cannot fork (`0xC0000142`,
  git-for-windows #6349). CMake's FetchContent `git submodule` step fails.
  Workaround, no security change: `darkwatch-hle/tools/deps_windows.bat`
  clones the four deps with plain git and points the release preset at them
  (`FETCHCONTENT_SOURCE_DIR_*`). Configure + `xbe_recomp` build checked.
- `GetVoiceProperties` answers 0 mix bins, so XACT applies no per-bin
  attenuation (3D/positional volume may be off).
- Reading a crash on this PC: `lldb` from VS does not start (exit 53).
  Instead, `python darkwatch-hle/tools/mdump.py <dmp>` prints the module base and RIP, then run
  `llvm-symbolizer --obj=Darkwatch.exe 0x140000000+RVA` on the Debug build.

## Next, in order

1. ~~**task-001** – native Windows build of the
   HLE port on this PC, and reproduce the crash above with it. Every later fix
   needs this loop; today a fix can only be tested by cross-building on the Mac.~~
   Done 2026-10-17. *Junior-safe.*
2. ~~Fix the post-START DSOUND crash.~~ Done 2026-10-17, see above.
2b. ~~**task-002** – Debug build, three logged runs and
    a kernel trace of the LOADING-screen crash.~~ Done 2026-10-17. *Junior-safe.*
2c. ~~Fix the LOADING-screen crash from that evidence.~~ Done 2026-10-17: reaches NEW PROFILE.
2d. ~~NEW PROFILE graphics bugs.~~ Done 2026-10-17 (vertex-buffer fences,
    `SetPalette` named); references from
    ~~task-003~~ (done 2026-10-17).
3. ~~**Now: task-004**: walk the menus to the
   first level and record each screen and the first blocker. *Junior-safe*
   (collects evidence, changes no code).~~ Done 2026-10-17 (blocker: crash on
   briefing CONTINUE).
3b. ~~Fix the first blocker task-004 finds.~~ Done 2026-10-18 (BeginPush/EndPush
    named): reaches the first level.
3c. ~~**Now: task-005**: play the first level (frame
    rate with `XBE_PERF`, audio, walk/look/shoot/door, minutes without a fault)
    and name the two survived faults with a Debug run. *Junior-safe*: collects
    evidence, changes no code.~~ Done 2026-10-18 (60 fps, sound fine; top
    problem: invisible/clipping enemies).
3d. ~~Fix the worst problem task-005 finds.~~ Enemies and gun draw now
    (2026-10-18); black models fixed (pushed constants, 2026-10-18).
    ~~Stretched vertical lines in the fog~~ Fixed 2026-10-18: the game's
    2.0 texture scale for the 640x480 previous frame (see "Now" above).
3e. ~~Keyboard/mouse native input~~ Done 2026-10-18 (see "Keyboard and mouse"). Was: (owner request,
    2026-10-18): for end users and to let the agent drive menus/QA without
    the pad.
3f. ~~**The CRI movie crash**~~ fixed 2026-10-20 (`sub_0030E2B0` null handle). (guest thread in CRI code `0x0030xxxx`, null
    read, when a movie ends or is skipped). It now happens often enough
    (twice on 2026-10-18 on the way into the level) to block QA runs.
    *Senior.*
    task-007 done 2026-10-18: crash rate A 0%, B 0%, C 0% (task-007 notes).
3g. Junior-safe, in this order:
    ~~**task-006** – script from boot to the first level (`goto_level1.ps1`)~~ Done 2026-10-18 (about 45 s, 5/5 runs);
    ~~**task-007** – evidence for the CRI movie crash (rates, Debug stacks)~~ Done 2026-10-18 (0 crashes in 100 runs; four old dumps symbolized);
    ~~**task-008** – level 1 QA pass with longplay references, incl. the fog-streak question~~ **Closed** 2026-10-18: its key result is in (console fog has no lines; the streaks are fixed). The rest (play past the meat locker, more console references) moves into the play-through QA with the F4 report box (Road step 5); console frames come from the per-level PS2 videos (see "Console references for QA"). Notes: task-008 notes;
    ~~**task-009** – stutter trace in the first level with the new `XBE_PERF` hitch log (compile vs other, first vs second pass). First sample (2026-10-18): the first level frame took 1.5 s, 1.45 s of it 25 shader and 36 pipeline compiles; first-door hitches 140-185 ms, all compiles.~~
    task-009 done 2026-10-18: 9-19 hitches per first pass, ~60-68% compile; second pass 0 hitches (task-009 notes).
    Engineer tasks (branch `jr/task-NNN`, senior merges; see "Roles"):
    **task-010** – shader disk cache (and a pipeline library if pipelines cost much), to stop the compile hitches;
    task-010 on jr/task-010 2026-10-19: warm run load frame 1548-1765 ms -> 88 ms; door hitches 4-6 -> 0 (task-010 notes). **Merged** 2026-10-19 (senior check: warm load frame 247 ms, 0 shader compiles; the first-run stretch step, precompile/background compile, was not done).
    **task-011** – fog/door ghosting: what alpha the previous frame holds vs the console, and the fix.
    task-011 on jr/task-011 2026-10-19: back buffer fmt 6 (A8R8G8B8); ghosting is Xbox-faithful (PS2 has none), now the opt-in "PS2 alpha blending" menu setting; default unchanged (task-011 notes). **Merged** 2026-10-20 (senior check: Graphics page; restored the lost Anisotropic filtering label; anisotropic default now 16x, owner).
    **task-012** – bug-report hotkey with an in-game note box.
    task-012 on jr/task-012 2026-10-19: F4 report box done (task-012 notes). **Merged** 2026-10-19 (senior check: F4 in the level, typed note, Ctrl+Enter; folder complete, about 15 MB a report because the capture saves every texture).
    Order (owner, 2026-10-18): 009 (short), 010, 012; then the senior's menu frame; 011 when convenient.
    Senior: PC settings menu done 2026-10-19 (see "PC settings menu" above).
    **Next for the junior (2026-10-19):**
    **task-011** (fog/door ghosting), then
    **task-013** (find the game's FOV, `fov`/`viewmodel_fov` settings; add them to the menu's Game page in `src/common/menu.cpp`).
4. Speed, audio, presentation polish toward the BLACK bar
   (https://github.com/TGP482/BLACK-XBERecomp).
