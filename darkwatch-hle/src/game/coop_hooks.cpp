// Co-op: the PS2 version's two-player split-screen campaign is still in the Xbox build, behind
// the main menu. Its MULTIPLAYER entry has two links, XboxOnly multiplayer_main and Ps2Only
// multiplayer_profile_select_ps2; the second one opens a VERSUS / CO-OP choice (sub_000EA939), and CO-OP leads to the
// two-player coop_profile screen, then chapter_select. In co-op the level setup (sub_000CC4C4) loads each chunk's
// _resources_CoOp list, which the disc has, and its _StreamFile_CoOp.str, which it has not: both suffixes are turned
// into the single-player ones. Player 1 is port 0 (keyboard and mouse too), player 2 port 1; the chapter screen
// asks for controllers in ports 3 and 4 as well, which co-op games report plugged in (TestPad).
#include <atomic>
#include <cstdio>
#include <cstring>

#include "core/input.h"
#include "core/log.h"
#include "xdk/hle.h"

namespace {

constexpr uint32_t kIntern = 0x000138FA;       // StringId::StringId(const char*), thiscall, ret 4
constexpr uint32_t kPs2Link = 0x004298F8;      // "multiplayer_profile_select_ps2"
constexpr uint32_t kCoopStreamName = 0x004257C8;  // "_StreamFile_CoOp.str"
constexpr uint32_t kCoopResourcesName = 0x004257E0;  // "_resources_CoOp"

// CO-OP was picked in the mode popup, and no main menu entry since (VERSUS or SINGLE PLAYER turn it off)
std::atomic<bool> s_coop{false};

const char* PoolText(uint32_t id) {
    id &= ~3u;  // the low two bits are flags
    return id ? reinterpret_cast<const char*>(kernel::g_base + id + 2) : "";
}

}  // namespace

namespace coop {

// a main menu pick (from the main menu's select handler, sub_000EA9B7 in menu_hooks.cpp, before the game's own):
// the MULTIPLAYER entry takes the PS2 link, so the game asks VERSUS or CO-OP
void MainPick(X86Context& ctx, uint32_t item) {
    s_coop = false;  // a new main menu pick; CO-OP turns it on again
    if (item && std::strcmp(PoolText(kernel::R32(item + 0x50)), "multiplayer_main") == 0) {
        uint32_t eax = ctx.eax, ebx = ctx.ebx, ecx = ctx.ecx, edx = ctx.edx, esi = ctx.esi, edi = ctx.edi,
                 ebp = ctx.ebp, esp = ctx.esp;
        ctx.ecx = item + 0x50;  // the link's id is made again from the PS2 one's text (the old one's count leaks)
        kernel::CallGuest(ctx, kIntern, {kPs2Link});
        ctx.eax = eax, ctx.ebx = ebx, ctx.ecx = ecx, ctx.edx = edx, ctx.esi = esi, ctx.edi = edi, ctx.ebp = ebp;
        ctx.esp = esp;
        LOGR("coop: MULTIPLAYER takes the PS2 link (%s)", PoolText(kernel::R32(item + 0x50)));
    }
}

}  // namespace coop

// level setup: co-op names the chunks' stream files <chunk>_StreamFile_CoOp.str, which the disc does not have
X86_EXTERN_FUNC(__imp__sub_000CC4C4);
X86_FUNC(sub_000CC4C4) {
    char* name = reinterpret_cast<char*>(kernel::g_base + kCoopStreamName);
    if (std::strcmp(name, "_StreamFile_CoOp.str") == 0) {
        std::memcpy(name, "_StreamFile.str", sizeof("_StreamFile.str"));
        LOGR("coop: co-op levels stream the normal _StreamFile.str");
    }
    // the co-op resource lists name what only the missing co-op stream files hold: the level never leaves LOADING
    // (WrongTrain). The single-player lists load, and co-op runs on them; XBE_COOP_RESOURCES=coop keeps the co-op
    // lists, for finding out what they need
    char* res = reinterpret_cast<char*>(kernel::g_base + kCoopResourcesName);
    const char* keep = std::getenv("XBE_COOP_RESOURCES");
    if (!(keep && std::strcmp(keep, "coop") == 0) && std::strcmp(res, "_resources_CoOp") == 0) {
        std::memcpy(res, "_resources", sizeof("_resources"));
        LOGR("coop: co-op levels load the normal _resources lists");
    }
    __imp__sub_000CC4C4(ctx, base);
}

// the mode popup's handler (thiscall, 2 args: the choice, 0 VERSUS and 1 CO-OP, then the popup)
X86_EXTERN_FUNC(__imp__sub_000EA939);
X86_FUNC(sub_000EA939) {
    uint32_t choice = kernel::R32(ctx.esp + 4);
    s_coop = choice == 1;
    LOGR("coop: mode %s", choice == 1 ? "co-op, ports 3 and 4 report plugged in" : "versus, real controllers");
    __imp__sub_000EA939(ctx, base);
}

namespace coop {

// every polled pad: in co-op the front end wants a controller in all four ports (the PS2 multitap's), and
// sub_000EFA10 asks for ports 3 and 4 to be reconnected; from the CO-OP pick on they are reported plugged in, with
// nothing held (versus sees the real controllers, for its own up-to-four-player split screen). Then the
// test pads: with XBE_COOP_P2 naming a folder, port<n>.txt there (n 1-3) makes port n a plugged-in controller
// holding what the file names (" a b x y start back up down left right lt rt lx=-1 rx=1 ..."), read on each poll,
// so a script can play the other players
void TestPad(int port, input::Pad& pad) {
    if (port >= 2 && !pad.connected && s_coop) pad.connected = true;
    static const char* dir = std::getenv("XBE_COOP_P2");
    if (port < 1 || port > 3 || !dir) return;
    char text[256] = {}, file[512];
    std::snprintf(file, sizeof(file), "%s\\port%d.txt", dir, port);
    FILE* f = std::fopen(file, "r");
    if (!f) return;
    size_t n = std::fread(text, 1, sizeof(text) - 1, f);
    text[n] = 0;
    std::fclose(f);
    pad = input::Pad{};
    pad.connected = true;
    auto has = [&](const char* w) { return std::strstr(text, w) != nullptr; };
    const char* analog[] = {"a", "b", "x", "y", "black", "white", "lt", "rt"};
    for (int i = 0; i < 8; i++) {
        char w[16];
        std::snprintf(w, sizeof(w), " %s ", analog[i]);
        if (has(w)) pad.analog[i] = 0xFF;
    }
    const char* buttons[] = {" up ", " down ", " left ", " right ", " start ", " back "};
    for (int i = 0; i < 6; i++)
        if (has(buttons[i])) pad.buttons |= uint16_t(1u << i);
    if (has(" lx=-1 ")) pad.lx = -32768;
    if (has(" lx=1 ")) pad.lx = 32767;
    if (has(" ly=-1 ")) pad.ly = -32768;
    if (has(" ly=1 ")) pad.ly = 32767;
    if (has(" rx=-1 ")) pad.rx = -32768;
    if (has(" rx=1 ")) pad.rx = 32767;
    if (has(" ry=-1 ")) pad.ry = -32768;
    if (has(" ry=1 ")) pad.ry = 32767;
}

}  // namespace coop
