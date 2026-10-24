// More voices for the game's streamed 3D sounds.
//
// The sound manager (made by sub_00206E43) keeps three records at +0xB4C (0x20 bytes each) for the 3D sounds that
// stream their waves, each with an XACT sound source of its own. sub_00206044 gives a new sound a free record, or
// takes the one of the least important sound playing (by priority, then by distance). In the Wrong Train two of
// the three are held for the whole level by priority 1 and 5 loops from its start, so every other priority 0 sound
// shares the last one, and one farther than the sound on it is turned away (0x1E). The horse's cry in the straw car
// (cue HORSE_WAKE01) is asked for once, as the player walks up, and is lost that way.
//
// So the pool gets three more records, kept in guest memory of their own (the manager's other fields stay where
// they are), and XACT is made with three more voices and streams for their sources. The manager's 57 other 3D
// records and these six stay within the 64 sources it asks XACT for. In the first cars of the level 16 of 17 such
// sounds were turned away with three records, and 2 of 126 with six.
#include <cstring>

#include "core/log.h"
#include "xdk/hle.h"

namespace sound_pool {
namespace {

constexpr uint32_t kRecord = 0x20, kOwn = 3, kExtra = 3;
constexpr uint32_t kOwnAt = 0xB4C;  // the manager's own three
constexpr uint32_t kEngine = 0x400; // the manager's XACT engine

constexpr uint32_t kAlloc = 0x0024C546;   // operator new, cdecl
constexpr uint32_t kCtor = 0x0020DEF9;    // record: construct, thiscall
constexpr uint32_t kDtor = 0x0020DF53;    // record: destruct, thiscall
constexpr uint32_t kInit = 0x0020DD84;    // record: make its sound source (the engine), thiscall, ret 4 -> al
constexpr uint32_t kFree = 0x0020DF1F;    // record: release its sound source, thiscall
constexpr uint32_t kBusy = 0x0020DD64;    // record: made and in use -> al
constexpr uint32_t kPriority = 0x0020DD71;  // record: its sound's priority, 0 when free
constexpr uint32_t kStop = 0x0020DE9E;    // record: stop its sound, thiscall
constexpr uint32_t kStart = 0x0020DE53;   // record: start (priority, distance, sound, out), thiscall, ret 0x10

uint32_t s_manager = 0, s_extra = 0;  // the manager the extra records are made for, and the records

uint32_t Record(uint32_t manager, uint32_t i) {
    return i < kOwn ? manager + kOwnAt + i * kRecord : s_extra + (i - kOwn) * kRecord;
}

uint32_t Count(uint32_t manager) { return manager == s_manager && s_extra ? kOwn + kExtra : kOwn; }

float F32(uint32_t at) {
    float f = 0;
    if (at) std::memcpy(&f, kernel::g_base + at, 4);
    return f;
}

struct Saved {
    X86Context& ctx;
    uint32_t ebx, esi, edi, ebp;
    explicit Saved(X86Context& c) : ctx(c), ebx(c.ebx), esi(c.esi), edi(c.edi), ebp(c.ebp) {}
    ~Saved() { ctx.ebx = ebx, ctx.esi = esi, ctx.edi = edi, ctx.ebp = ebp; }
};

}  // namespace
}  // namespace sound_pool

using namespace sound_pool;

// XACT's engine, made once by the manager: three more voices and streams (the parameters: voices, 3D sources,
// streams, ...)
X86_EXTERN_FUNC(__imp__sub_003E27AB);
X86_FUNC(sub_003E27AB) {
    const uint32_t params = kernel::R32(ctx.esp + 4);
    if (params) {
        kernel::W32(params, kernel::R32(params) + kExtra);
        kernel::W32(params + 8, kernel::R32(params + 8) + kExtra);
        LOGR("sound pool: XACT voices %u, streams %u", kernel::R32(params), kernel::R32(params + 8));
    }
    __imp__sub_003E27AB(ctx, base);
}

// the manager's records get their sources: the extra ones too
X86_EXTERN_FUNC(__imp__sub_002061A6);
X86_FUNC(sub_002061A6) {
    const uint32_t manager = ctx.ecx;
    Saved saved(ctx);
    __imp__sub_002061A6(ctx, base);
    uint32_t ok = ctx.eax & 0xFF;
    if (ok && manager != s_manager) {
        s_extra = kernel::CallGuest(ctx, kAlloc, {kExtra * kRecord});
        s_manager = s_extra ? manager : 0;
        for (uint32_t i = 0; s_extra && i < kExtra; i++) {
            ctx.ecx = s_extra + i * kRecord;
            kernel::CallGuest(ctx, kCtor, {});
        }
    }
    if (ok && manager == s_manager) {
        for (uint32_t i = kOwn; i < kOwn + kExtra; i++) {
            ctx.ecx = Record(manager, i);
            if (!(kernel::CallGuest(ctx, kInit, {kernel::R32(manager + kEngine)}) & 0xFF)) {
                LOGR("sound pool: extra record %u has no sound source", i);
                ok = 0;
                break;
            }
        }
        if (ok) LOGR("sound pool: %u streamed 3D sounds (was %u)", kOwn + kExtra, kOwn);
    }
    ctx.eax = (ctx.eax & ~0xFFu) | ok;
}

// and give them up
X86_EXTERN_FUNC(__imp__sub_00206215);
X86_FUNC(sub_00206215) {
    const uint32_t manager = ctx.ecx;
    Saved saved(ctx);
    if (manager == s_manager && s_extra)
        for (uint32_t i = kOwn; i < kOwn + kExtra; i++) {
            ctx.ecx = Record(manager, i);
            kernel::CallGuest(ctx, kFree, {});
        }
    ctx.ecx = manager;
    __imp__sub_00206215(ctx, base);
}

// the manager goes: so do the extra records (their memory is the game heap's; one manager lives for the game)
X86_EXTERN_FUNC(__imp__sub_0020705D);
X86_FUNC(sub_0020705D) {
    const uint32_t manager = ctx.ecx;
    __imp__sub_0020705D(ctx, base);
    if (manager != s_manager || !s_extra) return;
    Saved saved(ctx);
    const uint32_t eax = ctx.eax;
    for (uint32_t i = kOwn; i < kOwn + kExtra; i++) {
        ctx.ecx = Record(manager, i);
        kernel::CallGuest(ctx, kDtor, {});
    }
    s_manager = s_extra = 0;
    ctx.eax = eax;
}

// sub_00206044 (thiscall: priority, distance pointer, sound, out; ret 0x10) over all the records, in the same
// order and by the same rules: a free record first; else the least important sound's, when the new one is more
// important, or as important and nearer; 0x1D with no record made, 0x1E when it is turned away
X86_FUNC(sub_00206044) {
    const uint32_t manager = ctx.ecx;
    const uint32_t priority = kernel::R32(ctx.esp + 4), distance = kernel::R32(ctx.esp + 8);
    const uint32_t sound = kernel::R32(ctx.esp + 12), out = kernel::R32(ctx.esp + 16);
    uint32_t result;
    {
        Saved saved(ctx);
        const uint32_t n = Count(manager);
        int pick = -1, free = -1;
        uint32_t least = 0;
        float least_distance = -1.0f;
        for (uint32_t i = 0; i < n && free < 0; i++) {
            const uint32_t r = Record(manager, i);
            ctx.ecx = r;
            if (!(kernel::CallGuest(ctx, kBusy, {}) & 0xFF)) {
                free = int(i);
                break;
            }
            ctx.ecx = r;
            const uint32_t p = kernel::CallGuest(ctx, kPriority, {});
            if (pick < 0 || p < least) {
                least = p;
                least_distance = F32(kernel::R32(r + 0x14));
                pick = int(i);
            }
        }
        if (free >= 0) {
            ctx.ecx = Record(manager, uint32_t(free));
            result = kernel::CallGuest(ctx, kStart, {priority, distance, sound, out});
        } else if (pick < 0) {
            result = 0x1D;
        } else if (priority < least || (priority == least && F32(distance) >= least_distance)) {
            result = 0x1E;
        } else {
            const uint32_t r = Record(manager, uint32_t(pick));
            ctx.ecx = r;
            kernel::CallGuest(ctx, kStop, {});
            ctx.ecx = r;
            result = kernel::CallGuest(ctx, kStart, {priority, distance, sound, out});
        }
    }
    kernel::Return(ctx, result, 4);
}
