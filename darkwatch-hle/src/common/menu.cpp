// the PC settings menu (ImGui): F1, or Back+Start on a pad, opens and closes it. Pages on the left,
// their settings on the right; most apply at once, the display mode on Apply with a keep-or-revert
// countdown. The settings file is written when the menu closes. The game gets no input while it is up.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"
#include "gpu/backend.h"

namespace report {
void ReportWindow(void* hwnd);
bool ReportMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
void ReportUi(double fps);
void ReportPad(int port, input::Pad& pad);
bool ReportOpen();
}  // namespace report

namespace game_menu {
void Patch();  // src/game/menu_hooks.cpp
}

namespace content {
extern bool g_bonus;  // content.cpp
}

namespace prompts {
extern uint32_t g_style;  // prompts.cpp
void Frame();
}  // namespace prompts

namespace mouse_menus {  // src/game/mouse_menus.cpp
extern bool g_on;
void Frame();
void Wheel(int delta);
void Pad(int port, input::Pad& pad);
}  // namespace mouse_menus

namespace menu {

bool g_show_fps = false;  // settings file: show_fps
bool g_ps2_alpha = false;  // settings file: ps2_alpha
// the game-side options (src/game/pc_hooks.cpp), settings file names in game_info.cpp
uint32_t g_fov = 80;        // fov: degrees across a 4:3 frame, 0 the game's 55
uint32_t g_weapon_fov = 0;  // weapon_fov: the same for the first-person arms and gun
bool g_widescreen = true;   // widescreen_fov
bool g_mouse_direct = true;  // mouse_direct_aim
uint32_t g_reticle = 100;    // reticle_size, percent
bool g_cutscene_borders = false;  // cutscene_borders

namespace {

using Clock = std::chrono::steady_clock;

std::atomic<bool> s_open{false};
std::atomic<bool> s_pad_toggle{false};  // Back+Start seen by the pad hook, for the next ui frame
std::atomic<bool> s_game_open{false};   // the game's PC SETTINGS entry was chosen (a guest thread)
std::atomic<bool> s_hold{false};        // after closing: the game gets no input until everything is let go
bool s_mouse_was_captured = false;
bool s_dirty = false;  // something changed: the settings file is written on close
int s_page = 0;
float s_scale = 0;  // the ui scale the style was last sized for
ImGuiStyle s_base_style;

// display: what the page shows before Apply, and what Apply can go back to
struct Display {
    app::DisplayMode mode;
    uint32_t width, height;
};
Display s_pending{}, s_revert{};
Clock::time_point s_revert_at{};
bool s_confirming = false;
std::vector<app::Mode> s_modes;  // the monitor's resolutions, largest first

// rebinding: the control waiting for a key, and whether that key replaces or joins the others
int s_bind = -1;
bool s_bind_add = false;
bool s_bind_released = false;  // the click that started it has been let go

const char* kPages[] = {"Display", "Graphics", "Controls", "Game"};
constexpr int kPageCount = int(sizeof(kPages) / sizeof(kPages[0]));
const char* kModeNames[] = {"Fullscreen", "Borderless", "Windowed"};

// settings file control name, label
const std::pair<const char*, const char*> kControlLabels[] = {
    {"a", "A"},
    {"b", "B"},
    {"x", "X"},
    {"y", "Y"},
    {"black", "Black"},
    {"white", "White"},
    {"left_trigger", "Left trigger"},
    {"right_trigger", "Right trigger"},
    {"dpad_up", "D-pad up"},
    {"dpad_down", "D-pad down"},
    {"dpad_left", "D-pad left"},
    {"dpad_right", "D-pad right"},
    {"start", "Start"},
    {"back", "Back"},
    {"left_thumb", "Left stick click"},
    {"right_thumb", "Right stick click"},
    {"left_stick_left", "Move left"},
    {"left_stick_right", "Move right"},
    {"left_stick_up", "Move forward"},
    {"left_stick_down", "Move back"},
    {"right_stick_left", "Look left"},
    {"right_stick_right", "Look right"},
    {"right_stick_up", "Look up"},
    {"right_stick_down", "Look down"},
};
constexpr int kControlCount = int(sizeof(kControlLabels) / sizeof(kControlLabels[0]));

void ReadModes() {
    s_modes.clear();
    HMONITOR mon = MonitorFromWindow(static_cast<HWND>(app::Window()), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) return;
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsW(mi.szDevice, i, &dm); i++) {
        if (dm.dmBitsPerPel != 32 || dm.dmPelsWidth < 640 || dm.dmPelsHeight < 480) continue;
        app::Mode m{dm.dmPelsWidth, dm.dmPelsHeight};
        bool seen = false;
        for (const app::Mode& k : s_modes)
            if (k.width == m.width && k.height == m.height) seen = true;
        if (!seen) s_modes.push_back(m);
    }
    std::sort(s_modes.begin(), s_modes.end(), [](const app::Mode& a, const app::Mode& b) {
        return a.width != b.width ? a.width > b.width : a.height > b.height;
    });
}

void SetupStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 6;
    s.FrameRounding = 4;
    s.GrabRounding = 4;
    s.ChildRounding = 4;
    s.WindowPadding = ImVec2(16, 14);
    s.FramePadding = ImVec2(10, 6);
    s.ItemSpacing = ImVec2(10, 8);
    s.WindowBorderSize = 1;
    ImVec4* c = s.Colors;
    const ImVec4 red(0.62f, 0.10f, 0.10f, 1.0f), red_hi(0.78f, 0.16f, 0.14f, 1.0f), red_lo(0.40f, 0.07f, 0.07f, 1.0f);
    c[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.05f, 0.94f);
    c[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.08f, 0.08f, 0.60f);
    c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.06f, 0.06f, 0.97f);
    c[ImGuiCol_Border] = ImVec4(0.35f, 0.12f, 0.12f, 0.60f);
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = c[ImGuiCol_TitleBgCollapsed] = red_lo;
    c[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.12f, 0.12f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.14f, 0.14f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.38f, 0.16f, 0.16f, 1.0f);
    c[ImGuiCol_Button] = red_lo;
    c[ImGuiCol_ButtonHovered] = red;
    c[ImGuiCol_ButtonActive] = red_hi;
    c[ImGuiCol_Header] = red_lo;
    c[ImGuiCol_HeaderHovered] = red;
    c[ImGuiCol_HeaderActive] = red_hi;
    c[ImGuiCol_SliderGrab] = red;
    c[ImGuiCol_SliderGrabActive] = red_hi;
    c[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
    c[ImGuiCol_NavHighlight] = ImVec4(0.95f, 0.40f, 0.30f, 1.0f);
    c[ImGuiCol_Separator] = ImVec4(0.35f, 0.12f, 0.12f, 0.60f);
    s_base_style = s;
}

// text and spacing grow with the output: sized for 1080 lines, the font loaded large and scaled down
constexpr float kFontPixels = 40, kTextPixels = 22;
void ScaleUi() {
    ImGuiIO& io = ImGui::GetIO();
    float scale = std::clamp(io.DisplaySize.y / 1080.0f, 0.6f, 3.0f);
    if (scale == s_scale) return;
    s_scale = scale;
    ImGui::GetStyle() = s_base_style;
    ImGui::GetStyle().ScaleAllSizes(scale);
    io.FontGlobalScale = io.Fonts->Fonts.Size > 1 ? kTextPixels / kFontPixels * scale : scale;
}

void Open() {
    if (s_open) return;
    s_open = true;
    // a game menu's pointer lets go of the mouse while this is up, which would look again: free it too
    s_mouse_was_captured = input::MouseCaptured() || input::Pointer();
    if (s_mouse_was_captured) input::ToggleMouse();
    const app::Settings& s = app::Get();
    s_pending = {s.display_mode, s.width, s.height};
    s_bind = -1;
    ReadModes();
    LOGR("menu: open");
}

void Close() {
    if (!s_open) return;
    if (s_confirming) {  // closing mid-countdown keeps the new mode, as Keep would
        s_confirming = false;
    }
    s_open = false;
    s_hold = true;
    s_bind = -1;
    if (s_mouse_was_captured) {
        input::ToggleMouse();
        s_mouse_was_captured = false;
    }
    if (s_dirty) {
        app::Save();
        s_dirty = false;
        LOGR("menu: settings saved");
    }
    LOGR("menu: closed");
}

void ApplyDisplay(const Display& d) {
    app::Settings& s = app::Get();
    s.display_mode = d.mode;
    s.width = d.width;
    s.height = d.height;
    app::ApplyDisplay();
    s_dirty = true;
}

std::string ModeName(uint32_t w, uint32_t h) {
    if (!w || !h) {
        app::Mode d = app::Desktop();
        char t[48];
        std::snprintf(t, sizeof(t), "Desktop (%u x %u)", d.width, d.height);
        return t;
    }
    char t[32];
    std::snprintf(t, sizeof(t), "%u x %u", w, h);
    return t;
}

void Hint(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.58f, 0.56f, 1.0f));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

// a label on the left, its control filling the rest of the row
void Row(const char* label) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.42f);
    ImGui::SetNextItemWidth(-1);
}

void PageDisplay() {
    app::Settings& s = app::Get();
    Row("Display mode");
    int mode = int(s_pending.mode);
    if (ImGui::Combo("##mode", &mode, kModeNames, 3)) s_pending.mode = app::DisplayMode(mode);
    Row("Resolution");
    if (ImGui::BeginCombo("##res", ModeName(s_pending.width, s_pending.height).c_str())) {
        if (ImGui::Selectable(ModeName(0, 0).c_str(), !s_pending.width)) s_pending.width = s_pending.height = 0;
        for (const app::Mode& m : s_modes) {
            bool on = m.width == s_pending.width && m.height == s_pending.height;
            if (ImGui::Selectable(ModeName(m.width, m.height).c_str(), on)) {
                s_pending.width = m.width;
                s_pending.height = m.height;
            }
            if (on) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    bool changed = s_pending.mode != s.display_mode || s_pending.width != s.width || s_pending.height != s.height;
    ImGui::BeginDisabled(!changed || s_confirming);
    if (ImGui::Button("Apply display mode")) {
        s_revert = {s.display_mode, s.width, s.height};
        ApplyDisplay(s_pending);
        s_confirming = true;
        s_revert_at = Clock::now() + std::chrono::seconds(15);
    }
    ImGui::EndDisabled();
    if (s_confirming) {
        long left = long(std::chrono::duration_cast<std::chrono::seconds>(s_revert_at - Clock::now()).count());
        if (left < 0) {
            ApplyDisplay(s_revert);
            s_pending = s_revert;
            s_confirming = false;
        } else {
            ImGui::SameLine();
            ImGui::Text("Keep this mode? Going back in %ld s", left);
            if (ImGui::Button("Keep")) s_confirming = false;
            ImGui::SameLine();
            if (ImGui::Button("Go back")) {
                ApplyDisplay(s_revert);
                s_pending = s_revert;
                s_confirming = false;
            }
        }
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    Row("Render resolution");
    int pct = int(s.render_percent);
    if (ImGui::SliderInt("##render", &pct, 25, 200, "%d%%")) {
        s.render_percent = uint32_t(pct / 5 * 5);
        s_dirty = true;
    }
    Hint("Of the output height. Above 100% sharpens, below it runs faster.");
    Row("Max frame rate");
    const uint32_t kCaps[] = {30, 60, 120, 144, 165, 240, 0};
    const char* kCapNames[] = {"30", "60", "120", "144", "165", "240", "Unlimited"};
    int cap = 1;
    for (int i = 0; i < 7; i++)
        if (kCaps[i] == s.fps_cap) cap = i;
    if (ImGui::Combo("##fps", &cap, kCapNames, 7)) {
        s.fps_cap = kCaps[cap];
        s_dirty = true;
    }
    if (s.fps_cap != 60) Hint("The game was made for 60. Other rates are not tested yet.");
    Row("V-Sync");
    if (ImGui::Checkbox("##vsync", &s.vsync)) s_dirty = true;
}

// 0 is the game's own; the slider runs 40-110 degrees, the far left reading as the game's
bool FovSlider(const char* id, uint32_t* deg) {
    int v = *deg ? int(*deg) : 39;
    char label[32];
    std::snprintf(label, sizeof(label), *deg ? "%u" : "Game (55)", *deg);
    if (!ImGui::SliderInt(id, &v, 39, 110, label)) return false;
    *deg = v <= 39 ? 0 : uint32_t(v);
    return true;
}

void PageGraphics() {
    app::Settings& s = app::Get();
    Row("PS2 alpha blending");
    if (ImGui::Checkbox("##ps2alpha", &g_ps2_alpha)) s_dirty = true;
    Hint("Drop alpha from presented frames, like the PS2 port: no ghosting from the previous-frame haze. Off matches the Xbox.");
    Row("Anisotropic filtering");
    const uint32_t kLevels[] = {0, 2, 4, 8, 16};
    const char* kNames[] = {"As the game asks", "2x", "4x", "8x", "16x"};
    int at = 0;
    for (int i = 0; i < 5; i++)
        if (kLevels[i] == s.aniso) at = i;
    if (ImGui::Combo("##aniso", &at, kNames, 5)) {
        s.aniso = kLevels[at];
        s_dirty = true;
    }
    Row("Anti-aliasing (MSAA)");
    const uint32_t kSamples[] = {0, 2, 4, 8};
    const char* kSampleNames[] = {"Off", "2x", "4x", "8x"};
    int ms = 0;
    for (int i = 0; i < 4; i++)
        if (kSamples[i] == s.msaa) ms = i;
    if (ImGui::Combo("##msaa", &ms, kSampleNames, 4)) {
        s.msaa = kSamples[ms];
        s_dirty = true;
    }
    Hint("Smooths the edges of the 3D picture. Costs GPU time, more with a higher render resolution.");
    Row("Precompile shaders");
    if (ImGui::Checkbox("##precompile", &s.precompile_shaders)) s_dirty = true;
    Hint("At launch, compile the shaders earlier play used when the graphics driver lacks them (a new driver, a cleared cache), so they do not stutter in the game.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    Row("Field of view");
    if (FovSlider("##fov", &g_fov)) s_dirty = true;
    Row("Weapon field of view");
    if (FovSlider("##wfov", &g_weapon_fov)) s_dirty = true;
    Hint("Degrees across a 4:3 frame; the game's is 55. The game's zooms work on top. A wide view can show a wall right beside you cut open, and a wide weapon view the ends of the arms and gun.");
    Row("Widescreen framing");
    if (ImGui::Checkbox("##wide", &g_widescreen)) s_dirty = true;
    Hint("On a wide screen, see more at the sides instead of a stretched 4:3 picture. Menus and the HUD stay stretched.");
    Row("Reticle size");
    int ret = int(g_reticle);
    if (ImGui::SliderInt("##reticle", &ret, 50, 200, "%d%%")) {
        g_reticle = uint32_t(ret / 5 * 5);
        s_dirty = true;
    }
    Row("Cut-scene borders");
    if (ImGui::Checkbox("##borders", &g_cutscene_borders)) s_dirty = true;
    Hint("The black bars of the in-level cut-scenes. Movies are not changed.");
    ImGui::Spacing();
    Hint("More graphics settings (draw distance, ambient occlusion) come later.");
}

std::string KeysOf(const char* control) {
    for (const auto& [c, keys] : input::Bindings())
        if (c == control) return keys;
    return "";
}

void SetKeys(const char* control, const std::string& keys) {
    auto& list = app::Get().keys;
    bool found = false;
    for (auto& [c, k] : list)
        if (c == control) {
            k = keys;
            found = true;
        }
    if (!found) list.emplace_back(control, keys);
    input::ReloadBindings();
    s_dirty = true;
}

// the key pressed while a control waits; Escape gives up
void PollBind() {
    if (s_bind < 0) return;
    bool any = false;
    for (int vk = 1; vk < 255; vk++)
        if (GetAsyncKeyState(vk) & 0x8000) any = true;
    if (!s_bind_released) {
        s_bind_released = !any;
        return;
    }
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
        s_bind = -1;
        return;
    }
    for (int vk = 1; vk < 255; vk++) {
        // the left and right Shift, Ctrl and Alt are named apart; the plain ones would bind both
        if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_F1) continue;
        if (!(GetAsyncKeyState(vk) & 0x8000)) continue;
        std::string name = input::KeyName(vk);
        if (name.empty()) continue;
        const char* control = kControlLabels[s_bind].first;
        std::string keys = s_bind_add ? KeysOf(control) : "";
        if (keys.find(name) == std::string::npos) keys = keys.empty() ? name : keys + "," + name;
        SetKeys(control, keys);
        LOGR("menu: %s = %s", control, keys.c_str());
        s_bind = -1;
        return;
    }
}

void PageControls() {
    app::Settings& s = app::Get();
    Row("Button prompts");
    const char* kPrompts[] = {"Controller in use", "Xbox", "PlayStation", "Switch", "Keyboard and mouse"};
    int style = int(std::min<uint32_t>(prompts::g_style, 4));
    if (ImGui::Combo("##prompts", &style, kPrompts, 5)) {
        prompts::g_style = uint32_t(style);
        s_dirty = true;
    }
    Hint("The buttons the game's prompts show. Controller in use: PlayStation or Switch pictures for those pads, the "
         "bound keys and mouse buttons while the keyboard or mouse was used last, Xbox for the rest.");
    ImGui::Spacing();
    Row("Keyboard and mouse");
    if (ImGui::Checkbox("##kb", &s.keyboard)) s_dirty = true;
    ImGui::BeginDisabled(!s.keyboard);
    Row("Mouse look");
    if (ImGui::Checkbox("##look", &s.mouse_look)) s_dirty = true;
    Row("Mouse sensitivity");
    int sens = int(s.mouse_sensitivity);
    if (ImGui::SliderInt("##sens", &sens, 10, 400, "%d%%")) {
        s.mouse_sensitivity = uint32_t(sens);
        s_dirty = true;
    }
    Row("Direct mouse aiming");
    if (ImGui::Checkbox("##direct", &g_mouse_direct)) s_dirty = true;
    Hint("On foot the mouse turns the view by how far it moves, with no stick curve or aim assist. Off: the mouse is the right stick, as on the horse and in split screen.");
    Row("Mouse in menus");
    if (ImGui::Checkbox("##mousemenus", &mouse_menus::g_on)) s_dirty = true;
    Hint("In the game's menus the cursor picks a row, left click selects it, right click goes back, the wheel steps through the rows.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Key bindings");
    Hint("Set: press a key or mouse button for it. Add: one more key. Escape gives up.");
    float col = ImGui::GetContentRegionAvail().x;
    if (ImGui::BeginTable("##keys", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthFixed, col * 0.34f);
        ImGui::TableSetupColumn("keys", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("buttons", ImGuiTableColumnFlags_WidthFixed, col * 0.30f);
        auto bindings = input::Bindings();
        for (int i = 0; i < kControlCount; i++) {
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(kControlLabels[i].second);
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            if (s_bind == i) ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1), "press a key...");
            else {
                std::string keys = KeysOf(kControlLabels[i].first);
                for (size_t at = 0; (at = keys.find(',', at)) != std::string::npos; at += 2) keys.insert(at + 1, " ");
                ImGui::TextUnformatted(keys.c_str());
            }
            ImGui::TableNextColumn();
            if (ImGui::Button("Set")) {
                s_bind = i;
                s_bind_add = false;
                s_bind_released = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add")) {
                s_bind = i;
                s_bind_add = true;
                s_bind_released = false;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("Default keys")) {
        s.keys.clear();
        input::ReloadBindings();
        s_dirty = true;
    }
    ImGui::EndDisabled();
}

void PageGame() {
    Row("Show frame rate");
    if (ImGui::Checkbox("##fps", &g_show_fps)) s_dirty = true;
    Row("War Train and Splitter grenades");
    if (ImGui::Checkbox("##bonus", &content::g_bonus)) s_dirty = true;
    Hint("Content the Xbox disc has but leaves off: the War Train chapter and the Splitter grenade pickups (D-pad up changes grenade type). Takes effect at the next start.");
}

bool s_confirm_exit = false;

void DrawMenu() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 size(std::min(io.DisplaySize.x * 0.9f, 1000 * s_scale), std::min(io.DisplaySize.y * 0.9f, 700 * s_scale));
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    if (!ImGui::Begin("Darkwatch - PC settings", nullptr,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    if (ImGui::IsWindowAppearing()) ImGui::SetWindowFocus();
    // the bumpers (and Ctrl+Tab's neighbours, PageUp/PageDown) step through the pages
    if (s_bind < 0) {
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false) || ImGui::IsKeyPressed(ImGuiKey_PageUp, false))
            s_page = (s_page + kPageCount - 1) % kPageCount;
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false) || ImGui::IsKeyPressed(ImGuiKey_PageDown, false))
            s_page = (s_page + 1) % kPageCount;
    }
    float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    float side = 180 * s_scale;
    ImGui::BeginChild("##pages", ImVec2(side, -footer), ImGuiChildFlags_Border | ImGuiChildFlags_NavFlattened);
    for (int i = 0; i < kPageCount; i++)
        if (ImGui::Selectable(kPages[i], s_page == i)) s_page = i;
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##page", ImVec2(0, -footer), ImGuiChildFlags_Border | ImGuiChildFlags_NavFlattened);
    switch (s_page) {
        case 0: PageDisplay(); break;
        case 1: PageGraphics(); break;
        case 2: PageControls(); break;
        default: PageGame(); break;
    }
    ImGui::EndChild();
    if (ImGui::Button("Resume")) Close();
    ImGui::SameLine();
    Hint("F1 / Back+Start: close    LB/RB: pages");
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("Exit to Desktop").x -
                    ImGui::GetStyle().FramePadding.x * 2);
    if (ImGui::Button("Exit to Desktop")) s_confirm_exit = true;
    if (s_confirm_exit) {
        ImGui::OpenPopup("Exit to Desktop?");
        s_confirm_exit = false;
    }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Exit to Desktop?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Progress since the last save is lost.");
        if (ImGui::Button("Exit")) {
            if (s_dirty) app::Save();
            LOGR("menu: exit to desktop");
            PostMessageW(static_cast<HWND>(app::Window()), WM_CLOSE, 0, 0);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();
}

void DrawFps(double fps) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 8 * s_scale, 8 * s_scale), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::SetNextWindowBgAlpha(0.45f);
    if (ImGui::Begin("##fpsbox", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings))
        ImGui::Text("%.0f fps", fps);
    ImGui::End();
}

void Ui(double fps) {
    ScaleUi();
    game_menu::Patch();
    prompts::Frame();
    mouse_menus::Frame();
    gpu::SetClearPresentedAlpha(g_ps2_alpha);
    if (s_game_open.exchange(false) && !s_open && !report::ReportOpen()) Open();
    bool toggle = s_pad_toggle.exchange(false);
    if (s_bind < 0 && !report::ReportOpen() && ImGui::IsKeyPressed(ImGuiKey_F1, false)) toggle = true;
    if (toggle) {
        if (s_open) Close();
        else if (!report::ReportOpen()) Open();
    }
    if (s_open) {
        PollBind();
        bool popup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
        if (s_bind < 0 && !popup &&
            (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)))
            Close();
    }
    if (s_open) DrawMenu();
    if (g_show_fps) DrawFps(fps);
    report::ReportUi(fps);
}

bool AnyInput(const input::Pad& p) {
    if (p.buttons) return true;
    for (uint8_t a : p.analog)
        if (a > 30) return true;
    auto off = [](int16_t v) { return v > 8000 || v < -8000; };
    return off(p.lx) || off(p.ly) || off(p.rx) || off(p.ry);
}

constexpr uint16_t kStart = 1 << 4, kBack = 1 << 5;

void Pad(int port, input::Pad& pad) {
    report::ReportPad(port, pad);
    mouse_menus::Pad(port, pad);
    if (port == 0) {
        static bool combo = false;
        bool now = (pad.buttons & (kStart | kBack)) == (kStart | kBack);
        if (now && !combo) s_pad_toggle = true;
        combo = now;
        if (now) pad.buttons &= uint16_t(~(kStart | kBack));
    }
    if (s_hold && port == 0 && !AnyInput(pad)) s_hold = false;
    if (s_open || s_hold) {
        bool connected = pad.connected;
        pad = input::Pad{};
        pad.connected = connected;
    }
}

void Window(void* hwnd) {
    // Segoe UI, loaded large and drawn scaled (ScaleUi); ImGui's own font when it is missing
    wchar_t dir[MAX_PATH];
    if (GetWindowsDirectoryW(dir, MAX_PATH)) {
        std::string path;
        for (const wchar_t* c = dir; *c; c++) path += char(*c);
        path += "\\Fonts\\segoeui.ttf";
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            ImFontConfig cfg;
            cfg.OversampleH = 2;
            ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), kFontPixels, &cfg);
            ImGui::GetIO().Fonts->AddFontDefault();
        }
    }
    SetupStyle();
    report::ReportWindow(hwnd);
}

}  // namespace

bool MenuOpen() { return s_open; }
void OpenFromGame() { s_game_open = true; }
void MenuWindow(void* hwnd) { Window(hwnd); }
bool MenuMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    if (msg == WM_MOUSEWHEEL && !s_open) mouse_menus::Wheel(GET_WHEEL_DELTA_WPARAM(wp));
    return report::ReportMessage(hwnd, msg, wp, lp, result);
}
void MenuUi(double fps) { Ui(fps); }
void MenuPad(int port, input::Pad& pad) { Pad(port, pad); }

}  // namespace menu
