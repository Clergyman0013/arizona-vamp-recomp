#include <cstdint>
#include <string>

#include "app/app.h"
#include "core/input.h"

// src/common/menu.cpp: the F1 settings menu; it passes each hook on to the F4 report box (report.cpp)
namespace menu {
extern bool g_show_fps;
extern bool g_ps2_alpha;
extern uint32_t g_fov, g_weapon_fov, g_reticle;
extern bool g_widescreen, g_mouse_direct, g_cutscene_borders;
void MenuWindow(void* hwnd);
bool MenuMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
void MenuUi(double fps);
void MenuPad(int port, input::Pad& pad);
}  // namespace menu

// src/common/content.cpp: the War Train and Splitter selectors, patched as the packs are read
namespace content {
extern bool g_bonus;
void Read(const std::string& xbox, uint64_t pos, uint8_t* data, uint32_t len);
}  // namespace content

// src/common/prompts.cpp: PlayStation and Switch pictures in the game's button prompts
namespace prompts {
extern uint32_t g_style;
void Texture(uint32_t data, uint32_t format, uint32_t width, uint32_t height, uint32_t level, uint8_t* bgra, uint32_t pitch);
}  // namespace prompts

namespace mouse_menus {
extern bool g_on;  // src/game/mouse_menus.cpp
}

// what the runtime asks of the game; strings must be set, hooks may stay null
// Darkwatch's defaults, before the settings file loads over them: 16x anisotropic filtering
namespace {
[[maybe_unused]] const bool kDefaults = [] {
    app::Get().aniso = 16;
    return true;
}();
}  // namespace

const app::GameInfo app::kGame = {
    .title = L"Darkwatch",               // window and splash
    .short_title = L"Darkwatch",               // message boxes
    .name = L"Darkwatch",                      // window classes, crash dumps, handover event
    .log = "darkwatch",                        // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "Darkwatch"}},  // each recompiled xbe and its exe, for title launches
    .options = {{"show_fps", &menu::g_show_fps, nullptr},  // game settings kept in the settings file
                {"ps2_alpha", &menu::g_ps2_alpha, nullptr},
                {"fov", nullptr, &menu::g_fov},
                {"weapon_fov", nullptr, &menu::g_weapon_fov},
                {"widescreen_fov", &menu::g_widescreen, nullptr},
                {"reticle_size", nullptr, &menu::g_reticle},
                {"cutscene_borders", &menu::g_cutscene_borders, nullptr},
                {"mouse_direct_aim", &menu::g_mouse_direct, nullptr},
                {"bonus_content", &content::g_bonus, nullptr},
                {"button_prompts", nullptr, &prompts::g_style},
                {"mouse_menus", &mouse_menus::g_on, nullptr}},
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = nullptr,                      // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = menu::MenuWindow,           // the game window is up, before boot
    .message = menu::MenuMessage,         // window messages first; true takes one
    .ui = menu::MenuUi,                   // ImGui drawing each frame
    .pad = menu::MenuPad,                 // changes a polled pad
    .rumble = nullptr,                    // changes a rumble before it is set
    .read = content::Read,                // changes what a game file read returns
    .texture = prompts::Texture,          // changes a decoded texture
};
