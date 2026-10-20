// The mouse in the game's menus: pointing at a row picks it, a left click is A on it, a right click is B, the wheel
// steps through the rows. While a menu takes input the cursor is free and shown (input::SetPointer); in play the
// mouse looks again.
//
// A menu (vtable 0x43C8B8) gets the pads' presses in sub_001EF55C (thiscall, 2 args), which moves the pick a row at a
// time and calls SetSelected (sub_001EF329, thiscall, the row) after the move sound (sub_001E917C). Its rows: count
// (u16) +0x58, array +0x5C, picked +0x68, first shown +0x60, rows shown +0x64 (0: all), takes input +0xA1, across
// rather than down +0x70; a row is on with +0x54. Every element keeps its box at +0x10 as centre and half size in
// its parent's box (+0x04), -1 to 1 with y up, the screen's -1 to 1 at the top; the menu draws its shown rows as
// equal bands from its top (sub_001EF781), inside the box at +0x88.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"
#include "xdk/hle.h"

namespace menu {
bool MenuOpen();
}

namespace mouse_menus {

bool g_on = true;  // settings.toml mouse_menus

namespace {

constexpr uint32_t kMoveSound = 0x001E917C, kSetSelected = 0x001EF329;
// the cursor stays free this long after a menu took input: a screen change takes a moment without one
constexpr uint64_t kGrace = 1500;

std::atomic<uint64_t> s_menu_at{0};  // when a menu last took input, ms
std::atomic<uint64_t> s_play_at{0};  // when the player's view last turned, ms
std::atomic<int> s_presses[4] = {};  // A, B, up, down: presses for the pad hook to make
bool s_left = false, s_right = false;  // the buttons as last seen
int s_last_x = -1, s_last_y = -1;    // the cursor as last seen
std::atomic<int> s_wheel{0};         // notches, up positive

uint64_t Now() { return GetTickCount64(); }

struct Box {
    float x, y, w, h;  // centre, half size
};

Box Read(uint32_t a) {
    Box b;
    std::memcpy(&b, kernel::g_base + a, sizeof(b));
    return b;
}

Box Inside(const Box& p, const Box& c) { return {p.x + c.x * p.w, p.y + c.y * p.h, c.w * p.w, c.h * p.h}; }

// an element's box on the screen, through its parents
Box OnScreen(uint32_t e, int depth = 0) {
    Box b = Read(e + 0x10);
    uint32_t parent = kernel::R32(e + 4);
    if (!parent || depth > 12) return b;
    return Inside(OnScreen(parent, depth + 1), b);
}

// the cursor on the game's picture, -1 to 1 with y up; false off the picture or the window
bool Cursor(float& x, float& y, int& px, int& py) {
    HWND hwnd = static_cast<HWND>(app::Window());
    POINT p;
    RECT r;
    if (!hwnd || GetForegroundWindow() != hwnd || !GetCursorPos(&p) || !ScreenToClient(hwnd, &p) || !GetClientRect(hwnd, &r))
        return false;
    // the frame is shown 16:9 or 4:3 in the middle of the window (gpu BlitFrame)
    float w = float(r.right), h = float(r.bottom), aspect = app::Widescreen() ? 16.0f / 9.0f : 4.0f / 3.0f;
    float vw = std::min(w, h * aspect), vh = vw / aspect;
    x = (float(p.x) - (w - vw) / 2) / vw * 2 - 1;
    y = 1 - (float(p.y) - (h - vh) / 2) / vh * 2;
    px = p.x;
    py = p.y;
    return x >= -1 && x <= 1 && y >= -1 && y <= 1;
}

// the shown row of the menu under the cursor, -1 for none
int RowAt(uint32_t m, float x, float y) {
    uint32_t n = kernel::R16(m + 0x58), rows = kernel::R32(m + 0x5C);
    if (!n || !rows) return -1;
    uint32_t shown = kernel::R32(m + 0x64);
    if (!shown || shown > n) shown = n;
    uint32_t first = kernel::R32(m + 0x60);
    Box area = Read(m + 0x88);
    Box box = OnScreen(m);
    if (area.w > 0 && area.h > 0) box = Inside(box, area);
    if (std::fabs(x - box.x) > box.w) return -1;
    float band = 1.0f / float(shown);
    for (uint32_t j = 0; j < shown && first + j < n; j++) {
        Box row = Inside(box, {0, 1 - band - 2 * band * float(j), 1, band});
        if (std::fabs(y - row.y) <= row.h) return int(first + j);
    }
    return -1;
}

bool On(uint32_t m, int i) {
    uint32_t row = kernel::R32(kernel::R32(m + 0x5C) + uint32_t(i) * 4);
    return row && kernel::g_base[row + 0x54];
}

// the mouse stays with the menus: a menu took input just now, or a while ago with the player's view standing since
// (the view updates under the pause menu too, but goes on past it once play resumes)
bool Held(uint64_t now) {
    uint64_t menu = s_menu_at;
    return now - menu < 250 || (now - menu < kGrace && s_play_at < menu + 100);
}

// a press for the pad hook to make: 60 ms down, 40 ms up
struct Press {
    uint64_t until = 0;
    bool down = false;
};
Press s_press[4];

}  // namespace

// game thread: a menu takes the pads' presses
void MenuInput(X86Context& ctx, uint32_t m) {
    if (!g_on || !app::Get().keyboard || !kernel::g_base[m + 0xA1]) return;
    s_menu_at = Now();
    static bool was_on = false;  // the pointer, as this saw it last
    if (!input::Pointer()) {
        was_on = false;
        return;
    }
    float x, y;
    int px, py;
    bool in = Cursor(x, y, px, py);
    // where the cursor is when the pointer comes on (the middle, after mouse look) is no move
    bool moved = was_on && (px != s_last_x || py != s_last_y);
    was_on = true;
    s_last_x = px;
    s_last_y = py;
    int row = in ? RowAt(m, x, y) : -1;
    if (row >= 0 && !On(m, row)) row = -1;
    if (row >= 0 && moved && uint32_t(row) != kernel::R32(m + 0x68)) {
        kernel::CallGuest(ctx, kMoveSound, {});
        ctx.ecx = m;
        kernel::CallGuest(ctx, kSetSelected, {uint32_t(row)});
    }
    bool left = in && (GetAsyncKeyState(VK_LBUTTON) & 0x8000), right = in && (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
    // a click on a row is A on it; a menu without rows takes it anywhere
    if (left && !s_left && (row >= 0 || !kernel::R16(m + 0x58))) {
        if (row >= 0 && uint32_t(row) != kernel::R32(m + 0x68)) {
            ctx.ecx = m;
            kernel::CallGuest(ctx, kSetSelected, {uint32_t(row)});
        }
        s_presses[0]++;
    }
    if (right && !s_right) s_presses[1]++;
    s_left = left;
    s_right = right;
    int wheel = s_wheel.exchange(0);
    for (; wheel > 0; wheel--) s_presses[2]++;
    for (; wheel < 0; wheel++) s_presses[3]++;
}

// each frame (render thread): the cursor is free while a menu took input in the last moment
void Frame() {
    bool want = g_on && app::Get().keyboard && !menu::MenuOpen() && Held(Now());
    if (want != input::Pointer()) {
        input::SetPointer(want);
        LOGI("mouse menus: pointer %s", want ? "on" : "off");
        if (!want)
            for (auto& p : s_presses) p = 0;
    }
}

// game thread: the player's view update ran, a menu does not have the mouse (the pause menu's grace ends at once)
void Playing() { s_play_at = Now(); }

// window thread
void Wheel(int delta) {
    if (input::Pointer()) s_wheel += delta / WHEEL_DELTA;
}

// player 1's pad, as polled: the clicks and the wheel as A, B and the d-pad up and down
void Pad(int port, input::Pad& pad) {
    if (port != 0) return;
    uint64_t now = Now();
    for (int i = 0; i < 4; i++) {
        Press& p = s_press[i];
        if (p.until && now >= p.until) {
            p.until = p.down ? now + 40 : 0;  // let go, then a gap before the next
            p.down = false;
        }
        if (!p.until && s_presses[i] > 0) {
            s_presses[i]--;
            p.down = true;
            p.until = now + 60;
        }
        if (!p.down) continue;
        if (i == 0) pad.analog[0] = 0xFF;
        else if (i == 1) pad.analog[1] = 0xFF;
        else pad.buttons |= uint16_t(i == 2 ? 1 : 2);  // d-pad up, down
    }
}

}  // namespace mouse_menus

X86_EXTERN_FUNC(__imp__sub_001EF55C);
X86_FUNC(sub_001EF55C) {
    mouse_menus::MenuInput(ctx, ctx.ecx);
    __imp__sub_001EF55C(ctx, base);
}
