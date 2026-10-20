# The xboxrecomp port (reference)

> This is the older, reference port in `darkwatch/`. The main line is the HLE port in `darkwatch-hle/`; see the [README](../README.md).

Static recompilation of **Darkwatch** (Xbox, 2005; High Voltage Software / Capcom) into native C. Builds + runs on **Windows** (MSVC, D3D11) and **macOS** (Clang, OpenGL 3.3). No runtime emulation: whole XBE lifted to ≈3.47M lines C by [xboxrecomp](https://github.com/sp00nznet/xboxrecomp).

```
35,128 functions lifted          (0 failures)
3,468,538 lines of C             (143 files, ~133 MB)
0 unresolved ICALL targets       (see "Status")
24 XBE sections, 151 kernel imports
```

## Layout

```
.
├── Darkwatch (USA).xiso.iso     your disc image (not part of the repo)
├── game_files/                  extracted game data (default.xbe + *.pck)
├── darkwatch/
│   ├── CMakeLists.txt           builds the native executable
│   ├── config/darkwatch.ini     options (widescreen, scale, vsync, ...)
│   ├── scripts/                 data extraction, regeneration, builds
│   └── src/
│       ├── main_win32.c         Windows bootstrap (VEH crash handler)
│       ├── main_posix.c         macOS bootstrap (signal crash handler)
│       ├── config.[ch]          options reader
│       ├── display.[ch]         options → D3D8 presentation layer
│       ├── recomp_manual.c      function overrides + ICALL diagnostics
│       └── recomp/gen/          the recompiled game (generated)
└── tools/xboxrecomp/            the toolkit (patched; see patches/)
```

## Quick start (macOS)

```bash
# 1. extract the disc (once)
scripts/prepare_game_data.sh "Darkwatch (USA).xiso.iso"

# 2. build (installs SDL2/libepoxy via Homebrew if needed)
darkwatch/scripts/build_macos.sh

# 3. run from the project root so the data is found
./darkwatch/build/darkwatch_recomp
```

## Quick start (Windows)

```bat
git clone https://github.com/sp00nznet/xboxrecomp tools\xboxrecomp
darkwatch\scripts\build_windows.bat
darkwatch\build\Release\darkwatch_recomp.exe
```

Only need MSVC 2022 + CMake 3.20. D3D11 backend: no external deps.

## Regenerating the recompiled code

All under `darkwatch/src/recomp/gen/` = machine output:

```bash
darkwatch/scripts/regenerate_recomp.sh
```

Runs toolkit's five stages on `game_files/default.xbe`: `xbe_parser → disasm → func_id → abi_analysis → recomp`. ~10 min. `recomp_types.h` written by last stage; no hand edits.

## Options

`darkwatch.ini` (copy of `darkwatch/config/darkwatch.ini`) read from working dir or next to executable; env vars override.

| Option | Default | Notes |
|---|---|---|
| `resolution_scale` | `3` | window = guest backbuffer × scale; 1 = 640×480 |
| `window_width` / `window_height` | `0` | explicit size; 0 = from scale |
| `widescreen` | `1` | widen horizontal FOV to window aspect, no stretch |
| `fov_scale` | `1.0` | extra FOV multiplier |
| `fullscreen` | `0` | borderless fullscreen desktop |
| `vsync` | `1` | swap interval |
| `fps_limit` | `0` | frame cap; 0 = uncapped |
| `audio` | `1` | master switch |
| `keyboard` | `1` | keyboard → pad mapping |
| `deadzone` | `10` | left-stick deadzone (percent) |
| `trap_null` | `0` | fault on guest null deref (diagnostic) |
| `watchdog_seconds` | `0` | hang watchdog |

Env equivalents: `DARKWATCH_RESOLUTION_SCALE`, `DARKWATCH_WIDESCREEN`, `DARKWATCH_FULLSCREEN`, `DARKWATCH_VSYNC`, `DARKWATCH_FOV_SCALE`, `DARKWATCH_GAME_DIR`, `DARKWATCH_CONFIG`, …

Toolkit diagnostic switches (`RECOMP_KERNEL_WATCH`, `RECOMP_WATCH`, `RECOMP_TRAP_NULL`, `RECOMP_WATCHDOG_SECS`, `XBOX_LOG_LEVEL`, …) still work alongside; use these during bring-up. `RECOMP_FILE_TRACE` prints every guest file open with raw OBJECT_ATTRIBUTES — found partition-device open at boot.

## Bring-up loop

Recompiled dispatch can't see vtable call targets. Runtime records every observed indirect target (resolved or not), dumps on exit, crash, Ctrl-C:

```
DARKWATCH_ICALL_FEEDBACK=ON (default) → icall_targets.dump
        │
        ├─ python -m tools.recomp.icall_feedback merge icall_targets.dump
        ├─ python -m tools.disasm <xbe> --seed-functions tools/recomp/output/icall_targets.json
        ├─ python -m tools.recomp <xbe> --all --split 250 --gen-dir src/recomp/gen
        └─ rebuild → run again
```

`scripts/bringup_iterate.sh [--run]` does full turn in one go. Database cumulative: each pass turns unresolved stubs into real functions, title gets further.

## How the render path works

Guest creates D3D8 device with 640×480 (or similar) backbuffer; presentation layer enlarges **window** instead of re-rendering guest scene bigger. Gives crisp integer scale on any display, guest logic untouched. `widescreen` hooks here: projection matrix horizontal axis scaled by `game_aspect / window_aspect` **on the presentation copy only**, so `GetTransform` still returns exactly what game set.

## Status (honest)

**Current state: [STATUS.md](../STATUS.md)** (Windows-first; the HLE port in `darkwatch-hle/` is the main line). The summary below is the older xboxrecomp-port view. Evidence in [BRINGUP.md](../BRINGUP.md).

- **Pipeline**: clean. 36,107 functions lifted, 0 failures, ~3.59 M lines C. ICALL feedback: 0 unresolved code targets on runs below.
- **Both hosts build**: macOS/Clang (OpenGL backend) + Windows/MSVC (D3D11).
- **Boot**: CRT/XAPI startup, partitions, archives, heap, D3D8 device, display mode, XML type loader, DirectSound + XACT (emulated APU, ARM64 MMIO trap), CRI Sofdec movie player.
- **First picture**: intro FMV plays + presents — 1,577 flips in 120 s run, correct image, via software NV2A rasteriser at ~5 fps.
- **Function detector**: now places branches out of alias bodies, jump-table arms in gaps, callback tables inside code sections, hand-written asm after `int3` padding. See BRINGUP.md.
- **Known defect**: timing-dependent crash in movie player when guest code runs fast (`XBOX_LOG_LEVEL` 1–2, no instrumentation): CRI stream object reaches `sub_0030DF70` with vtable already cleared. Slower runs (`XBOX_LOG_LEVEL=3`, frame dumping) don't hit it. See BRINGUP.md.
- **Layer 2**: NV2A methods handled except one (0x147C); fixed-function T&L, stencil, culling, scissor, blits added to software executor. See BRINGUP.md.
- **Front end**: title screen, LOADING, and the NEW PROFILE menu render correctly; **gamepad input works** (USB/OHCI, SDL2 pads, scripted presses).
- **Not done**: rare CRI movie-player crash; macOS audio output (APU runs, nothing sent to CoreAudio yet); GPU-accelerated presentation.

Start bring-up runs (any platform) with:

```bash
darkwatch/scripts/run_bringup.sh          # switches preset, Ctrl-C dumps state
```

## Toolkit patches

`tools/xboxrecomp` = upstream clone, local fixes carried as `patches/*.patch` (applied against upstream `main`):

1. **macOS build** — POSIX-compatible `PCONTEXT`/`EXCEPTION_POINTERS` shims so `xbox_kernel` compiles with Clang.
2. **`lahf` and `_cf`** — function whose only carry consumer is `lahf` (Darkwatch's `sub_00059E99`) now declares `_cf`; upstream lifted it to non-compiling code. Regression test included (`tools/recomp/test_function_needs_cf_lahf.py`).
3. **Partition devices on POSIX** — `\Device\Harddisk0\PartitionN` backed by sparse `PartitionN.img` under save dir, retail partition table in `Partition0.img`, mirrors Win32 path layer. Title's first storage call = `partition0` open; without this, exits to dashboard before drawing.
4. **A partition image opened as a directory** — `FILE_DIRECTORY_FILE` on device image now opens containing dir, like Win32 backend; title's volume probe read failure as "no volume" and quit.
5. **Display options** — `xbox_D3D8SetDisplayOptions()` for window size, resolution scale, widescreen/FOV, fullscreen, vsync, frame cap; wired through OpenGL + D3D11 backends.
6. **Diagnostics** — `RECOMP_FILE_TRACE` prints guest file opens with raw `OBJECT_ATTRIBUTES`; ICALL-feedback dump available to hosts.
7. **GPU fence execution** — pushbuffer executor resolves DMA context handles via PFIFO RAMHT/PRAMIN tables driver builds in aperture; executes `BACK_END_WRITE_SEMAPHORE_RELEASE` (0x1D70), `SET_CONTEXT_DMA_SEMAPHORE` (0x1A4), `subch5:0x0310` (PGRAPH 0xFD400B10), `WAIT_FOR_IDLE` and D3D8's `NO_OPERATION` software methods; `xbox_Nv2aFenceSignal()` + `pb_write_seq - 2` fence mirror arm stall event at `dev+0x1DCC`. Lets D3D8 device creation complete in Darkwatch. `RECOMP_GPU_IDLE_FENCE` remains as lever.
8. **Vblank vector override** — `RECOMP_NV2A_INTR_VECTOR` picks vector GPU interrupt delivered on (titles differ in which they connect).

After pulling upstream, re-apply with `git -C tools/xboxrecomp apply ../../patches/*.patch`.

## Legal

No game code or assets included or redistributed. Must own Darkwatch for original Xbox. Toolkit MIT (its `src/apu` + `src/nv2a` parts LGPL-2.1 from xemu — see toolkit's `NOTICE`).