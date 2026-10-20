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

#include "app/app.h"
#include "core/log.h"
#include "xdk/hle.h"

namespace menu {
void OpenFromGame();
}

namespace coop {
void MainPick(X86Context& ctx, uint32_t item);
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

// QUIT TO DESKTOP, a sixth entry under the main menu's five. The main page builds its "menu" element from the
// front end's data (sub_000EA7F6, thiscall on the page, which keeps the menu at +0x58); after that a row is made as
// the game's own pages make theirs in code (sub_000E6435): a MenuItem (0x68 bytes, sub_001EFB52 with its element
// name, its link and one more id, each copied: name +0x30, link +0x50) whose text element (+0x58) draws the name,
// then the menu's AddItem (sub_001EFA66). A name starting with ~ is drawn as it is written, not looked up as a text id. The row's text takes
// the scaled-font mode and scale of the rows above it. A menu keeps its rows' count (u16) at +0x58 and the rows at
// +0x5C, the selected row's index at +0x68.
namespace {
constexpr char kQuitName[] = "~QUIT TO DESKTOP", kQuitLink[] = "quit_to_desktop";
constexpr uint32_t kIntern = 0x000138FA;   // StringId(const char*), thiscall on the id, ret 4
constexpr uint32_t kRelease = 0x00011C1A;  // ~StringId, thiscall
constexpr uint32_t kAlloc = 0x0024C546;    // operator new, cdecl

bool Linked(uint32_t item, const char* link) {
    uint32_t s = kernel::R32(item + 0x50) & ~3u;
    return s && std::strcmp(reinterpret_cast<const char*>(kernel::g_base + s + 2), link) == 0;
}

void AddQuitRow(X86Context& ctx, uint32_t menu) {
    uint32_t count = kernel::R16(menu + 0x58), rows = kernel::R32(menu + 0x5C);
    if (!rows || count < 1 || count > 8) return;
    uint32_t first = kernel::R32(rows);
    if (!first || !game_menu::Named(first, "GEN_SinglePlayer")) return;  // the title screen's main menu only
    for (uint32_t i = 0; i < count; i++)
        if (Linked(kernel::R32(rows + i * 4), kQuitLink)) return;
    uint32_t esp = ctx.esp;
    ctx.esp -= 0x80;
    uint32_t at = ctx.esp;  // ids: name, an empty one, link; then the two texts
    std::memset(kernel::g_base + at, 0, 0x80);
    std::memcpy(kernel::g_base + at + 0x20, kQuitName, sizeof(kQuitName));
    std::memcpy(kernel::g_base + at + 0x40, kQuitLink, sizeof(kQuitLink));
    ctx.ecx = at;
    kernel::CallGuest(ctx, kIntern, {at + 0x20});
    ctx.ecx = at + 8;
    kernel::CallGuest(ctx, kIntern, {at + 0x40});
    uint32_t row = kernel::CallGuest(ctx, kAlloc, {0x68});
    if (row) {
        ctx.ecx = row;
        row = kernel::CallGuest(ctx, 0x001EFB52, {at, at + 8, at + 4});  // name, link, an empty id
    }
    if (row) {
        // the text's look as the rows above have it: +0x64 the font mode, +0x6C the font scale, +0x70/+0x74 more of its
        // layout. A text made in code is 0x80 bytes (the front end's own are larger): nothing past that is written.
        uint32_t text = kernel::R32(row + 0x58), peer = kernel::R32(kernel::R32(rows) + 0x58);
        if (text && peer)
            for (uint32_t o : {0x64u, 0x6Cu, 0x70u, 0x74u}) kernel::W32(text + o, kernel::R32(peer + o));
        ctx.ecx = menu;
        kernel::CallGuest(ctx, 0x001EFA66, {row});
        // the menu's box (+0x10 x, y, width, height) spaces its rows down from the top: one row taller, upward, so the
        // last one stays above the footer, and wide enough for QUIT TO DESKTOP on one line
        float box[4];
        std::memcpy(box, kernel::g_base + menu + 0x10, sizeof(box));
        float grow = box[3] / float(count);
        box[1] += grow;
        box[3] += grow;
        box[2] *= 1.25f;
        std::memcpy(kernel::g_base + menu + 0x10, box, sizeof(box));
        LOGR("menu: main menu has QUIT TO DESKTOP");
    }
    for (uint32_t id : {at, at + 4, at + 8}) {
        ctx.ecx = id;
        kernel::CallGuest(ctx, kRelease, {});
    }
    ctx.esp = esp;
}
}  // namespace

X86_EXTERN_FUNC(__imp__sub_000EA7F6);
X86_FUNC(sub_000EA7F6) {
    uint32_t page = ctx.ecx;
    __imp__sub_000EA7F6(ctx, base);
    if (uint32_t menu = kernel::R32(page + 0x58)) {
        uint32_t eax = ctx.eax, ecx = ctx.ecx;
        AddQuitRow(ctx, menu);
        ctx.eax = eax;
        ctx.ecx = ecx;
    }
}

// the main menu's select handler (thiscall on the page, 1 arg): QUIT TO DESKTOP closes the window, which shuts the
// game down as its close button does
X86_EXTERN_FUNC(__imp__sub_000EA9B7);
X86_FUNC(sub_000EA9B7) {
    uint32_t menu = kernel::R32(ctx.ecx + 0x58);
    uint32_t item = menu ? game_menu::Selected(menu) : 0;
    coop::MainPick(ctx, item);  // MULTIPLAYER asks VERSUS or CO-OP: src/game/coop_hooks.cpp
    if (item && Linked(item, kQuitLink)) {
        LOGR("menu: quit to desktop");
        PostMessageW(static_cast<HWND>(app::Window()), WM_CLOSE, 0, 0);
        kernel::Return(ctx, 0, 1);
        return;
    }
    __imp__sub_000EA9B7(ctx, base);
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