#include <cstdint>

#include "app/app.h"
#include "core/input.h"

// src/common/menu.cpp: the F1 settings menu; it passes each hook on to the F4 report box (report.cpp)
namespace menu {
extern bool g_show_fps;
extern bool g_ps2_alpha;
void MenuWindow(void* hwnd);
bool MenuMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
void MenuUi(double fps);
void MenuPad(int port, input::Pad& pad);
}  // namespace menu

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
                {"ps2_alpha", &menu::g_ps2_alpha, nullptr}},
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = nullptr,                      // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = menu::MenuWindow,           // the game window is up, before boot
    .message = menu::MenuMessage,         // window messages first; true takes one
    .ui = menu::MenuUi,                   // ImGui drawing each frame
    .pad = menu::MenuPad,                 // changes a polled pad
    .rumble = nullptr,                    // changes a rumble before it is set
};
