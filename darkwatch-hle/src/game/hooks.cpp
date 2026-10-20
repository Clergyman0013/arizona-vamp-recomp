// Hooks for Darkwatch's default.xbe. A strong definition replaces the recompiled function at that address.
#include <cstring>

#include "xdk/hle.h"

// DirectSound stream methods the XDK 5849 signatures do not name, which XACT calls on the host's streams. Each one
// takes the stream and one argument and writes a parameter into the stream's hardware voice (through this+0x10 or
// this+0xC): the 3D source block, a voice flag bit, LFO/envelope-style registers, the pitch. The host's streams
// have no hardware voice -- run as recompiled, these dereferenced a null voice -- and its mixer reads none of
// these registers, so they keep the XDK's result and do nothing else.
// ponytail: 0x003A924D sets the voice pitch; streams play at their own rate until it maps onto the mixer's
// frequency (pitch is 4096 per octave against 48 kHz).
#define STREAM_VOICE_SETTER(va)                          \
    X86_FUNC(va) {                                       \
        (void)base;                                      \
        kernel::Return(ctx, 0, 2); /* S_OK, stdcall */   \
    }

// GetVoiceProperties(this, DSVOICEPROPS*): two of the methods above were getters, not setters. XACT reads the
// props on a stack local (0x003E5BB9) and walks MixBinVolumePairs up to dwMixBinCount (0x003E8688); left unwritten,
// the count was stack garbage (0x523) and the walk ran off the guest stack when the front end's movies2.pck started.
// The host voice is out of reach here, so report no mix bins, pitch 0 and full I3DL2 volumes; XACT then adds no
// per-bin attenuation. DSVOICEPROPS (5849): count, 8 {bin, volume} pairs, lPitch, lI3DL2Direct/RoomVolume.
// ponytail: report the voice's real bins and volumes once dsound.cpp exposes them.
#define VOICE_PROPS_GETTER(va)                                                          \
    X86_FUNC(va) {                                                                      \
        (void)base;                                                                     \
        uint32_t props = kernel::R32(ctx.esp + 8);                                      \
        for (uint32_t o = 0; props && o < 0x50; o += 4) kernel::W32(props + o, 0);      \
        kernel::Return(ctx, 0, 2); /* S_OK, stdcall */                                  \
    }
VOICE_PROPS_GETTER(sub_003A9165)  // IDirectSoundBuffer_GetVoiceProperties (B0055) via the thunk at 0x003A9795
STREAM_VOICE_SETTER(sub_003A91B3)  // via 0x003A97B1: copies a 3D source block into the voice
STREAM_VOICE_SETTER(sub_003A9201)  // via 0x003A97CD: sets or clears bit 0 of voice byte 0xB
STREAM_VOICE_SETTER(sub_003A924D)  // via 0x003A97EE (B0099): voice+0x18, then the pitch register
STREAM_VOICE_SETTER(sub_003A92F1)  // via 0x003A97F3 (B0092): voice setter 0x003AEDDD
VOICE_PROPS_GETTER(sub_003A948B)   // stream GetVoiceProperties (B0081) via 0x003A9829
STREAM_VOICE_SETTER(sub_003A94DD)  // via 0x003A982E: 3D source block
STREAM_VOICE_SETTER(sub_003A952F)  // via 0x003A9833: voice flag bit
// The last five bodies are reached only by a `jmp` from their thunk, and the recompiler folds such a body into the
// thunk's function: there is no sub_003A924D to replace, and XACT's call ran the thunk's copy (it crashed at
// 0x003A9568, in the copy of 0x003A952F). The thunks are the real entry points; the body hooks stay for a
// regeneration that places the bodies on their own.
STREAM_VOICE_SETTER(sub_003A97EE)  // B0099, body 0x003A924D
STREAM_VOICE_SETTER(sub_003A97F3)  // B0092, body 0x003A92F1
VOICE_PROPS_GETTER(sub_003A9829)   // B0081, body 0x003A948B
STREAM_VOICE_SETTER(sub_003A982E)  // body 0x003A94DD
STREAM_VOICE_SETTER(sub_003A9833)  // body 0x003A952F
// IDirectSoundStream_SetOutputBuffer (B0098, body 0x003A9F72 folded into the thunk the same way): XACT routes its wave bank streams into a submix
// buffer after creating them; run as recompiled it read the null voice at +0xB0. The SDK's
// IDirectSoundBuffer_SetOutputBuffer is a no-op too: the voice keeps its own mix bins.
STREAM_VOICE_SETTER(sub_003AA08A)
// The same kind of setter on buffers, this-adjusting thunks into CDirectSoundBuffer: B0076 SetPitch, B006D SetLFO,
// B0067 SetEG (ids between the named SetOutputBuffer/SetPlayRegion, SetI3DL2Source/SetLoopRegion, and
// SetCurrentPosition/SetFilter). Not seen to crash yet; they would read the same missing voice.
STREAM_VOICE_SETTER(sub_003A95E5)  // SetPitch -> 0x003A8F91
STREAM_VOICE_SETTER(sub_003A9601)  // SetLFO   -> 0x003A8FDF
STREAM_VOICE_SETTER(sub_003A961D)  // SetEG    -> 0x003A902D

// IDirectSound_CreateSoundStream(this, desc, stream, unknown): .XTLID B003B, between CreateSoundBuffer (B003A) and
// DownloadEffectsImage (B003C), which the signatures do not name. XACT creates its wave bank streams through it when
// the front end loads audioWpn.pck. Run as recompiled it built a console stream on a null hardware voice
// (sub_003A9C25 -> sub_003A8F1A read guest 0x4); the device is the host's, so make the stream DirectSoundCreateStream
// does, as the SDK's IDirectSound_CreateSoundBuffer does for buffers.
X86_FUNC(sub_003AAE5C) {
    (void)base;
    uint32_t desc = kernel::R32(ctx.esp + 8), stream = kernel::R32(ctx.esp + 12);
    uint32_t hr = kernel::CallGuest(ctx, SYM(DirectSoundCreateStream), {desc, stream});
    kernel::Return(ctx, hr, 4);  // stdcall, ret 0x10
}

// The resource wait inside D3D's vertex and index buffer Lock (0x00342410 / 0x00342460 call it unless the flags say
// NOOVERWRITE or READONLY): a surface waits for the push buffer, anything else for the fence in its lock field
// (+8) through D3D_BlockOnTime, which the SDK makes a no-op. D3D_BlockOnResource is the SDK's wait for the same
// thing, so a lock that should wait does.
X86_FUNC(sub_00348B60) {
    (void)base;
    uint32_t res = kernel::R32(ctx.esp + 4);
    kernel::CallGuest(ctx, SYM(D3D_BlockOnResource), {res});
    kernel::Return(ctx, 0, 1);  // stdcall, ret 4
}

// The texture setup for a material stage (this, desc, texture holder, 2 more): for screen space stages (desc+8 != 0)
// it loads the texture coordinate scale into c112 (register 16), 1/size for a power of two texture and 2.0 for the
// others. The meat locker fog's haze draws (vertex program B01034F1) map screen pixels through that scale onto the
// previous frame, a 640x480 linear texture, so 2.0 reads past its far edges and CLAMP smears the edge row and column
// into the fog's vertical streaks; the console shows none (references/level1-meatlocker-fog_xbox.png). A linear
// texture takes texels, so a screen pixel is 1.
X86_EXTERN_FUNC(__imp__sub_001CDEC7);
X86_EXTERN_FUNC(sub_00342B10);  // D3DDevice_SetVertexShaderConstant1 (ecx register, edx vector)
X86_FUNC(sub_001CDEC7) {
    uint32_t self = ctx.ecx, desc = kernel::R32(ctx.esp + 4), holder = kernel::R32(ctx.esp + 8);
    __imp__sub_001CDEC7(ctx, base);
    if (!holder) holder = kernel::R32(self + (kernel::R32(desc + 4) == 2 ? 0xC : 0x8));
    uint32_t tex = holder ? kernel::R32(holder) : 0;
    if (!kernel::R32(desc + 8) || !tex) return;
    uint32_t w = kernel::R32(tex + 0xC), h = kernel::R32(tex + 0x10);
    if (!(w & (w - 1)) && !(h & (h - 1))) return;  // power of two: the game's 1/size is right
    // the game's vector with x and y at 1: (1, 1, offset +0x1C, offset +0x1E)
    float scale[4] = {1, 1, float(int16_t(kernel::R32(tex + 0x1C))), float(int16_t(kernel::R32(tex + 0x1E)))};
    uint32_t eax = ctx.eax, v = ctx.esp - 0x40;
    for (int i = 0; i < 4; i++) {
        uint32_t bits;
        std::memcpy(&bits, &scale[i], 4);
        kernel::W32(v + i * 4, bits);
    }
    ctx.ecx = 112;
    ctx.edx = v;
    ctx.esp -= 4;
    kernel::W32(ctx.esp, 0x1CDFF0);  // return address, popped by the call
    sub_00342B10(ctx, base);
    ctx.eax = eax;
}

// The CRI movie player's handle check (cdecl: handle, out_a, out_b): it tells the handle's kind by its vtable and
// writes two counts, 0 and 0 for a handle of neither kind. sub_00303DF0 passes it the handle sub_0030DE00 finds
// for the player's current stream, and that lookup leaves 0 when the stream's slot is not set up yet (it reports CRI
// error FF000401 and goes on). The host's threads run truly side by side, so the decoder sometimes gets there first:
// the check then read the null handle's vtable (guest 0) and the game crashed, at the intro or a briefing's movie.
// A null handle is of neither kind.
X86_EXTERN_FUNC(__imp__sub_0030E2B0);
X86_FUNC(sub_0030E2B0) {
    if (!kernel::R32(ctx.esp + 4)) {
        static bool logged = false;
        if (!logged) LOGR("cri: movie stream handle not ready yet, counted as none");
        logged = true;
        kernel::W32(kernel::R32(ctx.esp + 8), 0);
        kernel::W32(kernel::R32(ctx.esp + 12), 0);
        kernel::Return(ctx, 0, 0);  // cdecl
        return;
    }
    __imp__sub_0030E2B0(ctx, base);
}

// LuaComponent's update (thiscall, dt in seconds; ret 4): it resumes the actor's scripts only when dt is at least
// 0.01 and reads dt for nothing else. Above 100 fps every frame is shorter, so scripted actors never updated
// (Darkwatch: Darker Edition saw WrongTrain's first door stay shut at 144 fps). A shorter frame counts as 0.01, so
// the scripts run each frame, as they do at 60.
X86_EXTERN_FUNC(__imp__sub_0002A359);
X86_FUNC(sub_0002A359) {
    uint32_t bits = kernel::R32(ctx.esp + 4);
    float dt;
    std::memcpy(&dt, &bits, 4);
    if (dt > 0 && dt < 0.01f) {
        static bool logged = false;
        if (!logged) LOGR("scripts: a frame under 10 ms (%.1f ms) counts as 10 for actor scripts", dt * 1000);
        logged = true;
        dt = 0.01f;
        std::memcpy(&bits, &dt, 4);
        kernel::W32(ctx.esp + 4, bits);
    }
    __imp__sub_0002A359(ctx, base);
}

// XOnlineGetUsers(users, count): the Xbox Live accounts stored on the hard disk. The host's XOnlineStartup succeeds
// without making the library's XOnline object, and this one returns XONLINE_E_NOT_INITIALIZED (0x80150005) when
// that object is null -- which a console never sees. Darkwatch's front end ignores the result and loads its UIX
// without the user list, then reads through the null object it left behind. Report no stored accounts instead.
X86_FUNC(sub_00369E76) {
    (void)base;
    kernel::W32(kernel::R32(ctx.esp + 8), 0);  // *count = 0
    kernel::Return(ctx, 0, 2);                  // S_OK, stdcall
}
