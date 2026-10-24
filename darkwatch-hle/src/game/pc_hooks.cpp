// PC options inside the game's own code: field of view (world and weapon), widescreen framing, direct mouse aiming,
// reticle size and the in-level cut-scene borders. The settings live in src/common/menu.cpp.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstring>

#include "app/app.h"
#include "core/log.h"
#include "core/input.h"
#include "xdk/hle.h"

namespace menu {
extern uint32_t g_fov;         // degrees, 0: the game's
extern uint32_t g_weapon_fov;  // degrees, 0: the game's
extern bool g_widescreen;      // 3D drawn wider on a wide screen instead of stretched
extern bool g_mouse_direct;
extern uint32_t g_reticle;     // percent
extern bool g_cutscene_borders;
}  // namespace menu

namespace mouse_menus {
void Playing();  // mouse_menus.cpp: the player's view turned, so no menu has the mouse
}

namespace settings_menu {
void Playing(X86Context& ctx);  // settings_menu.cpp: the pause menu closed with PC SETTINGS up, leave it
}

namespace {

float F32(uint32_t va) {
    uint32_t b = kernel::R32(va);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}
void WF32(uint32_t va, float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    kernel::W32(va, b);
}

// the game's own view: 55 degrees across a 4:3 frame (the player camera's setup, 0x000F9B7D)
constexpr float kGameFov = 55.0f;
constexpr float kPi = 3.14159265f;

// how much wider than the game's a setting's view is (tan of half the angle), 1 for 0
float FovScale(uint32_t degrees) {
    if (!degrees) return 1;
    float d = std::fmin(std::fmax(float(degrees), kGameFov), 110.0f);  // the settings' range
    return std::tan(d * kPi / 360.0f) / std::tan(kGameFov * kPi / 360.0f);
}

// local players: the game's singleton's count (1 outside split screen)
uint32_t LocalPlayers() {
    uint32_t game = kernel::R32(0x004AC9A0);
    return game ? kernel::R32(game + 0x4C) : 1;
}

}  // namespace

// ---- field of view and widescreen ----
//
// A view camera (0x68 bytes, made by sub_002172AE) keeps its aspect at +0x40, its angle in degrees at +0x44 (across
// the frame), a scale at +0x48 and its RenderWare camera at +0x34. sub_00216B7C sets the angle and hands the RW
// camera the view window tan(angle / 2) * scale wide, that over the aspect high. The player's camera is the one
// sub_000F9B1D makes (its only caller is the player view's setup): the aspect is 4:3, so on a wide screen the frame is
// stretched across it. Each time that camera starts a frame (sub_00216AF7, its BeginUpdate) the window is set again
// from the camera's own angle, wider and taller for the field of view setting, and wider for a wide screen (Hor+: the
// height stays the game's), so the game's zooms keep working on top. The first-person arms and gun draw through the same camera in
// sub_000DEDD0, between their own BeginUpdate and EndUpdate, and get the weapon setting.
namespace {
constexpr int kCams = 4;  // one a local player
std::atomic<uint32_t> s_cam[kCams] = {}, s_rwcam[kCams] = {};
thread_local bool s_weapon_pass = false;

bool PlayerCamera(uint32_t cam) {
    for (int i = 0; i < kCams; i++)
        if (cam && s_cam[i] == cam && kernel::R32(cam + 0x34) == s_rwcam[i]) return true;
    return false;
}

// RwCameraSetViewWindow (sub_00117830, cdecl: camera, window), when the window is not that already
void SetWindow(X86Context& ctx, uint32_t cam, bool weapon) {
    uint32_t rw = kernel::R32(cam + 0x34);
    float aspect = F32(cam + 0x40), angle = F32(cam + 0x44), scale = F32(cam + 0x48);
    if (!rw || !(aspect > 0) || !(angle > 0 && angle < 180) || !(scale > 0)) return;
    float wide = menu::g_widescreen && app::Widescreen() ? (16.0f / 9.0f) / (4.0f / 3.0f) : 1.0f;
    // the setting widens both ways (the picture keeps its shape); a wide screen only across
    float x = std::tan(angle * kPi / 360.0f) * scale * FovScale(weapon ? menu::g_weapon_fov : menu::g_fov);
    float w[2] = {x * wide, x / aspect};
    if (F32(rw + 0x68) == w[0] && F32(rw + 0x6C) == w[1]) return;
    uint32_t esp = ctx.esp;
    ctx.esp -= 0x20;
    uint32_t at = ctx.esp;
    WF32(at, w[0]);
    WF32(at + 4, w[1]);
    kernel::CallGuest(ctx, 0x00117830, {rw, at});
    ctx.esp = esp;
}
}  // namespace

// the player view's setup (cdecl: its settings; returns the view, whose +0xC is its camera)
X86_EXTERN_FUNC(__imp__sub_000F9B1D);
X86_FUNC(sub_000F9B1D) {
    __imp__sub_000F9B1D(ctx, base);
    uint32_t view = ctx.eax, cam = view ? kernel::R32(view + 0xC) : 0;
    if (!cam) return;
    int slot = 0;
    for (int i = 0; i < kCams; i++) {
        // the same camera again, a free slot, or one whose camera is gone
        if (!s_cam[i] || s_cam[i] == cam || kernel::R32(s_cam[i] + 0x34) != s_rwcam[i]) {
            slot = i;
            break;
        }
    }
    s_cam[slot] = cam;
    s_rwcam[slot] = kernel::R32(cam + 0x34);
    LOGR("view: player camera %08X (slot %d)", cam, slot);
}

// a camera's BeginUpdate (thiscall, returns whether it began)
X86_EXTERN_FUNC(__imp__sub_00216AF7);
X86_FUNC(sub_00216AF7) {
    uint32_t cam = ctx.ecx;
    if (PlayerCamera(cam)) SetWindow(ctx, cam, s_weapon_pass);
    __imp__sub_00216AF7(ctx, base);
}

// the first-person actor's draw (thiscall: the camera; ret 4)
X86_EXTERN_FUNC(__imp__sub_000DEDD0);
X86_FUNC(sub_000DEDD0) {
    uint32_t cam = kernel::R32(ctx.esp + 4);
    if (!PlayerCamera(cam)) {
        __imp__sub_000DEDD0(ctx, base);
        return;
    }
    s_weapon_pass = true;
    __imp__sub_000DEDD0(ctx, base);
    s_weapon_pass = false;
    SetWindow(ctx, cam, false);  // the world's window back
}

// ---- direct mouse aiming ----
//
// The player's look, once a frame: sub_000A2725 turns (yaw, degrees at the character's +0x148) and sub_000A28AE
// tilts (pitch at +0x144, between its limits at +0x5C and +0x60), both thiscall (dt, input; ret 8) on the look
// component, whose +0x40 is the character. They turn at a rate from the right stick, through a response curve and the
// aim assist (slowing and pulling toward a target, by the strength at the component's +0x5C). With the mouse as the
// stick, a fast flick hits full stick and the curve flattens slow moves. Here the mouse counts turn the view by an
// angle each instead, and while the mouse aims the assist is off and the pitch does not level itself (the flag at
// +0x74 the stick's rest sets). Split screen keeps the mouse as player 1's stick: the look calls do not say whose
// view they are. Only the on-foot controller's calls (sub_000A83DD, returning to 0xA84A1 and 0xA84B0) aim this way;
// the other movement controllers (sub_000A1AC5, sub_000A1042) call the same look, and there the mouse stays the
// stick: taking its motion while one of them runs (around the horse, a player report) leaves no mouse aim at all.
namespace {
constexpr float kDegreesPerCount = 0.06f;  // at 100% sensitivity

float Wrap(float deg) {
    while (deg > 180) deg -= 360;
    while (deg < -180) deg += 360;
    return deg;
}

// ret: the look call's return address, which says whose controller turns the view
bool Direct(uint32_t ret, uint32_t on_foot) {
    return ret == on_foot && menu::g_mouse_direct && app::Get().keyboard && app::Get().mouse_look && LocalPlayers() == 1;
}

float Degrees(int counts) { return float(counts) * kDegreesPerCount * float(app::Get().mouse_sensitivity) / 100.0f; }

// ms of the last mouse aim, for the assist and the pitch leveling
std::atomic<uint64_t> s_aimed{0};
}  // namespace

X86_EXTERN_FUNC(__imp__sub_000A2725);
X86_FUNC(sub_000A2725) {
    mouse_menus::Playing();
    settings_menu::Playing(ctx);
    uint32_t look = ctx.ecx, ch = kernel::R32(look + 0x40);
    // which movement controller turns the view, logged once each
    static uint32_t s_seen[4];
    uint32_t from = kernel::R32(ctx.esp);
    for (uint32_t& s : s_seen)
        if (s == from) break;
        else if (!s) {
            s = from;
            LOGR("mouse: look turned from %08X (character %08X)", from, ch);
            break;
        }
    if (!Direct(from, 0xA84A1) || !ch) {
        __imp__sub_000A2725(ctx, base);
        return;
    }
    int counts = input::TakeMouse(0);
    uint64_t now = GetTickCount64();
    if (counts) s_aimed = now;
    bool aiming = now - s_aimed < 1000;
    uint32_t assist = kernel::R32(look + 0x5C);
    if (aiming) kernel::W32(look + 0x5C, 0);
    __imp__sub_000A2725(ctx, base);
    if (aiming) kernel::W32(look + 0x5C, assist);
    if (counts) WF32(ch + 0x148, Wrap(F32(ch + 0x148) - Degrees(counts)));
}

X86_EXTERN_FUNC(__imp__sub_000A28AE);
X86_FUNC(sub_000A28AE) {
    uint32_t look = ctx.ecx, ch = kernel::R32(look + 0x40);
    if (!Direct(kernel::R32(ctx.esp), 0xA84B0) || !ch) {
        __imp__sub_000A28AE(ctx, base);
        return;
    }
    int counts = input::TakeMouse(1);
    uint64_t now = GetTickCount64();
    if (counts) s_aimed = now;
    bool aiming = now - s_aimed < 1000;
    uint32_t assist = kernel::R32(look + 0x5C);
    if (aiming) {
        kernel::W32(look + 0x5C, 0);
        kernel::g_base[look + 0x74] = 0;
    }
    __imp__sub_000A28AE(ctx, base);
    if (aiming) kernel::W32(look + 0x5C, assist);
    if (!counts) return;
    // the game's invert look setting (the character's +0x139) flips the mouse as it flips the stick
    float d = Degrees(counts) * (kernel::g_base[ch + 0x139] ? -1.0f : 1.0f);
    float lo = F32(ch + 0x5C), hi = F32(ch + 0x60);
    float p = F32(ch + 0x144) + d;
    if (lo < hi) p = std::fmin(std::fmax(p, lo), hi);
    WF32(ch + 0x144, p);
}

// ---- reticle size ----
//
// The HUD's sprite draw sub_000DCCDB (thiscall: quad, x, y, width, height, ...; ret 0x1C) scales the width and height
// by the element's own scale; for the reticle (ReticleDisplay, vtable 0x00428378) the setting scales them too, about
// the same centre.
X86_EXTERN_FUNC(__imp__sub_000DCCDB);
X86_FUNC(sub_000DCCDB) {
    if (menu::g_reticle != 100 && kernel::R32(ctx.ecx) == 0x00428378) {
        float s = float(std::fmin(std::fmax(float(menu::g_reticle), 25.0f), 300.0f)) / 100.0f;
        WF32(ctx.esp + 0x10, F32(ctx.esp + 0x10) * s);
        WF32(ctx.esp + 0x14, F32(ctx.esp + 0x14) * s);
    }
    __imp__sub_000DCCDB(ctx, base);
}

// ---- cut-scene borders ----
//
// The cinematic manager's letterbox is a ScreenMaskEffect it makes once (sub_0021B3D7) and keeps at 0x004B7A68;
// its draw sub_00326CD1 (thiscall: the camera; ret 4) puts a bar above and one below. With the setting off that
// effect draws nothing; fades and other mask effects draw as before, and so does the letterbox while a movie plays
// (the flag at 0x004B7A41).
X86_EXTERN_FUNC(__imp__sub_00326CD1);
X86_FUNC(sub_00326CD1) {
    if (!menu::g_cutscene_borders && ctx.ecx == kernel::R32(0x004B7A68) && !kernel::g_base[0x004B7A41]) {
        kernel::Return(ctx, 0, 1);
        return;
    }
    __imp__sub_00326CD1(ctx, base);
}
