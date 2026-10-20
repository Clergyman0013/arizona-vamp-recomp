# Plan

Where the project goes next, and why. BRINGUP.md has the evidence behind each
point; this file is the decisions and the order.

## The two ports, and which one carries the project

| | `darkwatch/` on xboxrecomp | `darkwatch-hle/` on XboxXBERecomp |
|---|---|---|
| Toolkit | [sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp) **v0.14.0** + `patches/xboxrecomp-darkwatch.patch` | [TGP482/XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp) 40e9c35 + `patches/xboxxberecomp-darkwatch.patch` |
| Graphics / audio | NV2A and APU emulated in software | D3D8 on D3D12, DirectSound on XAudio2 |
| Reaches | first level, playable, ~1.6 fps, in a macOS window | title screen (headless harness) |
| Role | **reference**: frame and behaviour comparisons, bisecting | **main line**: Windows is the target |

The main line stays the HLE port. The xboxrecomp port is kept building and
current with upstream because it is the only one that already plays the level:
when the HLE port does something wrong, the xboxrecomp port shows what right
looks like.

## The upstream iterative process, and where we stand on it

xboxrecomp's README describes bring-up as six steps (Boot, Stub, Fix ICALLs,
Add runtime, Debug, Iterate), driven by crashes, with `bringup_iterate.sh`
folding the runtime's indirect-call dump back into function detection.

| Step | xboxrecomp port | HLE port | Note |
|---|---|---|---|
| 1. Boot | done | done | |
| 2. Stub | done | done (`src/game/hooks.cpp`) | |
| 3. Fix ICALLs | done, fixed in the detector rather than seeded one by one | not needed so far (its own discovery) | |
| 4. Add runtime | done (APC timing, DSP boot ROM, fences, APU, USB...) | in progress: 7 fixes to reach PRESS START | |
| 5. Debug | done | tools in place (`XBE_KTRACE`, crash reports) | |
| 6. Iterate | at the level; blocked on speed | at PRESS START | |

We are on track: both ports followed this loop and both advance with it.
Four places where the loop as written was not enough, and what we do instead:

1. **Not every failure is a crash.** The level start and the black screen after
   the briefing were stalls: no fault, a running frame loop, nothing to
   iterate on. The loop needs a stall step: `sample` the process, trace kernel
   calls (`RECOMP_KTRACE` / `XBE_KTRACE`), and read the title's own state
   machine. Kept as the standing first move when a run stops making progress.
2. **The seed feedback goes stale.** `bringup_iterate.sh` merges
   `icall_targets.dump` into the detector's seeds, but the seeds file here was
   last refreshed on 2026-10-15. Each bring-up run should dump and merge again,
   or a function the detector stops finding is only found by crashing on it.
3. **A toolkit upgrade can lose functions without a test failing.** v0.14.0's
   new discovery (closed-CFG proofs, #176/#182) found 1,700 fewer starts in
   Darkwatch than our v0.13.1 + patch, among them vtable-only methods the
   front end calls at boot (0x000EF6C1, 0x001D37AC, 0x001F2AD9). Every upgrade
   now gets a function-set comparison against the last known-good list, then a
   scripted run to the level, before it is committed.
4. **The loop ends at "it renders", not "it runs".** Rendering at speed is not
   a step in it. For a title that drives the NV2A itself, as Darkwatch does,
   xboxrecomp offers the software executor and, since v0.14.0, a back-end
   interface with no GPU renderer behind it. That gap is why the main line moved
   to the HLE toolkit.

## Next steps, in order

1. ~~**Finish the xboxrecomp v0.14.0 upgrade.**~~ Done 2026-10-16: the
   patch is rebased (`patches/README.md`), the toolkit tests match pristine
   v0.14.0, and a scripted run in the SDL window goes boot → menus →
   difficulty → intro → briefing → level, no faults, 1.5 fps in the level.
2. **HLE port: past PRESS START into the level** with the headless harness on
   macOS (pad scripts, reference-renderer frames compared with the xboxrecomp
   port's).
3. **HLE port on Windows.** The exe is cross-built on the Mac
   (`tools/build_win_cross.sh`, release v0.1.0-debug) and links; next is the
   first run on the Windows PC: logs, `XBE_PERF`, F3 thread dumps and
   captures. Fix boot, then the renderer, then audio.
4. **HLE gaps already known:** stream pitch (the `0x003A924D` hook), the dot
   pattern on the title logo, the D3D symbols the scanner did not find (they
   trap if called).
5. **Optional, for a playable macOS build:** an OpenGL 4.1 `gpu::Backend` for
   XboxXBERecomp, porting the HLSL that `runtime/gpu/shaders.cpp` emits to
   GLSL, plus SDL3 audio. Only after the Windows renderer works, so that there
   is a reference to match.

## macOS or Windows for development

- **Logic, boot, progression:** macOS works for both ports.
  - The HLE port's headless harness builds natively on Apple silicon. Its CPU
    reference renderer shows frames as images: no window, and slow.
  - xboxrecomp v0.14.0 runs natively on arm64 and, with `RECOMP_FB_WINDOW`,
    shows the game in an SDL window with audio. That is still the software
    rasteriser: about 1.6 fps in the level. The release adds no GPU renderer.
- **Rendering at speed, audio, deployment:** Windows. The main line's renderer
  is Direct3D 12 and its audio XAudio2; neither exists on macOS. Until step 5
  exists, every check of how the game looks and runs at speed happens on the
  Windows PC.

So: keep developing on the Mac, and use the Windows PC for every rendering,
performance and audio check of the HLE port.
