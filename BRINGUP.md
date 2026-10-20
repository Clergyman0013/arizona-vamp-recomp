# Bring-up notes

Where the project stands, what was fixed to get there, and the next concrete step.
Everything below is evidence from the run logs, not inference.

## Current status (2026-10-16)

The plan and the order of work are in [PLAN.md](PLAN.md).

There are two ports of the same `default.xbe` in this repository.

| | xboxrecomp port, `darkwatch/` | HLE port, `darkwatch-hle/` |
|---|---|---|
| Toolkit | `tools/xboxrecomp` ([sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp) **v0.14.0** eb0aa1d + `patches/xboxrecomp-darkwatch.patch`) | `third_party/XboxXBERecomp` ([TGP482/XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp), MIT, 40e9c35 + `patches/xboxxberecomp-darkwatch.patch`) |
| Graphics, audio | NV2A and APU emulated: software rasteriser, MMIO-trapped APU | Direct3D 8 on Direct3D 12, DirectSound on XAudio2 (HLE) |
| Furthest point | first level playable: walk, look, fire, at **~1.6 fps**; on v0.14.0 it runs in an SDL window on macOS, with audio | title screen, PRESS START, in the headless harness on macOS |
| Role | reference to compare against, fallback | **main line**; Windows is the required target, macOS nice to have |

The switch was made because the 1.6 fps is the software rasteriser itself
(see "Where the xboxrecomp port stops"), and an API-level D3D renderer is the
path to full speed on Windows.

### Git

- `master` holds everything (default branch on GitHub). The older branches
  (`level-start-apc`, `level-play`, `hle-spike`, `xboxrecomp-v0.14`) are its
  ancestors, kept for reference.
- Remote: `origin` = github.com/Clergyman0013/arizona-vamp-recomp (the project was
  renamed Arizona Vamp Recomp on 2026-10-20; the history was restarted for the public release).
- Not in git, by design: `game_files/` (the game), the generated code
  (`darkwatch/src/recomp/gen/`, `darkwatch-hle/generated/`), build output, and
  the two toolkit clones, which are rebuilt from upstream plus the patches
  (`patches/README.md`).
- `patches/wip-xboxrecomp-preemption.diff` holds uncommitted, unreviewed
  preemption work (CPU-token quantum, `RECOMP_PREEMPT_POINT`) that sits in the
  `tools/xboxrecomp` working tree on top of the patch. It was compiled into
  every xboxrecomp-port test run above; it is parked, not adopted.

### HLE port: how to run it on macOS

```bash
git clone https://github.com/TGP482/XboxXBERecomp third_party/XboxXBERecomp
git -C third_party/XboxXBERecomp checkout 40e9c35
git -C third_party/XboxXBERecomp apply ../../patches/xboxxberecomp-darkwatch.patch
cd darkwatch-hle
cmake -S tools/host -B out/host -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build out/host
out/host/xbe_recomp config/game.toml --xdk ../third_party/XboxXBERecomp/xdk   # -> generated/game
tools/harness_mac.sh                     # headless exe, native arm64, out/harness/darkwatch
REF=300 tools/run_mac.sh run1 120        # 120 s run in out/run1; frames in out/run1/cap
python3 tools/crash_mac.py               # stack of the last fault
```

`darkwatch-hle/assets/` must hold the game (links to `game_files/` work).
`XBE_KTRACE=1` logs every kernel call.

On Windows: `darkwatch-hle\build.bat` (Visual Studio with the Clang tools),
after the same clone and patch.

### Debug builds and releases

Both are made on the Mac. Release **v0.1.0-debug** on GitHub has the two zips
(no game data in either).

| | macOS arm64 (xboxrecomp port) | Windows x64 (HLE port) |
|---|---|---|
| Build | `darkwatch/build`: `make` | `darkwatch-hle/tools/build_win_cross.sh Debug` (clang + lld-link + `xwin` SDK; `brew install llvm lld xwin`, `xwin --accept-license --arch x86_64 splat --output ~/.xwin-cache/sdk`) |
| Package | `darkwatch/scripts/package_macos.sh` (bundles the Homebrew dylibs) | `darkwatch-hle/tools/package_win.sh` |
| Logs | `logs/darkwatch_<date>_<time>.log` beside `Darkwatch.command`, plus `xbox_kernel.log` | `logs\darkwatch_NNN.log` beside the exe (verbose in Debug), `CrashDumps\*.dmp` |
| State | runs: window, audio, to the level (smoke-tested from the zip) | compiles and links; **not run on Windows yet** |

The cross build's generated code comes from the Mac's `xbe_recomp`
(`XBE_RECOMP_EXECUTABLE`, see `patches/README.md`); the recompiled partitions
are always `-O2`, the runtime is `-O0 -g` in Debug, and the CRT is linked
statically (no redistributable needed).

### Open items, in order

1. **Next:** drive the HLE harness past PRESS START with a pad script
   (`SCRIPT=`), through the menus and briefing into the first level, and
   compare frames with the xboxrecomp port's.
2. Windows build and first run of the HLE port, by the owner of a Windows PC
   (logs, `XBE_PERF`, F3 captures with `XBE_CAPTURE_DIR`).
3. HLE gaps known so far:
   - stream pitch is not applied (the `0x003A924D` hook);
   - the title logo shows a dot pattern in the reference renderer;
   - symbols `xbe_symbols` could not find: D3D `BeginPush`, `EndPush`,
     `BlockOnFence`, `InsertFence`, `EnableOverlay`, `FlushVertexCache`,
     `GetVertexShaderFunction`, `PersistDisplay`, `SetPalette`, `SetTile`,
     `SwitchTexture`, `D3DResource_IsBusy`, and some DirectSound 3D ones. They
     trap if called.
4. xboxrecomp port, if it is kept alive: the CRI movie-player crash (about
   one run in six, `sub_00315330`) and a decision on the parked preemption
   diff.

## xboxrecomp port: boot sequence

Run with the switches the bring-up scripts set:

```bash
RECOMP_PB_EXEC=1 RECOMP_TITLE_KEVENTS=1 RECOMP_VBLANK=1 \
./darkwatch/build/darkwatch_recomp
```

1. XBE loaded, 24/24 sections mapped, 151/151 kernel thunks bridged.
2. CRT/XAPI startup: threads, TLS, heap, critical sections, timers, DPCs, a
   connected interrupt for the GPU (vector 3 → routine `0x0034BDF0`).
3. Storage: `\Device\Harddisk0\partition0` and `partition1` open against the
   partition images, the save-side tree (`TDATA`, `UDATA`, cache images) is
   created, and the title's own volume probe passes.
4. Assets: it opens and streams the archives — `movies1..4.pck`, `audio*.pck`,
   `singlstr.pck`, `packfile.pck` — and allocates its game heap (~3.4 MB).
5. D3D8 device creation completes: the fence handshake below is answered, the
   display mode is set (`AvSetDisplayMode mode=0x080F0D12 pitch=2560
   fb=0x00204000`), and `PCRTC_START` is programmed.
6. Graphics: it builds NV2A pushbuffers and submits them. With
   `RECOMP_PB_EXEC=1` the runtime's executor parses them cleanly (**4 segments,
   1532 words, 0 unrecognised**) and the software rasteriser draws:

   ```
   [GPU] surface 0x00330000 pitch 2560 clip 640x480+0+0 clears 1 | 396 unhandled methods (158 distinct)
   [GPU] draws 3 (3 with coordinates), 12 indices; x 0.0..64.0  y 0.0..196.8
   [GPU] rasterised 6 triangles; 654562 pixels written
   ```

   Nothing is presented to a window yet — the window is created by the D3D8 GL
   backend, and this title drives the NV2A directly, so presentation wants the
   framebuffer window (`RECOMP_FB_WINDOW` / `RECOMP_FB_DUMP_FLIPS`).

## xboxrecomp port: the GPU fence

The obstacle described in the first revision of this file — the D3D8
ring-submit spinning on a GPU fence that nothing ever advanced — is fixed with
the driver's own protocol rather than a workaround. What the title does, read
from `D3D.asm`:

| Where | What |
|---|---|
| `sub_003486F0` (InsertFence) | writes a 4-dword fence packet into the pushbuffer: `subch5:0x0310` with `(pb_va << 5) \| ((fence & 0x1F) << 2) \| (wrap & 3)` (lands in PGRAPH register `0xFD400B10`), then `subch0:0x1D70` (BACK_END_WRITE_SEMAPHORE_RELEASE) with the fence value, then `pb_write_seq += 2` |
| `sub_003484D0` (GetPushBufferOffset) | spins while `((*pb_gpu_read_ptr << 2) ^ MEM32(0xFD400B10)) & 0x7C`, and treats `*pb_gpu_read_ptr + 2 == pb_write_seq` as "idle" |
| `sub_003487B0` (KickOff / MakeSpace) | `pb_write_seq` at `dev+0x2C`, `pb_gpu_read_ptr` at `dev+0x30`, ring bounds/`wrap` at `dev+0x24/0x28/0x40`, `DMA_PUT` written from `dev+0x1C20`; on the "space is short" path it patches the pending fence packet with `WAIT_FOR_IDLE` and then waits on the event at `dev+0x1DCC` |

So the retired fence for a drained ring is `pb_write_seq - 2`, and the GPU
acknowledges a fence by writing it into the semaphore block that
`dev+0x30` points at (guest physical, reached through the contiguous window).
That is now what the runtime does:

- `src/kernel/nv2a_pb_exec.c`
  - `NV097_SET_CONTEXT_DMA_SEMAPHORE` (0x01A4) / `NV097_SET_SEMAPHORE_OFFSET`
    (0x1D6C) / `NV097_BACK_END_WRITE_SEMAPHORE_RELEASE` (0x1D70): resolves the
    DMA context handle through the **PFIFO RAMHT + PRAMIN instance table** the
    driver builds in the aperture (`sub_0034CB42` / `sub_003500D0`), then
    writes the fence value into the semaphore;
  - `subch 5, method 0x0310` writes PGRAPH `0xFD400B10`;
  - `NV097_NO_OPERATION` with a non-zero parameter — XDK D3D8's software
    methods (`sub_0034C2F0`): `low5 == 1` queues the flip (display offset into
    `PCRTC_START`), `low5 == 5` signals the fence event, `low5 == 9` writes
    `SET_ZSTENCIL_CLEAR`;
  - `NV097_WAIT_FOR_IDLE` is accepted.
- `src/kernel/xbox_memory_layout.c`
  - `xbox_Nv2aFenceSignal()` — the executor's acknowledgement path;
  - `fence_mirrors_tick()` — for the D3D8 mirror shape (`put_off == 0x2C`,
    `get_ptr_off == 0x30`) it retires `pb_write_seq - 2` into
    `*pb_gpu_read_ptr`, keeps `0xFD400B10` aligned and arms the stall event at
    `dev+0x1DCC`.
- Both hosts register the mirrors after `xbox_kernel_bridge_init()`:
  fence `(0x3548F8, 0x2C, 0x30)`, counter `(0x3548F8, 0x2478 → 0x1DE4)`,
  vblank counter `(0x3548F8, 0x1DE8)`.

Result on the next run: device creation completes, `AvSetDisplayMode` is
reached (`mode=0x080F0D12 pitch=2560 fb=0x00204000`), `PCRTC_START` is
programmed, the executor clears and rasterises, and `DMA_PUT` advances well
past the old stopping point (`0x2000 → 0x37F0 → 0x3E54 → 0x3EA4`).

## xboxrecomp port: the road to the level

The unplaced-function crashes are fixed in the detector, not one seed at a
time. Two shapes the ret-probe rejected:

1. MSVC this-adjust thunks, packed after a `ret` and named only by
   `push imm32` or by a jump from another thunk:

   ```
   mov ecx, [esp+4]     ; or add/sub/lea ecx, imm
   jmp <method>
   ```

   `probes_as_this_adjust_thunk` plus a gap pass. 37 recovered, including
   the two that crashed the boot (`0x001D4778`, `0x001D4CC0`).

2. Address-taken functions in a gap that do `ret`, but only after a switch
   the table pass did not measure, so `probes_as_returning_body` stopped.
   `0x0012FE90` was the next crash. The immediate pass now walks the decoded
   stream up to the next function and accepts a ret, rejecting a first dozen
   instructions that decode as `pop es` / `insb` / `into` (data).

After that rebuild the boot still reaches `AvSetDisplayMode`, and the ICALL
dump has **0 unresolved code targets**. It then stops pumping frames, not on a
missing function.

The XML type loader clones a registered type through the descriptor's factory
slot. Every factory is `operator new(size); jmp ctor`, where the constructor
sits in a gap right after a `ret` and is named by nothing else. The tail-jump
pass ran before the vtable pass that creates those factories' aliases, so a
jump out of a factory was not readable as a tail jump yet, the constructor
stayed unplaced, and the translator emitted a no-op stub for it. The object
came back unconstructed -- vtable slot zero held heap junk -- and the loader
dereferenced a string (`"NotKnown"` at `0x004173FC`) as a vtable.

`detect_all` now sweeps tail-jump targets again after the data-pointer pass and
iterates to a fixpoint. 88 more functions; the four constructors on the failing
path (`0x00244E5A`, `0x0024510E`, `0x002450B3`, `0x00244F98`) are real code.

Two smaller fixes on the same path:

- `RECOMP_ITAIL` popped 4 bytes on failure. A `ret 4` tail caller then came
  back 4 bytes short. The macro takes the pop count now
  (`RECOMP_ITAIL_POP`), and the translator derives it from the function's own
  `ret`.
- `sub_0022B828`-style factories were also mis-entered: a constructor at
  `0x0024510E` resolved to the *middle* of `sub_002450FA`, so only the base
  half ran and the object ended with the base vtable. Fixed by the same sweep
  (it became an explicit tail-jump entry).

`in al, dx` at `0x0034C06D` is still the driver reading port `0x80C0`. No-op.

### Past the frame pump: loader, audio, movie

**Branches out of alias bodies.** 454 conditional branches (233 distinct
sites as first measured) lifted as `if (cc) { stub(); return; }`: an alias
starts inside an enclosing function, a loop head lies before it, and the
back-edge had no entry. The stub pops a return address nobody pushed, so the
loop ran once and the loader step never reported done. `detect_all` now gives
every branch target and every measured jump-table arm that leaves its body an
alias entry, to a fixpoint. 454 → 23; the 23 hit 6 targets, all mid-instruction
(data decoded as code).

**Callback tables inside code.** XPP keeps USB descriptor tables in its own
code section; `0x0040D524` and `0x0040DD62` are reached only through them.
The data-pointer pass now scans code-section gaps (runs of ≥ 2 pointers,
targets outside bodies) and realigns a target the sweep stepped over.

**DirectSound.** `DirectSoundCreate` returned `0x88780078` (DSERR_NODRIVER):
the lifted DSOUND talks to the APU, and `RECOMP_AC97_READY` was off. On, the
first register access was a SIGBUS — the trap's decoder is x86-64 only. New
`src/apu/apu_mmio_hook_arm64.c` decodes ARM64 LDR/STR/LDP/STP. Then:

- AC'97 channel reset spins on RR (`0xFEC0xx0B` bit 1); POSIX now arms the
  write trap the Win32 host had, completing the reset on the store.
- GP DSP doorbell: DirectSound posts commands 3 and 2 at `0x806D0810` and spins.
  That is GP scratch SGE[0] (`GPSADDR=0x006DC000` → page `0x006D0000`) + 0x810,
  so it is acknowledged automatically now — no per-title address.

**Movie decoder (CRI Sofdec).** Hand-written assembly: `0x00321E90` (callback
installed by `mov [esi+0x131c], 0x321e90`), PSFD_I `0x003C64A0`, PSFD_P
`0x003C71C0`, plus switch arms `0x00324A8A`/`0x00324ABC`. All unplaced: the
sweep comes out of VLC byte tables misaligned, and the bodies `jmp` forward
before they `ret`, which the strict probe rejects. A 16-byte-aligned target
after `int3` padding, or a section start, now counts as corroboration. Also
`shl`/`shr` now mask the count to 5 bits (the bit readers shift by `cl`).

**Heap.** The movie streams through 2 MB buffers; a grow-only heap ran out
(`xbox_HeapAlloc: out of memory`) and the NULL came back as a write to 0.
`RECOMP_HEAP_RECLAIM` is now a title default, with the other boot switches
(`darkwatch_config_title_defaults`).

**Result.** The intro FMV plays and is presented: 1,577 flips in 120 s at ~5
fps through the software rasteriser, image correct.

### The movie-player crash: suspension that did not suspend

With guest code running fast (`XBOX_LOG_LEVEL` 1–2) about 60% of runs died in
the player: `sub_0030DF70` got stream handle 0 from `[player+0x131c]`, read
`[0]`, and pushed garbage into `sub_00315330`. Instrumented runs never failed.

The handle is created by the main thread and stored into the player a few
calls later; the decode worker ran `sub_003075D0` in between. That is not a
lift error or a memory-ordering one (it happens under the guest lock too). CRI
runs its workers with `NtSuspendThread`/`NtResumeThread` -- 3.8 M suspend
calls in a minute -- and the POSIX `SuspendThread` shim only counted. The
"suspended" worker never stopped.

Fixed in the runtime: suspension of spawned guest threads is cooperative (safe
points at thread start, kernel-call return, self-suspend), `CreateSuspended`
is honoured, and the guest lock is a token reserved across a kernel call for
one slice so a non-blocking call does not hand the CPU off. `RECOMP_GUEST_LOCK`
is a title default now. 8 of 8 fast runs clean; the movie runs at ~7.5 fps
(was ~5).

### After the intro: XNET

After the intro (≈ 2,200 flips, ends on the Darkwatch logo) flips stop. The
main thread is busy, not blocked: `sub_0018E510` → `sub_001A39C0` →
`sub_001A3740` → entity/asset work in `0x001AA6A0`, `0x00213xxx` (a 64-dword
block allocator) and XNET (`0x00365xxx`). A 10-minute run shows no flips, no
file reads and no crash, so it is a stall rather than a slow load.

**Fixed:** the loop was XNET. After the movie (≈ 4,360 flips -- the intro is
longer than it looked) the main loop polls a UDP socket with an overlapped
`WSARecvFrom` (`sub_001ABC00` → `sub_00357229`). XNET references
`OVERLAPPED.hEvent` with `ObReferenceObjectByHandle(…, ExEventObjectType)`,
which was a stub returning NULL, so every call failed with 0x8007277B; and
once that returned a real KEVENT, `RtlNtStatusToDosError` turned the
expected WSAEWOULDBLOCK (0x80072733) into 317. Both fixed in the toolkit; the
title screen comes up. `RECOMP_KTRACE=<file>` found it.

### Title screen

Renders. The text read `PPPPPPPPPPP` because the strings themselves did: the
title uppercases each string through a string `operator[]` split into
tail-jump pieces (0x00041FCE → 0x00041FDE), and the piece that adds the index
was an unplaced stub. Found by watching writes: `DW_WRITE_WATCH=<VA>[,<s>[,<len>]]`
read-protects the page and prints the writer's backtrace, so the chain went
vertex buffer → source mesh → string → the copy loop → operator[]. Fixed in the
detector; "PRESS START", "DEMO" and the ESRB notice now read correctly.

The emblem and the ESRB box were pitted with black: every 2D layer is drawn
at one depth under LEQUAL, and barycentric rounding put some pixels a hair
behind the layer below. Depth is now interpolated relative to one vertex and
quantised like the fixed-point buffer; both screens render clean.

### Input

The XDK's USB stack (XPP, statically linked) drives the OHCI controllers
itself. The toolkit had an OHCI model with descriptor walking and a gamepad
device, but nothing initialised it. Wiring it up took four fixes, each of
which was the whole problem until the next one:

1. Trap and serve the controller registers on macOS (AArch64 fault routing),
   run its ISR holding the guest CPU.
2. Contiguous memory now starts above the image: XPP's endpoint descriptors
   had physical addresses equal to image VAs, so the model read the image.
3. Drain the DPCs an ISR queues immediately, not every 10 ms: USB DPCs queued
   ahead of D3D's vblank DPC made it miss the vblank, and every thread waited
   in BlockUntilVerticalBlank.
4. XPP's report parser (0x004116E9) was an unplaced function reached through
   a one-slot table in its code section; the detector now accepts that shape.

`RECOMP_USB=1` is a title default. `RECOMP_PAD_PRESS` / `RECOMP_PAD_SCRIPT` /
`RECOMP_PAD_LIVE` drive the pad headless; a host gamepad goes through SDL2.
START now skips the intro and reaches the title.

### Past the title

START at the title tears down the attract-mode workers and loads the menu.
Four fixes:

- a lifter bug: `imul bl` (and the other 8/16-bit one-operand mul/div forms)
  wrote EDX, which libpng uses for the PNG height -- every PNG decoded with
  height 0 and D3DX crashed in `sub_0039A5F9`;
- the guest CPU lock had a hole the size of the main thread: XAPI starts it
  inline inside PsCreateSystemThreadEx, and after its first blocking call it
  ran unlocked; file APCs too;
- the vblank interrupt status was cleared by the register stand-in before
  D3D's DPC read it, so render workers never left BlockUntilVerticalBlank
  and the main thread waited 30 s per worker for them to exit;
- re-initialising a KEVENT no longer orphans its waiters.

The LOADING screen with the menu art now renders. The crash after it was
D3DX trying its image decoders in turn: libjpeg's (`sub_0038F983`) frame is
`push ebp; lea ebp, [esp-0x70]`, which the translator called frameless, so
its setjmp saved the wrong ebp and its error longjmp left esp 0x90 high.

The disc-error screen after it was the Z: cache copy of audioWpn.pck failing
on an unhandled NtSetInformationFile class (19, allocation) on POSIX.

**The NEW PROFILE menu now comes up.**

Saved profiles showed as DAMAGED: empty "Profile N" files beside the real
one. XAPI's FindFirstFile closes its search after the first match, and on
POSIX the directory enumeration outlived the close. The next directory opened
at the same host handle continued the old search, so the game's probe for a
free profile name got stale answers and created empty files. NtClose now drops
the enumeration on POSIX as it already did on Win32. **An existing profile now
loads into the PROFILES screen.**

About a third of runs then sat on the bare DARKWATCH logo. The front end
stops its three CRI workers by setting a flag, resuming each one and waiting
up to 30 x 1 s for it to exit. The cooperative-suspend slots were never freed,
so a worker created at a reused host handle value had its resume delivered to
a dead slot and never started. Slots now die with their thread and unbind
when their handle closes.

### Layer 2: NV2A methods

The `[GPU]` ranking went from 154 distinct unhandled methods to one (0x147C,
sent once with 0, not in xemu's tables either). What was added, all in the
toolkit's software executor:

- fixed-function transform and lighting (`nv2a_ff.c`, after xemu's
  `vsh-ff.c`): composite/model-view/inverse/projection/texture matrices,
  skinning, texgen, eight lights, colour material, specular, fog generation.
  A FIXED batch used to be drawn only if its positions looked like pixels.
- back-face culling, window clip (scissor), clear rectangle, stencil test and
  operations, polygon offset, depth clip range (`SET_CLIP_MIN/MAX`), flat
  shading, texture colour key and alpha kill;
- the 2D classes on the other subchannels: NV062 surfaces, NV09F image blit,
  NV012 beta;
- current attribute values for arrays a batch does not supply, and the
  fixed-function names for them (`SET_NORMAL3F`, `SET_DIFFUSE_COLOR4UB`, ...);
- every other triangle of a strip was rasterised with reversed winding,
  invisible until culling existed;
- the rasteriser's row-split worker pool now exists on POSIX too.

The intro runs at ~19 clears/s (was ~12). The intro itself exercises only
the vertex-program path and culling, so the rest is checked by two new
toolkit tests, `tests/nv2a_ff` and `tests/nv2a_raster`, until the title gets
past the stall.

The CRI movie-player crash (`sub_00315330`, player `0x020DF7A0`) came back
once in about six runs after the GPU got faster: suspension narrowed the race,
it did not close it.

### Into the menus, and the DSP boot ROM

With the pad driven live (`RECOMP_PAD_LIVE`, frames from
`RECOMP_FB_WINDOW_DUMP_EVERY`), the front end now goes all the way through:
PROFILES -> main menu -> SINGLE PLAYER -> CHAPTER (THE WRONG TRAIN, preview
movie playing) -> DIFFICULTY.

Picking a difficulty went to a black screen with the main thread inside
DirectSound: `sub_003AC6A2` resets both DSPs (`sub_003AC462`,
`KeStallExecutionProcessor(10000)`) whenever EP P:6 (`*0x3C6218` =
`0xFE85A018`) is not `0xCCCCCC`. That value is word 6 of the `DOLBY` section,
the EP image DirectSound stages in scratch memory; on hardware the DSP boot
ROM copies scratch into P-RAM when the reset is released (xemu's
`dsp_bootstrap`). The APU model has no DSP, so P-RAM stayed zero and every
service pass reset the DSPs again. The model now does the boot ROM's copy
(`apu_dsp.c`), and the log shows `EP out of reset: ... (P:6 = 0xCCCCCC)`.

### Level start: file APCs ran too early

The black screen after DIFFICULTY was not a device wait. The flow object
(`0x01672DF0`, vtable `0x424B3C`) reaches in-game mode, and `sub_000C2ADB`
starts the level only when the audio loader at `[0x4AD2FC]` reports
`+0xBE4 == 2`. That loader copies the level's audio pack to the Z: cache in
64 KB chunks with `ReadFileEx`/`WriteFileEx` and a sub-state at `+0xBE8`:

    ReadFileEx(src, buf, n, &ovl, sub_00206415);  state = 1  /* waiting */
    sub_00206415:  pos += n;                       state = 2  /* read done */

The bridge ran the completion APC inside NtReadFile, before `ReadFileEx`
returned, so state 2 was overwritten with 1 and the loader slept in
`SleepEx(0, TRUE)` for an APC that had already run. APCs now queue per
thread and run at the next alertable user-mode wait, as on NT.

The live state gave it away: `+0xC04` (bytes read) had already advanced
while the sub-state still said "waiting for the read".

A dead end on the way, recorded so it is not repeated: the flow also hands
`sub_000C591E` to the network manager (`[0x4B4FC0]+0x1C`) when it leaves the
front end. That is the "session lost" handler (forcing it shows XBOX LIVE
"The game session is no longer available"), not a readiness signal.

### In the level: unlifted functions, not audio

With that fix the level loads and the mission briefing for THE WRONG TRAIN
comes up ("Tonight's robbery..."). After CONTINUE the screen went black. The
XACT time in a `sample` was a red herring: the game flow (`0x01672DF0`) had
reached mode 5 (playing), the frame loop ran a thousand times a second, and
the only subsystem the engine ticked was the flow itself. The log held the
answer: `[ICALL] Failed to resolve VA 0x00011C59` at the moment of the press.

0x00011C59 is a vtable method packed straight after the previous body's
`ret 4`. The data-pointer pass in the detector does consider vtable slots,
but a target in a gap was only probed for 64 instructions, and this one runs
104 before its first `ret`. Four other misses in the same log
(0x0006C695, 0x0007172E, 0x0009801D, 0x000AB894) had the same shape. A
target right after a sweep-decoded `ret` now gets the full probe: 61 new
entry points, nothing removed or moved.

Each fix let the level load run further and hit the next gap:

- 0x00016C60 follows a factory that is itself only a gap alias, so "after
  the `ret` of a measured body" was too narrow; any decoded `ret` vouches.
- 0x002914B6 is an arm of the switch at 0x00292020 in 0x00291270, which the
  detector splits into three pieces. The table's first arm lies in a sibling
  piece, and the lifter stopped reading the table there, so the switch became
  an unresolved indirect jump and the load crashed in `sub_00287EB0`. The
  table walk now skips arms that are known function starts.
- 0x002233BB is a callback pushed as an immediate, placed after
  `call <fatal>; int3`. The end walk ran over the `int3`, so the address
  looked mid-body. `int3` now ends a body like `ret`.

With all four, a run reports no unresolved indirect calls, and CONTINUE
drops into the level: HUD, revolver, "CHECKPOINT...". Regenerate after
updating the toolkit (`scripts/regenerate_recomp.sh`, or the disasm and
recomp steps of `bringup_iterate.sh` to keep the observed seeds).

### Where the xboxrecomp port stops: frame rate

Scripted play works: the left stick walks to the first door ("USE X TO
MELEE ATTACK THE DOOR LOCK"), the right stick looks, and RT fires (the
cylinder drops from six rounds to four). But the level runs at about
1.6 fps. The main thread is idle in D3D's flush spin (`sub_003485D0`,
waiting on PFB 0x100410); the NV2A thread is 96% busy in the software
rasteriser, mostly `sample_tex`, `xf_rows` and the register combiners. The
row-split pool hands out one triangle at a time, and most level triangles
are under the size that splits, so the 11 workers sit near 12% each.

Tools that work against the stuck state: `sample`, `RECOMP_KTRACE`, and
lldb attached with
`settings set platform.plugin.darwin.ignored-exceptions EXC_BAD_ACCESS|EXC_BAD_INSTRUCTION`
(the runtime's MMIO traps otherwise stop it on every access). The guest
registers are thread-locals: call the TLV thunk (first word of `g_ecx`'s
descriptor) with the descriptor address to find them. `g_icall_trace` holds
the last 16 indirect-call targets.

## HLE port on XboxXBERecomp (`darkwatch-hle/`)

The level ran at 1.6 fps through the software NV2A rasteriser, and Windows is the
required target. So the game moves to
[XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp) (MIT, the toolkit behind the
BLACK and Splinter Cell DA ports): Direct3D 8 and DirectSound implemented at the API
level on Direct3D 12 and XAudio2, instead of the NV2A and APU emulated underneath. The
xboxrecomp port above stays as the reference and fallback.

Darkwatch is XDK 5849 throughout, the build the SDK's signatures were made from:
`xbe_symbols` finds 180 of 238 functions (97 of the 109 D3D ones) and all 12 data
symbols. `config/game.toml` is its output plus hand edits.

On the Mac the SDK's headless harness builds natively (`tools/harness_mac.sh`) and runs
(`tools/run_mac.sh <name> <seconds>`, `REF=N` for reference-renderer frames in
`out/<name>/cap`). A fault writes a macOS crash report; `tools/crash_mac.py` prints its
stack. With `XBE_KTRACE=1` the log has every kernel call.

### Spike: boot to PRESS START

Each step got further, in order:

1. 16 KB host pages (Apple silicon): commits failed, then a re-commit inside a host page
   kept old bytes and the heap's free list read garbage. Commits round to the host page
   and zero what was not committed.
2. `CreatePixelShader`: the game scans the XDK's pixel shader object for its definition
   copy (and edits constants in it). The HLE object now has the XDK's shape.
3. Kernel export 232 `NtUserIoApcDispatcher`, the APC routine of `ReadFileEx`.
4. XACT reads the APU sample counter XGSCNT (0xFE80200C) itself; it is now a real page
   the mixer advances.
5. XACT's 3D voices call through the HRTF table `DirectSoundUseFullHRTF` installs; that
   function keeps its body.
6. XACT calls eight `CDirectSoundStream` methods the signatures do not name, each writing
   a hardware-voice parameter. `src/game/hooks.cpp` makes them `S_OK` no-ops. Pitch
   (0x003A924D) is among them and is not applied yet.
7. `XOnlineGetUsers` returns `XONLINE_E_NOT_INITIALIZED`, because the host's
   `XOnlineStartup` builds no XOnline object; the front end then dereferenced the null
   UIX object it left. A hook reports zero stored accounts. (Running the real
   `XOnlineStartup` instead needs the real XNet internals the SDK replaces.)

After that a 120 s headless run renders the title screen, DARKWATCH / PRESS START, with
no faults. The logo shows a regular dot pattern in the reference renderer, still to be
compared against hardware captures.

### Next step (HLE)

Drive the harness past PRESS START with a pad script, through the menus and the briefing
into the first level, comparing frames with the xboxrecomp port's. Then the Windows
build (`build.bat`): the D3D12 renderer and XAudio2 have to be run there.

## Diagnostics that earned their keep

| Switch | What it shows |
|---|---|
| `XBOX_LOG_LEVEL=4` | the kernel log file (`xbox_kernel.log`) with path translation and file opens |
| `RECOMP_FILE_TRACE=1` | every guest open: raw `OBJECT_ATTRIBUTES`, options, resolved path |
| `RECOMP_PB_EXEC=1` | executes the title's pushbuffers; `[PB]` inventory + `[GPU]` reports |
| `RECOMP_PB_SCAN=1` | read-only pushbuffer survey, no execution |
| `RECOMP_VBLANK=1` | delivers the GPU vblank interrupt to the connected ISR |
| `RECOMP_NV2A_INTR_VECTOR=<n>` | which vector to deliver it on (3 default; the title connects 3) |
| `RECOMP_TITLE_KEVENTS=1` | recognises events the title built by writing their dispatcher header |
| `RECOMP_GPU_IDLE_FENCE=1` | legacy bring-up lever: satisfies a fence wait outright |
| `RECOMP_FB_DUMP_FLIPS=1` | writes each presented frame to a BMP, no window needed |
| `RECOMP_FRAME_TRACE=1` + `RECOMP_FRAME_TRACE_METHODS=1` | method stream of one frame |
| `DARKWATCH_PROBE=0xVA[,+off]` | on stop/crash, follows the dword at `0xVA` and dumps memory around it |
| Ctrl-C / `SIGTERM` | dumps ICALL feedback and runs the probe |

`sample <pid> 2` on macOS is the fastest way to find the guest function a spin
is in: the generated code keeps every guest function as a named C symbol.
