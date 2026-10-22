// PC SETTINGS as the game's own page. The Xbox Live options page (front end OPTIONS and pause menu) is renamed by
// menu_hooks.cpp and already opens like any other game page; this file replaces its rows with our settings and
// drives them with the game's own widgets, sounds, A / B and mouse rules.
//
// The page has a small top page whose entries open sub-pages in place (the game's row widget has no page objects for
// us to make safely): DISPLAY AND GRAPHICS, CONTROLS, GAME, APPLY CHANGES and REPORT A PROBLEM. Rows are the game's
// MenuItem (0x43C8F8), BoolWidget (0x43C9B0) and IntegerWidget (0x43C97C) made with the same constructors the game's
// data pages use. The page handler is the game's 0xF6A6D for the Xbox Live page; its update, select (A) and back (B)
// virtuals are taken over while our rows are on the page.
//
// Values are ours alone: app::Get() and the menu:: globals (settings.toml). The game's own audio/controls options are
// untouched. Every change applies at once, as the F1 menu's do; the display mode and resolution wait for APPLY
// CHANGES, which keeps / goes back like F1. APPLY and the save prompt write the file, and DISCARD restores the values
// read when the page opened.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "app/app.h"
#include "core/log.h"
#include "xdk/hle.h"

namespace menu {
extern bool g_show_fps, g_ps2_alpha;
extern uint32_t g_fov, g_weapon_fov, g_reticle;
extern bool g_widescreen, g_mouse_direct, g_cutscene_borders;
void OpenFromGame();   // src/common/menu.cpp: the F1 overlay
void OpenControls();   // the same, opened on its Controls page (key capture)
const std::vector<app::Mode>& Modes();  // the monitor's resolutions, largest first
}  // namespace menu

namespace content {
extern bool g_bonus;
}

namespace prompts {
extern uint32_t g_style;
}

namespace mouse_menus {
extern bool g_on;
}

namespace report {
void Open();  // src/common/report.cpp: the F4 box
}

namespace settings_menu {

X86_EXTERN_FUNC(__imp__sub_000F6B14);
X86_EXTERN_FUNC(__imp__sub_000F5351);

namespace {

// game objects and calls, all proved in tasks/task-019-notes.md
constexpr uint32_t kMenuVtable = 0x43C8B8;   // Menu
constexpr uint32_t kAlloc = 0x0024C546;      // operator new, cdecl
constexpr uint32_t kIntern = 0x000138FA;     // StringId(const char*), thiscall on the id, ret 4
constexpr uint32_t kRelease = 0x00011C1A;    // ~StringId, thiscall

constexpr uint32_t kMenuItemCtor = 0x001EFB52;
constexpr uint32_t kReserve = 0x000166F7;    // grow a menu container (rows or children)
constexpr uint32_t kBoolCtor = 0x001F04A5;
constexpr uint32_t kBoolSet = 0x001F0396;
constexpr uint32_t kBoolInput = 0x001F045A;
constexpr uint32_t kIntCtor = 0x001F02EC;
constexpr uint32_t kIntSet = 0x001F0210;
constexpr uint32_t kIntInput = 0x001F0292;
constexpr uint32_t kInputTest = 0x001EE6DB;  // (controller bits, direction, pad state, out) -> pressed
constexpr uint32_t kSetText = 0x001E9583;    // text element: use this text id, thiscall, ret 4
constexpr uint32_t kMoveSound = 0x001E917C;
constexpr uint32_t kButtonOn = 0x001EEEC1;   // ButtonBar::Enable(index), thiscall, ret 4
constexpr uint32_t kButtonOff = 0x001EEEF2;  // ButtonBar::Disable(index), thiscall, ret 4
constexpr uint32_t kOriginalBack = 0x000F68E3;  // through our hook, which lets it pass once s_live is clear

// menu fields (mouse_menus.cpp has the same list)
constexpr uint32_t kMenuCount = 0x58, kMenuRows = 0x5C, kMenuFirst = 0x60, kMenuShown = 0x64, kMenuPicked = 0x68;
constexpr uint32_t kRowOn = 0x54, kRowText = 0x58, kRowInput = 0x24;
constexpr uint32_t kBoolState = 0x68;
constexpr uint32_t kIntValue = 0x64;

enum Page { kPageTop, kPageDisplay, kPageControls, kPageGame, kPageSave, kPageKeep, kPageCount };

// our page rows
enum Opt {
    kDisplayMode, kResolution, kRender, kFps, kVsync,
    kPs2, kAniso, kMsaa, kPrecompile, kFov, kWeaponFov, kWide, kReticle, kBorders,
    kPrompts, kKeyboard, kMouseLook, kMouseSens, kMouseDirect, kMouseMenus,
    kShowFps, kBonus,
    kOpenDisplay, kOpenControls, kOpenGame, kKeyBindings, kReport, kApply, kBack,
    kSaveChanges, kDiscardChanges, kKeepMode, kGoBackMode,
    kOptCount,
    kHeader = -1,
};

enum Kind { kBool, kInt, kChoice, kAction };

struct Def {
    Kind kind;
    const char* label;
    int lo, hi;
    const char* const* names;
    int name_count;
};

const char* const kModes[] = {"FULLSCREEN", "BORDERLESS", "WINDOWED"};
const char* const kCaps[] = {"30", "60", "120", "144", "165", "240", "UNLIMITED"};
const uint32_t kCapValues[] = {30, 60, 120, 144, 165, 240, 0};
const char* const kAnisoNames[] = {"AS THE GAME ASKS", "2X", "4X", "8X", "16X"};
const uint32_t kAnisoValues[] = {0, 2, 4, 8, 16};
const char* const kMsaaNames[] = {"OFF", "2X", "4X", "8X"};
const uint32_t kMsaaValues[] = {0, 2, 4, 8};
const char* const kPromptStyles[] = {"CONTROLLER IN USE", "XBOX", "PLAYSTATION", "SWITCH", "KEYBOARD AND MOUSE"};

const Def kDefs[kOptCount] = {
    {kChoice, "DISPLAY MODE", 0, 2, kModes, 3},
    {kChoice, "RESOLUTION", 0, 0, nullptr, 0},
    {kInt, "RENDER RESOLUTION", 25, 200, nullptr, 0},
    {kChoice, "MAX FRAME RATE", 0, 6, kCaps, 7},
    {kBool, "V-SYNC", 0, 0, nullptr, 0},

    {kBool, "PS2 ALPHA BLENDING", 0, 0, nullptr, 0},
    {kChoice, "ANISOTROPIC FILTERING", 0, 4, kAnisoNames, 5},
    {kChoice, "ANTI-ALIASING (MSAA)", 0, 3, kMsaaNames, 4},
    {kBool, "PRECOMPILE SHADERS", 0, 0, nullptr, 0},
    {kInt, "FIELD OF VIEW", 39, 110, nullptr, 0},
    {kInt, "WEAPON FIELD OF VIEW", 39, 110, nullptr, 0},
    {kBool, "WIDESCREEN FRAMING", 0, 0, nullptr, 0},
    {kInt, "RETICLE SIZE", 50, 200, nullptr, 0},
    {kBool, "CUT-SCENE BORDERS", 0, 0, nullptr, 0},

    {kChoice, "BUTTON PROMPTS", 0, 4, kPromptStyles, 5},
    {kBool, "KEYBOARD AND MOUSE", 0, 0, nullptr, 0},
    {kBool, "MOUSE LOOK", 0, 0, nullptr, 0},
    {kInt, "MOUSE SENSITIVITY", 10, 400, nullptr, 0},
    {kBool, "DIRECT MOUSE AIMING", 0, 0, nullptr, 0},
    {kBool, "MOUSE IN MENUS", 0, 0, nullptr, 0},

    {kBool, "SHOW FRAME RATE", 0, 0, nullptr, 0},
    {kBool, "WAR TRAIN AND SPLITTER", 0, 0, nullptr, 0},

    {kAction, "DISPLAY AND GRAPHICS", 0, 0, nullptr, 0},
    {kAction, "CONTROLS", 0, 0, nullptr, 0},
    {kAction, "GAME", 0, 0, nullptr, 0},
    {kAction, "KEY BINDINGS...", 0, 0, nullptr, 0},
    {kAction, "REPORT A PROBLEM", 0, 0, nullptr, 0},
    {kAction, "APPLY CHANGES", 0, 0, nullptr, 0},
    {kAction, "BACK", 0, 0, nullptr, 0},
    {kAction, "SAVE CHANGES", 0, 0, nullptr, 0},
    {kAction, "DISCARD CHANGES", 0, 0, nullptr, 0},
    {kAction, "KEEP", 0, 0, nullptr, 0},
    {kAction, "GO BACK", 0, 0, nullptr, 0},
};

struct Values {
    uint32_t v[kOptCount];
};

struct RowRef {
    int opt;
    uint32_t row;
};

std::atomic<uint32_t> s_handler{0};  // the Xbox Live page handler we took over
std::atomic<uint32_t> s_menu{0};
bool s_ready = false;   // rows built, original rows saved
bool s_live = false;    // the page is open
bool s_dirty = false;   // a value changed since the page opened (or the last Apply)
bool s_pending = false; // the page was asked for: snapshot it on the next update
Page s_page = kPageTop;

uint32_t s_orig_count = 0, s_orig_first = 0, s_orig_shown = 0, s_orig_picked = 0;
std::vector<uint32_t> s_orig_rows;
std::vector<uint8_t> s_orig_on;
std::vector<RowRef> s_pages[kPageCount];
std::vector<uint32_t> s_choice_rows[kOptCount];
uint32_t s_choice_idx[kOptCount] = {};
Values s_snapshot{};

// display mode and resolution wait for APPLY CHANGES
app::DisplayMode s_pending_mode = app::DisplayMode::Borderless;
uint32_t s_pending_w = 0, s_pending_h = 0;
std::vector<std::string> s_res_names;

// the keep / go back page
bool s_keep = false;
bool s_leave_after_keep = false;
app::DisplayMode s_revert_mode = app::DisplayMode::Borderless;
uint32_t s_revert_w = 0, s_revert_h = 0;
uint64_t s_revert_at = 0;
long s_revert_left = -1;

// the page heading and the description line (pool strings the page data draws)
uint32_t s_pool_title = 0, s_pool_desc = 0;
char s_pool_title_orig[16] = {};
char s_pool_desc_orig[40] = {};
int s_help_opt = -999;

// fixed guest memory for our texts and ids: allocated once with the game's own allocator
uint32_t s_strings = 0, s_strings_at = 0;
uint32_t s_ids = 0, s_ids_at = 0;
uint32_t s_choice_text[kOptCount][64] = {};  // "~VALUE" text ids for the choice rows

bool ReadyForText() { return s_strings && s_ids; }

uint32_t GuestText(X86Context& ctx, const char* text) {
    const size_t n = std::strlen(text) + 1;
    if (!s_strings || s_strings_at + n > 0x4000) return 0;
    uint32_t at = s_strings + s_strings_at;
    std::memcpy(kernel::g_base + at, text, n);
    s_strings_at += uint32_t(n + 3) & ~3u;
    (void)ctx;
    return at;
}

uint32_t Intern(X86Context& ctx, const char* text) {
    if (!s_ids || s_ids_at + 4 > 0x1000) return 0;
    uint32_t at = s_ids + s_ids_at;
    s_ids_at += 4;
    uint32_t chars = GuestText(ctx, text);
    if (!chars) return 0;
    ctx.ecx = at;
    kernel::CallGuest(ctx, kIntern, {chars});
    return at;
}

void Write8(uint32_t at, uint8_t value) { kernel::g_base[at] = value; }

uint32_t Alloc(X86Context& ctx, uint32_t size) {
    uint32_t at = kernel::CallGuest(ctx, kAlloc, {size});
    if (at) std::memset(kernel::g_base + at, 0, size);
    return at;
}

uint32_t Selected(uint32_t menu) {
    uint32_t rows = kernel::R32(menu + kMenuRows);
    return rows ? kernel::R32(rows + kernel::R32(menu + kMenuPicked) * 4) : 0;
}

bool RowIn(Page page, uint32_t row, int& opt) {
    for (const RowRef& r : s_pages[page])
        if (r.row == row) {
            opt = r.opt;
            return true;
        }
    return false;
}

uint32_t CapIndex(uint32_t raw) {
    for (int i = 0; i < 7; i++)
        if (kCapValues[i] == raw) return uint32_t(i);
    return 1;
}

uint32_t ResIndex(uint32_t w, uint32_t h) {
    if (!w || !h) return 0;
    const std::vector<app::Mode>& modes = menu::Modes();
    for (size_t i = 0; i < modes.size(); i++)
        if (modes[i].width == w && modes[i].height == h) return uint32_t(i + 1);
    return 0;
}

app::Mode ResMode(uint32_t index) {
    const std::vector<app::Mode>& modes = menu::Modes();
    if (index && index <= modes.size()) return modes[index - 1];
    return app::Mode{0, 0};
}

uint32_t ChoiceCount(int opt) {
    if (opt == kResolution) return 1 + uint32_t(menu::Modes().size());
    return uint32_t(kDefs[opt].name_count);
}

const char* ChoiceName(int opt, uint32_t index) {
    if (opt == kResolution)
        return index < s_res_names.size() ? s_res_names[index].c_str() : "DESKTOP";
    const Def& d = kDefs[opt];
    return index < uint32_t(d.name_count) ? d.names[index] : "";
}

uint32_t ChoiceIndex(int opt, uint32_t raw) {
    switch (opt) {
        case kDisplayMode: return raw < 3 ? raw : 1;
        case kResolution: return raw;
        case kFps: return CapIndex(raw);
        case kAniso:
            for (int i = 0; i < 5; i++)
                if (kAnisoValues[i] == raw) return uint32_t(i);
            return 0;
        case kMsaa:
            for (int i = 0; i < 4; i++)
                if (kMsaaValues[i] == raw) return uint32_t(i);
            return 0;
        default: return raw < uint32_t(kDefs[opt].name_count) ? raw : 0;
    }
}

uint32_t ChoiceValue(int opt, uint32_t index) {
    switch (opt) {
        case kFps: return kCapValues[std::min<uint32_t>(index, 6)];
        case kAniso: return kAnisoValues[std::min<uint32_t>(index, 4)];
        case kMsaa: return kMsaaValues[std::min<uint32_t>(index, 3)];
        default: return index;
    }
}

Values ReadValues() {
    Values v{};
    const app::Settings& s = app::Get();
    v.v[kDisplayMode] = uint32_t(s_pending_mode);
    v.v[kResolution] = ResIndex(s_pending_w, s_pending_h);
    v.v[kRender] = s.render_percent;
    v.v[kFps] = s.fps_cap;
    v.v[kVsync] = s.vsync;
    v.v[kPs2] = menu::g_ps2_alpha;
    v.v[kAniso] = s.aniso;
    v.v[kMsaa] = s.msaa;
    v.v[kPrecompile] = s.precompile_shaders;
    v.v[kFov] = menu::g_fov ? menu::g_fov : 39;
    v.v[kWeaponFov] = menu::g_weapon_fov ? menu::g_weapon_fov : 39;
    v.v[kWide] = menu::g_widescreen;
    v.v[kReticle] = menu::g_reticle;
    v.v[kBorders] = menu::g_cutscene_borders;
    v.v[kPrompts] = std::min<uint32_t>(prompts::g_style, 4);
    v.v[kKeyboard] = s.keyboard;
    v.v[kMouseLook] = s.mouse_look;
    v.v[kMouseSens] = s.mouse_sensitivity;
    v.v[kMouseDirect] = menu::g_mouse_direct;
    v.v[kMouseMenus] = mouse_menus::g_on;
    v.v[kShowFps] = menu::g_show_fps;
    v.v[kBonus] = content::g_bonus;
    return v;
}

void WriteValues(const Values& v) {
    app::Settings& s = app::Get();
    const app::Mode mode = ResMode(v.v[kResolution]);
    s.display_mode = app::DisplayMode(std::min<uint32_t>(v.v[kDisplayMode], 2));
    s.width = mode.width;
    s.height = mode.height;
    s.render_percent = std::clamp<uint32_t>(v.v[kRender], 25, 200);
    s.fps_cap = v.v[kFps];
    s.vsync = v.v[kVsync] != 0;
    menu::g_ps2_alpha = v.v[kPs2] != 0;
    s.aniso = v.v[kAniso];
    s.msaa = v.v[kMsaa];
    s.precompile_shaders = v.v[kPrecompile] != 0;
    menu::g_fov = v.v[kFov] <= 39 ? 0 : std::min<uint32_t>(v.v[kFov], 110);
    menu::g_weapon_fov = v.v[kWeaponFov] <= 39 ? 0 : std::min<uint32_t>(v.v[kWeaponFov], 110);
    menu::g_widescreen = v.v[kWide] != 0;
    menu::g_reticle = std::clamp<uint32_t>(v.v[kReticle], 50, 200);
    menu::g_cutscene_borders = v.v[kBorders] != 0;
    prompts::g_style = std::min<uint32_t>(v.v[kPrompts], 4);
    s.keyboard = v.v[kKeyboard] != 0;
    s.mouse_look = v.v[kMouseLook] != 0;
    s.mouse_sensitivity = std::clamp<uint32_t>(v.v[kMouseSens], 10, 400);
    menu::g_mouse_direct = v.v[kMouseDirect] != 0;
    mouse_menus::g_on = v.v[kMouseMenus] != 0;
    menu::g_show_fps = v.v[kShowFps] != 0;
    content::g_bonus = v.v[kBonus] != 0;
}

uint32_t RowValue(int opt) {
    switch (kDefs[opt].kind) {
        case kBool: {
            for (const RowRef& r : s_pages[s_page])
                if (r.opt == opt) return kernel::g_base[r.row + kBoolState];
            return 0;
        }
        case kInt: {
            for (const RowRef& r : s_pages[s_page])
                if (r.opt == opt) return kernel::R32(r.row + kIntValue);
            return 0;
        }
        default: return ChoiceIndex(opt, ReadValues().v[opt]);
    }
}

// The menu keeps its rows in two places: the list it draws and steers (count +0x58, array +0x5C) and its child
// list (count +0x08, array +0x0C), which is what draws the elements. ShowList writes both to the same rows, so a
// row that is not on the page is not drawn, and the game's old Xbox Live rows stop drawing as well.
void ShowList(X86Context& ctx, const std::vector<uint32_t>& rows, uint32_t first, uint32_t picked, uint32_t shown) {
    uint32_t menu = s_menu;
    if (!menu) return;

    ctx.ecx = menu + kMenuCount;
    kernel::CallGuest(ctx, kReserve, {uint32_t(rows.size() + 8)});
    uint32_t array = kernel::R32(menu + kMenuRows);
    for (size_t i = 0; array && i < rows.size(); i++) kernel::W32(array + uint32_t(i) * 4, rows[i]);
    kernel::W16(menu + kMenuCount, uint16_t(rows.size()));

    ctx.ecx = menu + 0x08;
    kernel::CallGuest(ctx, kReserve, {uint32_t(rows.size() + 8)});
    uint32_t children = kernel::R32(menu + 0x0C);
    for (size_t i = 0; children && i < rows.size(); i++) {
        kernel::W32(children + uint32_t(i) * 4, rows[i]);
        kernel::W32(rows[i] + 4, menu);
        Write8(rows[i] + kRowOn, 1);
    }
    kernel::W16(menu + 0x08, uint16_t(rows.size()));

    kernel::W32(menu + kMenuFirst, first);
    kernel::W32(menu + kMenuPicked, picked);
    kernel::W32(menu + kMenuShown, shown);
}

void CopyFont(uint32_t row, uint32_t peer) {
    uint32_t text = kernel::R32(row + kRowText), p = peer ? kernel::R32(peer + kRowText) : 0;
    if (!text || !p) return;
    for (uint32_t o : {0x64u, 0x6Cu, 0x70u, 0x74u}) kernel::W32(text + o, kernel::R32(p + o));
}

void CopyInput(uint32_t row) {
    // the game's widgets read the controller bit field at +0x24 before they take a direction; the rows of a page
    // share the menu's field, and a row made in code starts at zero
    if (s_menu) kernel::W32(row + kRowInput, kernel::R32(s_menu + kRowInput));
}

uint32_t MenuItem(X86Context& ctx, uint32_t name, uint32_t link) {
    uint32_t row = Alloc(ctx, 0x68);
    if (!row) return 0;
    uint32_t empty = Intern(ctx, "");
    ctx.ecx = row;
    kernel::CallGuest(ctx, kMenuItemCtor, {name, link, empty});
    CopyInput(row);
    return row;
}

// a choice row is a BoolWidget with both strings set to the chosen value: the game's own < > arrows and text
void UpdateChoiceText(X86Context& ctx, int opt, uint32_t index) {
    if (s_choice_rows[opt].empty()) return;
    uint32_t row = s_choice_rows[opt][0];
    uint32_t chars = s_choice_text[opt][std::min<uint32_t>(index, 63)];
    if (!chars) return;
    // +0x80 and +0x88 are StringId objects (the ctor makes them from GEN_Yes/GEN_No): rebuild both from our
    // chosen value, then copy one into the text element's StringId (+0x48) as BoolSet does
    for (uint32_t at : {row + 0x80, row + 0x88}) {
        ctx.ecx = at;
        kernel::CallGuest(ctx, kIntern, {chars});
    }
    uint32_t text = kernel::R32(row + kRowText);
    if (text) {
        ctx.ecx = text + 0x48;
        kernel::CallGuest(ctx, kSetText, {row + 0x80});
    }
    s_choice_idx[opt] = index;
}

void BuildRows(X86Context& ctx) {
    uint32_t menu = s_menu;
    if (!menu) return;
    for (int p = 0; p < kPageCount; p++) s_pages[p].clear();
    uint32_t peer = s_orig_rows.empty() ? 0 : s_orig_rows[0];

    auto header = [&](Page page, const char* text) {
        LOGR("settings page: header %s", text);
        uint32_t row = MenuItem(ctx, Intern(ctx, text), Intern(ctx, "pc_header"));
        if (!row) return;
        Write8(row + kRowOn, 0);
        CopyFont(row, peer);
        s_pages[page].push_back({kHeader, row});
    };

    auto action = [&](Page page, const char* text, const char* link, int opt) {
        LOGR("settings page: action %s", text);
        uint32_t row = MenuItem(ctx, Intern(ctx, text), Intern(ctx, link));
        if (!row) return;
        Write8(row + kRowOn, 0);
        CopyFont(row, peer);
        s_pages[page].push_back({opt, row});
    };

    auto option = [&](Page page, int opt, uint32_t index) {
        const Def& d = kDefs[opt];
        LOGR("settings page: option %d %s", opt, d.label);
        const Values v = ReadValues();
        uint32_t name = Intern(ctx, (std::string("~") + d.label).c_str());
        if (d.kind == kChoice) {
            // a BoolWidget with both strings set to the chosen value draws the game's < value > row
            uint32_t row = Alloc(ctx, 0xA0);
            if (!row) return;
            ctx.ecx = row;
            kernel::CallGuest(ctx, kBoolCtor, {name, 0, 0, 0, 0, 0});
            Write8(row + 0x7C, 0);
            CopyInput(row);
            CopyFont(row, peer);
            s_choice_rows[opt].assign(ChoiceCount(opt), row);
            UpdateChoiceText(ctx, opt, index);
            s_pages[page].push_back({opt, row});
            return;
        }
        if (d.kind == kBool) {
            uint32_t row = Alloc(ctx, 0xA0);
            if (!row) return;
            // BoolWidget takes its callback delegate by value (five dwords after the name); all zero is a null
            // delegate, and the setter only calls it when +0x7C is set, which we keep clear.
            ctx.ecx = row;
            kernel::CallGuest(ctx, kBoolCtor, {name, 0, 0, 0, 0, 0});
            Write8(row + 0x7C, 0);  // no callback: values are written here, not by the game's page
            CopyInput(row);
            CopyFont(row, peer);
            ctx.ecx = row;
            kernel::CallGuest(ctx, kBoolSet, {v.v[opt] != 0, 0});
            s_pages[page].push_back({opt, row});
        } else {
            uint32_t cb = Alloc(ctx, 0x28);
            uint32_t row = Alloc(ctx, 0xA0);
            if (!row) return;
            ctx.ecx = row;
            kernel::CallGuest(ctx, kIntCtor, {uint32_t(d.lo), uint32_t(d.hi), name, cb});
            Write8(row + 0x78, 0);  // no callback
            CopyInput(row);
            // an IntegerWidget draws its own name and value: unlike MenuItem it has no text child, and +0x58 is
            // a byte flag there, so the MenuItem text look above must not be copied onto it
            uint32_t value = std::clamp<int>(int(v.v[opt]), d.lo, d.hi);
            ctx.ecx = row;
            kernel::CallGuest(ctx, kIntSet, {value, 0});
            s_pages[page].push_back({opt, row});
        }
    };

    // top page
    header(kPageTop, "~PC SETTINGS");
    action(kPageTop, "~DISPLAY AND GRAPHICS", "pc_open_display", kOpenDisplay);
    action(kPageTop, "~CONTROLS", "pc_open_controls", kOpenControls);
    action(kPageTop, "~GAME", "pc_open_game", kOpenGame);
    action(kPageTop, "~APPLY CHANGES", "pc_apply", kApply);
    action(kPageTop, "~REPORT A PROBLEM", "pc_report", kReport);
    action(kPageTop, "~BACK", "pc_back", kBack);

    // display and graphics
    header(kPageDisplay, "~DISPLAY AND GRAPHICS");
    option(kPageDisplay, kDisplayMode, ChoiceIndex(kDisplayMode, ReadValues().v[kDisplayMode]));
    option(kPageDisplay, kResolution, ChoiceIndex(kResolution, ReadValues().v[kResolution]));
    option(kPageDisplay, kRender, 0);
    option(kPageDisplay, kFps, ChoiceIndex(kFps, ReadValues().v[kFps]));
    option(kPageDisplay, kVsync, 0);
    option(kPageDisplay, kPs2, 0);
    option(kPageDisplay, kAniso, ChoiceIndex(kAniso, ReadValues().v[kAniso]));
    option(kPageDisplay, kMsaa, ChoiceIndex(kMsaa, ReadValues().v[kMsaa]));
    option(kPageDisplay, kPrecompile, 0);
    option(kPageDisplay, kFov, 0);
    option(kPageDisplay, kWeaponFov, 0);
    option(kPageDisplay, kWide, 0);
    option(kPageDisplay, kReticle, 0);
    option(kPageDisplay, kBorders, 0);
    action(kPageDisplay, "~BACK", "pc_back", kBack);

    // controls
    header(kPageControls, "~CONTROLS");
    option(kPageControls, kPrompts, ChoiceIndex(kPrompts, ReadValues().v[kPrompts]));
    option(kPageControls, kKeyboard, 0);
    option(kPageControls, kMouseLook, 0);
    option(kPageControls, kMouseSens, 0);
    option(kPageControls, kMouseDirect, 0);
    option(kPageControls, kMouseMenus, 0);
    action(kPageControls, "~KEY BINDINGS...", "pc_keys", kKeyBindings);
    action(kPageControls, "~BACK", "pc_back", kBack);

    // game
    header(kPageGame, "~GAME");
    option(kPageGame, kShowFps, 0);
    option(kPageGame, kBonus, 0);
    action(kPageGame, "~BACK", "pc_back", kBack);

    // the small save / discard and keep / go back pages
    header(kPageSave, "~UNSAVED CHANGES");
    action(kPageSave, "~SAVE CHANGES", "pc_save", kSaveChanges);
    action(kPageSave, "~DISCARD CHANGES", "pc_discard", kDiscardChanges);

    header(kPageKeep, "~KEEP THIS MODE?");
    action(kPageKeep, "~KEEP", "pc_keep", kKeepMode);
    action(kPageKeep, "~GO BACK", "pc_goback", kGoBackMode);
}

void SetDescription(const char* text) {
    if (!s_pool_desc) return;
    size_t n = std::strlen(text);
    if (n > 36) n = 36;
    std::memcpy(kernel::g_base + s_pool_desc + 2, text, n);
    kernel::g_base[s_pool_desc + 2 + n] = 0;
}

const char* HelpFor(int opt) {
    switch (opt) {
        case kOpenDisplay: return "PICTURE SETTINGS";
        case kOpenControls: return "INPUT SETTINGS";
        case kOpenGame: return "GAME SETTINGS";
        case kKeyBindings: return "KEY SETUP IN F1";
        case kReport: return "REPORT A PROBLEM";
        case kApply: return "SAVE THE SETTINGS";
        case kBack: return s_page == kPageTop ? "BACK TO OPTIONS" : "BACK TO TOP";
        case kSaveChanges: return "KEEP THE CHANGES";
        case kDiscardChanges: return "UNDO THE CHANGES";
        case kKeepMode: return "STAY IN THIS MODE";
        case kGoBackMode: return "USE THE OLD MODE";
        case kDisplayMode: return "SCREEN OR WINDOW";
        case kResolution: return "SCREEN RESOLUTION";
        case kRender: return "SHARPER OR FASTER";
        case kFps: return "FRAME RATE CAP";
        case kVsync: return "LESS SCREEN TEAR";
        case kPs2: return "PS2 ALPHA EFFECT";
        case kAniso: return "SHARPER TEXTURES";
        case kMsaa: return "SMOOTH EDGES";
        case kPrecompile: return "FEWER STUTTERS";
        case kFov: return "VIEW WIDTH";
        case kWeaponFov: return "GUN VIEW WIDTH";
        case kWide: return "WIDER SIDE VIEW";
        case kReticle: return "CROSSHAIR SIZE";
        case kBorders: return "CUT-SCENE BARS";
        case kPrompts: return "BUTTON PICTURES";
        case kKeyboard: return "KEYS AND MOUSE";
        case kMouseLook: return "MOUSE TURNS VIEW";
        case kMouseSens: return "MOUSE SPEED";
        case kMouseDirect: return "MOUSE AIMING";
        case kMouseMenus: return "MOUSE IN MENUS";
        case kShowFps: return "SHOW FRAME RATE";
        case kBonus: return "UNLOCKED CONTENT";
        default: return "";
    }
}

void UpdateHelp() {
    if (s_page == kPageKeep) return;
    uint32_t row = Selected(s_menu);
    int opt = 0;
    if (row && RowIn(s_page, row, opt) && opt != s_help_opt) {
        s_help_opt = opt;
        SetDescription(HelpFor(opt));
    }
}

void ShowPage(X86Context& ctx, Page page, bool reset) {
    uint32_t menu = s_menu;
    if (!menu) return;
    s_page = page;
    const std::vector<RowRef>& refs = s_pages[page];
    std::vector<uint32_t> rows;
    rows.reserve(refs.size());
    for (const RowRef& r : refs) rows.push_back(r.row);

    uint32_t first = reset ? 0 : kernel::R32(menu + kMenuFirst);
    uint32_t picked = reset ? 0 : kernel::R32(menu + kMenuPicked);
    uint32_t shown = reset || !kernel::R32(menu + kMenuShown) ? (s_orig_shown ? s_orig_shown : 6)
                                                               : kernel::R32(menu + kMenuShown);
    if (reset) {
        picked = 0;
        for (size_t i = 0; i < refs.size(); i++)
            if (refs[i].opt != kHeader) {
                picked = uint32_t(i);
                break;
            }
    }
    if (picked >= refs.size()) picked = refs.empty() ? 0 : uint32_t(refs.size() - 1);
    ShowList(ctx, rows, first, picked, shown);
    // headers draw but are not pickable: menu input skips rows with +0x54 clear
    for (const RowRef& r : refs)
        if (r.opt == kHeader) Write8(r.row + kRowOn, 0);
    s_help_opt = -999;
    UpdateHelp();
}

void RestoreOriginal(X86Context& ctx) {
    if (!s_menu) return;
    ShowList(ctx, s_orig_rows, s_orig_first, s_orig_picked, s_orig_shown);
    for (size_t i = 0; i < s_orig_rows.size() && i < s_orig_on.size(); i++)
        Write8(s_orig_rows[i] + kRowOn, s_orig_on[i]);
}

// The page's heading and its description are pool strings the page data draws; they are not the row menu_hooks.cpp
// renamed. While our page is open, rewrite the heading's pool string to PC SETTINGS and use the description (which
// otherwise shows a second PC SETTINGS under the list) for the selected row's help; Leave restores both.
uint32_t FindPool(const char* text, size_t n) {
    uint8_t* lo = kernel::g_base + 0x10000;
    uint8_t* hi = kernel::g_base + 0x08000000;
    MEMORY_BASIC_INFORMATION mi;
    for (uint8_t* a = lo; a < hi && VirtualQuery(a, &mi, sizeof(mi)); a = static_cast<uint8_t*>(mi.BaseAddress) + mi.RegionSize) {
        if (mi.State != MEM_COMMIT || !(mi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE))) continue;
        uint8_t* b = static_cast<uint8_t*>(mi.BaseAddress);
        uint8_t* e = b + mi.RegionSize;
        for (uint8_t* p = b + 3; p + n <= e; p++) {
            p = static_cast<uint8_t*>(std::memchr(p, text[0], size_t(e - p)));
            if (!p || p + n > e) break;
            if (std::memcmp(p, text, n) == 0 && p[-3] == 0) return uint32_t(p - 2 - kernel::g_base);
        }
    }
    return 0;
}

void PatchHeading(X86Context& ctx) {
    // the page's description line is the Xbox Live page's own comment ("Choose to appear Online or Offline."),
    // a pool string; use its room for the selected row's help. menu_hooks.cpp renamed the Xbox Live row text to
    // PC SETTINGS, so that string is only the fallback.
    if (!s_pool_desc) {
        uint32_t d = FindPool("Choose to appear Online or Offline.", sizeof("Choose to appear Online or Offline."));
        if (!d) d = FindPool("PC SETTINGS", sizeof("PC SETTINGS"));
        if (d) {
            s_pool_desc = d;
            std::memcpy(s_pool_desc_orig, kernel::g_base + d + 2, sizeof(s_pool_desc_orig));
            LOGR("settings page: description pool string (%08X: %s)", d,
                 reinterpret_cast<const char*>(kernel::g_base + d + 2));
        }
    }
    if (uint32_t t = FindPool("Xbox Live", sizeof("Xbox Live"))) {
        s_pool_title = t;
        std::memcpy(s_pool_title_orig, kernel::g_base + t + 2, sizeof(s_pool_title_orig));
        std::memcpy(kernel::g_base + t + 2, "PC SETTINGS", sizeof("PC SETTINGS"));
        LOGR("settings page: heading pool string renamed (%08X)", t);
    }
    s_help_opt = -999;
    UpdateHelp();
    (void)ctx;
}

void RestoreHeading() {
    if (s_pool_title) {
        std::memcpy(kernel::g_base + s_pool_title + 2, s_pool_title_orig, sizeof(s_pool_title_orig));
        s_pool_title = 0;
    }
    if (s_pool_desc) {
        std::memcpy(kernel::g_base + s_pool_desc + 2, s_pool_desc_orig, sizeof(s_pool_desc_orig));
        s_pool_desc = 0;
    }
}

void SyncRows(X86Context& ctx) {
    if (!s_ready || s_page >= kPageSave) return;
    const Values v = ReadValues();
    bool choice_changed = false;
    for (RowRef& r : s_pages[s_page]) {
        int opt = r.opt;
        if (opt < 0 || opt >= kOptCount) continue;
        const Def& d = kDefs[opt];
        if (d.kind == kBool) {
            uint32_t want = v.v[opt] != 0;
            if (kernel::g_base[r.row + kBoolState] != want) {
                ctx.ecx = r.row;
                kernel::CallGuest(ctx, kBoolSet, {want, 0});
            }
        } else if (d.kind == kInt) {
            uint32_t want = std::clamp<int>(int(v.v[opt]), d.lo, d.hi);
            if (kernel::R32(r.row + kIntValue) != want) {
                ctx.ecx = r.row;
                kernel::CallGuest(ctx, kIntSet, {want, 0});
            }
        } else if (d.kind == kChoice) {
            uint32_t idx = ChoiceIndex(opt, v.v[opt]);
            if (idx != s_choice_idx[opt]) {
                UpdateChoiceText(ctx, opt, idx);
                choice_changed = true;
            }
        }
    }
    if (choice_changed) ShowPage(ctx, s_page, false);
}

void ApplyOne(X86Context& ctx, int opt) {
    if (opt < 0 || opt >= kOptCount) return;
    Values v = ReadValues();
    const Def& d = kDefs[opt];
    if (d.kind == kChoice) {
        v.v[opt] = ChoiceValue(opt, RowValue(opt));
    } else {
        v.v[opt] = RowValue(opt);
    }
    // the display options wait for APPLY CHANGES
    v.v[kDisplayMode] = uint32_t(app::Get().display_mode);
    v.v[kResolution] = ResIndex(app::Get().width, app::Get().height);
    WriteValues(v);
    s_dirty = true;
    (void)ctx;
}

void ChangeChoice(X86Context& ctx, int opt, uint32_t index) {
    index = std::min<uint32_t>(index, ChoiceCount(opt) ? ChoiceCount(opt) - 1 : 0);
    if (opt == kDisplayMode) {
        s_pending_mode = app::DisplayMode(std::min<uint32_t>(index, 2));
    } else if (opt == kResolution) {
        const app::Mode mode = ResMode(index);
        s_pending_w = mode.width;
        s_pending_h = mode.height;
    }
    UpdateChoiceText(ctx, opt, index);
    kernel::CallGuest(ctx, kMoveSound, {});
    if (opt != kDisplayMode && opt != kResolution) {
        Values v = ReadValues();
        v.v[opt] = ChoiceValue(opt, index);
        v.v[kDisplayMode] = uint32_t(app::Get().display_mode);
        v.v[kResolution] = ResIndex(app::Get().width, app::Get().height);
        WriteValues(v);
        s_dirty = true;
    } else {
        s_dirty = true;
    }
    ShowPage(ctx, s_page, false);
}

void RevertDisplay(X86Context& ctx) {
    app::Settings& s = app::Get();
    s.display_mode = s_revert_mode;
    s.width = s_revert_w;
    s.height = s_revert_h;
    app::ApplyDisplay();
    app::Save();
    s_pending_mode = s_revert_mode;
    s_pending_w = s_revert_w;
    s_pending_h = s_revert_h;
    UpdateChoiceText(ctx, kDisplayMode, std::min<uint32_t>(uint32_t(s_revert_mode), 2));
    UpdateChoiceText(ctx, kResolution, ResIndex(s_revert_w, s_revert_h));
    s_dirty = false;
    s_keep = false;
    LOGR("settings page: display mode restored");
}

void Leave(X86Context& ctx);

void ApplyAll(X86Context& ctx, bool leave_after) {
    Values v = ReadValues();
    app::Settings& s = app::Get();
    const bool display_changed = uint32_t(s.display_mode) != v.v[kDisplayMode] || s.width != ResMode(v.v[kResolution]).width ||
                                 s.height != ResMode(v.v[kResolution]).height;
    s_revert_mode = s.display_mode;
    s_revert_w = s.width;
    s_revert_h = s.height;
    WriteValues(v);
    if (display_changed) app::ApplyDisplay();
    app::Save();
    s_snapshot = v;
    s_dirty = false;
    s_help_opt = -999;
    LOGR("settings page: applied");
    if (display_changed) {
        s_keep = true;
        s_leave_after_keep = leave_after;
        s_revert_at = GetTickCount64() + 15000;
        s_revert_left = -1;
        ShowPage(ctx, kPageKeep, true);
    } else if (leave_after) {
        Leave(ctx);
    } else {
        ShowPage(ctx, kPageTop, true);
    }
}

void Discard(X86Context& ctx) {
    WriteValues(s_snapshot);
    app::ApplyDisplay();
    s_pending_mode = app::DisplayMode(std::min<uint32_t>(s_snapshot.v[kDisplayMode], 2));
    s_pending_w = ResMode(s_snapshot.v[kResolution]).width;
    s_pending_h = ResMode(s_snapshot.v[kResolution]).height;
    SyncRows(ctx);
    s_dirty = false;
    LOGR("settings page: discarded");
}

void Leave(X86Context& ctx) {
    RestoreHeading();
    RestoreOriginal(ctx);
    s_live = false;
    s_keep = false;
    s_page = kPageTop;
    s_help_opt = -999;
    ctx.ecx = s_handler;
    kernel::CallGuest(ctx, kOriginalBack, {0});  // s_live is clear: our hook passes it to the game's back
}

void TryLeave(X86Context& ctx) {
    if (s_dirty) {
        ShowPage(ctx, kPageSave, true);
        return;
    }
    Leave(ctx);
}

bool Press(X86Context& ctx, uint32_t handler) {
    if (!s_ready || handler != s_handler) return false;
    uint32_t menu = s_menu;
    uint32_t row = Selected(menu);
    if (!row) return false;

    if (s_page == kPageSave) {
        if (row == (s_pages[kPageSave].size() > 1 ? s_pages[kPageSave][1].row : 0)) {
            ApplyAll(ctx, true);
        } else if (row == (s_pages[kPageSave].size() > 2 ? s_pages[kPageSave][2].row : 0)) {
            Discard(ctx);
            Leave(ctx);
        }
        return true;
    }
    if (s_page == kPageKeep) {
        if (row == (s_pages[kPageKeep].size() > 1 ? s_pages[kPageKeep][1].row : 0)) {
            s_keep = false;
            if (s_leave_after_keep) {
                s_leave_after_keep = false;
                Leave(ctx);
            } else {
                ShowPage(ctx, kPageTop, true);
            }
        } else if (row == (s_pages[kPageKeep].size() > 2 ? s_pages[kPageKeep][2].row : 0)) {
            RevertDisplay(ctx);
            if (s_leave_after_keep) {
                s_leave_after_keep = false;
                Leave(ctx);
            } else {
                ShowPage(ctx, kPageTop, true);
            }
        }
        return true;
    }

    int opt = 0;
    if (!RowIn(s_page, row, opt)) return true;
    if (opt == kHeader) return true;
    if (opt == kOpenDisplay) {
        ShowPage(ctx, kPageDisplay, true);
        return true;
    }
    if (opt == kOpenControls) {
        ShowPage(ctx, kPageControls, true);
        return true;
    }
    if (opt == kOpenGame) {
        ShowPage(ctx, kPageGame, true);
        return true;
    }
    if (opt == kKeyBindings) {
        menu::OpenControls();
        return true;
    }
    if (opt == kReport) {
        report::Open();
        return true;
    }
    if (opt == kApply) {
        ApplyAll(ctx, false);
        return true;
    }
    if (opt == kBack) {
        if (s_page == kPageTop) TryLeave(ctx);
        else ShowPage(ctx, kPageTop, true);
        return true;
    }

    const Def& d = kDefs[opt];
    if (d.kind == kBool) {
        uint32_t want = kernel::g_base[row + kBoolState] ? 0 : 1;
        ctx.ecx = row;
        kernel::CallGuest(ctx, kBoolSet, {want, 1});
        ApplyOne(ctx, opt);
    } else if (d.kind == kInt) {
        uint32_t value = kernel::R32(row + kIntValue);
        if (value < uint32_t(d.hi))
            value++;
        else
            value = uint32_t(d.lo);
        ctx.ecx = row;
        kernel::CallGuest(ctx, kIntSet, {value, 1});
        ApplyOne(ctx, opt);
    } else if (d.kind == kChoice) {
        if (uint32_t count = ChoiceCount(opt)) ChangeChoice(ctx, opt, (RowValue(opt) + 1) % count);
    }
    return true;
}

bool Pressed(X86Context& ctx, uint32_t menu, uint32_t state, int direction) {
    if (!state) return false;
    return kernel::CallGuest(ctx, kInputTest, {menu + kRowInput, uint32_t(direction), state, 0}) != 0;
}

}  // namespace

// the game's Xbox Live options page handler is built for both the front end OPTIONS page and the pause menu; once
// it exists our rows go in place of the Xbox Live ones
void Construct(X86Context& ctx) {
    uint32_t handler = ctx.ecx;
    uint32_t menu = handler ? kernel::R32(handler + 4) : 0;
    if (!menu || kernel::R32(menu) != kMenuVtable) {
        LOGR("settings page: Xbox Live page element %08X is not a menu (vtable %08X)", menu,
             menu ? kernel::R32(menu) : 0);
        return;
    }

    if (menu == s_menu && s_ready) return;
    LOGR("settings page: construct handler %08X menu %08X", handler, menu);

    s_handler = handler;
    s_menu = menu;
    s_ready = false;
    s_live = false;
    s_dirty = false;
    s_keep = false;
    s_page = kPageTop;
    s_orig_rows.clear();
    s_orig_on.clear();
    for (int p = 0; p < kPageCount; p++) s_pages[p].clear();

    s_orig_count = kernel::R16(menu + kMenuCount);
    uint32_t rows = kernel::R32(menu + kMenuRows);
    for (uint32_t i = 0; rows && i < s_orig_count && i < 64; i++) {
        uint32_t row = kernel::R32(rows + i * 4);
        s_orig_rows.push_back(row);
        s_orig_on.push_back(row ? kernel::g_base[row + kRowOn] : 0);
    }
    s_orig_first = kernel::R32(menu + kMenuFirst);
    s_orig_shown = kernel::R32(menu + kMenuShown);
    s_orig_picked = kernel::R32(menu + kMenuPicked);

    if (!s_strings) s_strings = Alloc(ctx, 0x4000);
    if (!s_ids) s_ids = Alloc(ctx, 0x1000);
    if (!ReadyForText()) {
        LOGE("settings page: no guest memory for texts");
        return;
    }
    LOGR("settings page: guest text %08X ids %08X", s_strings, s_ids);
    s_strings_at = s_ids_at = 0;
    s_pool_title = s_pool_desc = 0;
    std::memset(s_choice_text, 0, sizeof(s_choice_text));
    for (int opt = 0; opt < kOptCount; opt++) s_choice_rows[opt].clear();
    s_pending_mode = app::Get().display_mode;
    s_pending_w = app::Get().width;
    s_pending_h = app::Get().height;

    s_res_names.clear();
    s_res_names.push_back("DESKTOP");
    for (const app::Mode& m : menu::Modes()) {
        char line[32];
        std::snprintf(line, sizeof(line), "%u X %u", m.width, m.height);
        s_res_names.push_back(line);
    }

    for (int opt = 0; opt < kOptCount; opt++) {
        const Def& d = kDefs[opt];
        if (d.kind != kChoice) continue;
        LOGR("settings page: choice texts %d", opt);
        const uint32_t count = std::min<uint32_t>(ChoiceCount(opt), 63);
        for (uint32_t i = 0; i < count; i++) {
            char line[128];
            std::snprintf(line, sizeof(line), "~%s", ChoiceName(opt, i));
            s_choice_text[opt][i] = GuestText(ctx, line);
        }
    }

    BuildRows(ctx);
    // the value is read again now that the page is open: an F1 change made while the menu was away shows up
    s_snapshot = ReadValues();
    s_dirty = false;
    LOGR("settings page: rows built");
    s_ready = true;
    s_pending = true;
    LOGR("settings page: ready in menu %08X (was %u rows)", menu, s_orig_count);
}

// called by the OPTIONS / pause select handlers just before the game opens the Xbox Live page
void Opening(X86Context& ctx) {
    const app::Settings& s = app::Get();
    s_pending_mode = s.display_mode;
    s_pending_w = s.width;
    s_pending_h = s.height;
    const Values snapshot = ReadValues();
    if (s_ready) {
        // Only take the snapshot here: the page is being opened by the game's own select handler and the menu
        // must not be reshaped under it. The next update puts our rows up, before the page is drawn.
        s_snapshot = snapshot;
        s_dirty = false;
        s_keep = false;
        s_live = true;
        s_page = kPageTop;
        s_help_opt = -999;
    }
    s_pending = true;
}

void EnableSelect(X86Context& ctx);

// The page handler's update. Returns 0 for another handler, 1 when ours has been updated and 2 when the page has
// just been opened: the caller then runs the game's own update once (it enables the page's button prompts) before
// Opened puts our rows up.
int Update(X86Context& ctx) {
    if (!s_ready || ctx.ecx != s_handler) return 0;
    if (s_pending) {
        const app::Settings& s = app::Get();
        s_pending_mode = s.display_mode;
        s_pending_w = s.width;
        s_pending_h = s.height;
        s_snapshot = ReadValues();
        s_dirty = false;
        s_keep = false;
        s_live = true;
        s_page = kPageTop;
        s_pending = false;
        s_help_opt = -999;
        LOGR("settings page: update opening");
        return 2;
    }
    if (!s_live) return 1;
    if (s_page == kPageKeep) {
        const long left = long((s_revert_at - GetTickCount64()) / 1000);
        if (GetTickCount64() >= s_revert_at) {
            RevertDisplay(ctx);
            if (s_leave_after_keep) {
                s_leave_after_keep = false;
                Leave(ctx);
            } else {
                ShowPage(ctx, kPageTop, true);
            }
            return 1;
        }
        if (left != s_revert_left) {
            s_revert_left = left;
            char line[24];
            std::snprintf(line, sizeof(line), "GOING BACK: %ld S", left < 0 ? 0 : left);
            SetDescription(line);
        }
    } else {
        UpdateHelp();
        SyncRows(ctx);
    }
    EnableSelect(ctx);
    return 1;
}

// The page's button bar has the SELECT prompt (entry 4) but the Xbox Live page data leaves it hidden. Show it
// again after the page's own update, which puts it back to the page's default.
void EnableSelect(X86Context& ctx) {
    uint32_t page = kernel::R32(s_handler + 8);
    uint32_t bar = page ? kernel::R32(page + 0x54) : 0;
    if (!bar) return;
    ctx.ecx = bar;
    kernel::CallGuest(ctx, kButtonOn, {4});
}

// after the game's own first update: our rows up, heading renamed
void Opened(X86Context& ctx) {
    if (!s_live) return;
    ShowPage(ctx, kPageTop, true);
    PatchHeading(ctx);
    EnableSelect(ctx);
    LOGR("settings page: opened");
}

bool PressHandler(X86Context& ctx, uint32_t handler) { return Press(ctx, handler); }

bool BackHandler(X86Context& ctx, uint32_t handler) {
    // !s_live: another page's back, or the original back after Leave; never re-enter
    if (!s_ready || !s_live || handler != s_handler) return false;
    if (s_page == kPageSave) {
        ShowPage(ctx, kPageTop, true);
        return true;
    }
    if (s_page == kPageKeep) {
        RevertDisplay(ctx);
        if (s_leave_after_keep) {
            s_leave_after_keep = false;
            Leave(ctx);
        } else {
            ShowPage(ctx, kPageTop, true);
        }
        return true;
    }
    if (s_page != kPageTop) {
        ShowPage(ctx, kPageTop, true);
        return true;
    }
    TryLeave(ctx);
    return true;
}

// left and right on the selected row: the game's own widget input changes a slider or an on/off row; a choice row
// steps through its values here (it has one ready row per value, so the text changes without rewriting it)
void MenuInput(X86Context& ctx, uint32_t menu) {
    if (!s_ready || menu != s_menu || !s_live || s_page >= kPageSave) return;
    uint32_t row = Selected(menu);
    int opt = 0;
    if (!row || !RowIn(s_page, row, opt) || opt < 0) return;
    const Def& d = kDefs[opt];
    if (d.kind == kAction || d.kind == kChoice) {
        if (d.kind == kAction) return;
        uint32_t state = kernel::R32(ctx.esp + 8);
        const bool left = Pressed(ctx, menu, state, 2), right = Pressed(ctx, menu, state, 3);
        if (left == right) return;
        const uint32_t count = ChoiceCount(opt);
        if (!count) return;
        const uint32_t index = ChoiceIndex(opt, ReadValues().v[opt]);
        const uint32_t next = right ? std::min(index + 1, count - 1) : (index ? index - 1 : 0);
        if (next != index) ChangeChoice(ctx, opt, next);
        return;
    }
    uint32_t state = kernel::R32(ctx.esp + 8);
    if (!state) return;
    uint32_t before = RowValue(opt);
    ctx.ecx = row;
    kernel::CallGuest(ctx, d.kind == kBool ? kBoolInput : kIntInput, {0, state});
    if (RowValue(opt) != before) ApplyOne(ctx, opt);
}

// the page handler is going away (the pause menu was closed, or the front end left): unapplied changes are
// discarded, like DISCARD CHANGES, so a Start close can never leave half-applied options behind
void Destroying(X86Context& ctx, uint32_t handler) {
    if (!s_ready || !s_live || handler != s_handler) return;
    LOGR("settings page: handler destroyed while open");
    Discard(ctx);
    RestoreHeading();
    s_live = false;
    s_keep = false;
    s_page = kPageTop;
}

}  // namespace settings_menu

X86_EXTERN_FUNC(__imp__sub_000F6A6D);
X86_FUNC(sub_000F6A6D) {
    __imp__sub_000F6A6D(ctx, base);
    settings_menu::Construct(ctx);
}

X86_EXTERN_FUNC(__imp__sub_000F6B14);
X86_FUNC(sub_000F6B14) {
    switch (settings_menu::Update(ctx)) {
        case 0: break;
        case 2:
            // the game's own first update enables the page's button prompts (A select / B back)
            __imp__sub_000F6B14(ctx, base);
            settings_menu::Opened(ctx);
            return;
        default: return;
    }
    __imp__sub_000F6B14(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_000F6DD3);
X86_FUNC(sub_000F6DD3) {
    if (settings_menu::PressHandler(ctx, ctx.ecx)) {
        kernel::Return(ctx, 0, 1);
        return;
    }
    __imp__sub_000F6DD3(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_000F68E3);
X86_FUNC(sub_000F68E3) {
    if (settings_menu::BackHandler(ctx, ctx.ecx)) {
        kernel::Return(ctx, 0, 1);
        return;
    }
    __imp__sub_000F68E3(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_000F5351);
X86_FUNC(sub_000F5351) {
    settings_menu::Destroying(ctx, ctx.ecx);
    __imp__sub_000F5351(ctx, base);
}
