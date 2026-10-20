// The game's own menus lead to the PC settings menu (src/common/menu.cpp). Xbox Live is gone, so its entries
// become PC SETTINGS: XBOX LIVE OPTIONS in the pause menu and XBOX LIVE in the main menu's OPTIONS page. The
// multiplayer menu's XBOX LIVE (the online mode) stays as it is.
//
// Texts: the game keeps its strings once each in a pool, {u16 references, chars, 0}; the string tables map the
// CRC-32 of a text id (upper case, for drawing) to them. A menu entry keeps its element name, the text id, at
// +0x30 as a pool pointer (low two bits are flags). The pool's "Xbox Live Options" (XBL_XboxLiveOptions, also
// the OPTIONS entry's description) is rewritten in place to "PC SETTINGS" (shorter, so it fits).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstring>

#include "core/log.h"
#include "xdk/hle.h"

namespace menu {
void OpenFromGame();
}

namespace game_menu {

namespace {

constexpr char kOld[] = "Xbox Live Options", kNew[] = "PC SETTINGS";
std::atomic<uint32_t> s_text{0};  // guest address of the rewritten pool string ({u16, chars})

// the pool string "\0<u16>Xbox Live Options\0" in the guest's committed memory, at its u16
uint32_t FindText() {
    const size_t n = sizeof(kOld);  // with its 0
    uint8_t* lo = kernel::g_base + 0x10000;
    uint8_t* hi = kernel::g_base + 0x08000000;
    MEMORY_BASIC_INFORMATION mi;
    for (uint8_t* a = lo; a < hi && VirtualQuery(a, &mi, sizeof(mi)); a = static_cast<uint8_t*>(mi.BaseAddress) + mi.RegionSize) {
        if (mi.State != MEM_COMMIT || !(mi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE))) continue;
        uint8_t* b = static_cast<uint8_t*>(mi.BaseAddress);
        uint8_t* e = b + mi.RegionSize;
        for (uint8_t* p = b + 3; p + n <= e; p++) {
            p = static_cast<uint8_t*>(std::memchr(p, 'X', size_t(e - p)));
            if (!p || p + n > e) break;
            if (std::memcmp(p, kOld, n) == 0 && p[-3] == 0) return uint32_t(p - 2 - kernel::g_base);
        }
    }
    return 0;
}

// a menu entry's element name is name (its id string)
bool Named(uint32_t item, const char* name) {
    uint32_t s = kernel::R32(item + 0x30) & ~3u;
    return s && std::strcmp(reinterpret_cast<const char*>(kernel::g_base + s + 2), name) == 0;
}

// the selected entry of a menu element
uint32_t Selected(uint32_t menu) {
    uint32_t items = kernel::R32(menu + 0x5C);
    return items ? kernel::R32(items + kernel::R32(menu + 0x68) * 4) : 0;
}

}  // namespace

// render thread, once a second until done: the string tables load during boot
void Patch() {
    if (uint32_t t = s_text) {
        if (std::memcmp(kernel::g_base + t + 2, kNew, sizeof(kNew)) == 0) return;
        s_text = 0;  // the pool was rebuilt (a language change): look again
    }
    static uint64_t next = 0;
    uint64_t now = GetTickCount64();
    if (now < next) return;
    next = now + 1000;
    if (uint32_t t = FindText()) {
        std::memcpy(kernel::g_base + t + 2, kNew, sizeof(kNew));
        s_text = t;
        LOGR("menu: Xbox Live Options renamed PC SETTINGS (pool string at %08X)", t);
    }
}

}  // namespace game_menu

// the pause menu's select (A) handler, thiscall (this: the page's handler, +4 its menu; 1 arg): it tells the
// entries apart by their element names; XBL_XboxLiveOptions opens the PC settings menu instead of signing in
X86_EXTERN_FUNC(__imp__sub_000F64B3);
X86_FUNC(sub_000F64B3) {
    uint32_t item = game_menu::Selected(kernel::R32(ctx.ecx + 4));
    if (item && kernel::g_base[item + 0x54] && game_menu::Named(item, "XBL_XboxLiveOptions")) {
        menu::OpenFromGame();
        kernel::Return(ctx, 0, 1);
        return;
    }
    __imp__sub_000F64B3(ctx, base);
}

// the front end's OPTIONS page, built when it opens (thiscall, this: the page; its main_options handler at +0xF0,
// whose +4 is the menu). An entry's text element (+0x80) draws the text whose upper-case id has the CRC-32 at its
// +0x48 (the element's +0xC8): the XBOX LIVE entry draws XBL_XBOXLIVEOPTIONS', PC SETTINGS, from here on.
namespace {
std::atomic<uint32_t> s_options_menu{0};  // only compared: the page may be gone
constexpr uint32_t kPcSettingsText = 0x51CE4181;  // CRC-32 of "XBL_XBOXLIVEOPTIONS"
}  // namespace

X86_EXTERN_FUNC(__imp__sub_000EA2F0);
X86_FUNC(sub_000EA2F0) {
    uint32_t page = ctx.ecx;
    __imp__sub_000EA2F0(ctx, base);
    uint32_t handler = kernel::R32(page + 0xF0);
    uint32_t menu = handler ? kernel::R32(handler + 4) : 0;
    uint32_t items = menu ? kernel::R32(menu + 0x5C) : 0, count = menu ? kernel::R32(menu + 0x64) : 0;
    for (uint32_t i = 0; items && i < count && i < 16; i++) {
        uint32_t item = kernel::R32(items + i * 4);
        if (!item || !game_menu::Named(item, "XBL_XboxLive")) continue;
        kernel::W32(item + 0xC8, kPcSettingsText);
        kernel::g_base[item + 0x54] = 1;
        s_options_menu = menu;
        LOGR("menu: OPTIONS entry XBOX LIVE is PC SETTINGS");
    }
}

// a menu's SetEnabled(index, on) (thiscall, 2 args): the OPTIONS page's PC SETTINGS entry stays on (the game
// turns XBOX LIVE off without a sign-in)
X86_EXTERN_FUNC(__imp__sub_001EF3ED);
X86_FUNC(sub_001EF3ED) {
    uint32_t menu = ctx.ecx;
    if (menu && menu == s_options_menu) {
        uint32_t item = kernel::R32(kernel::R32(menu + 0x5C) + kernel::R32(ctx.esp + 4) * 4);
        if (item && game_menu::Named(item, "XBL_XboxLive")) kernel::W32(ctx.esp + 8, 1);
    }
    __imp__sub_001EF3ED(ctx, base);
}

// the front end's main_options select (A) handler, thiscall (this: the handler, +4 its menu; 1 arg)
X86_EXTERN_FUNC(__imp__sub_000F56F3);
X86_FUNC(sub_000F56F3) {
    uint32_t item = game_menu::Selected(kernel::R32(ctx.ecx + 4));
    if (item && game_menu::Named(item, "XBL_XboxLive")) {
        menu::OpenFromGame();
        kernel::Return(ctx, 0, 1);
        return;
    }
    __imp__sub_000F56F3(ctx, base);
}