// F4 bug reports: a note box (ImGui) plus the frame from when F4 was pressed
// plus the log tail, saved to reports\<YYYY-MM-DD_HHMMSS>\ beside the exe.
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
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <imgui.h>

#include "app/app.h"
#include "core/diag.h"
#include "core/input.h"
#include "core/log.h"
#include "gpu/backend.h"

namespace menu {
bool MenuOpen();
}

namespace report {
namespace {

std::atomic<bool> s_open{false};    // the box is up; the game gets no input
std::atomic<bool> s_want_open{false};  // F4 asked for the box (window or render thread)
int s_skip = 0;                     // ui frames to hold the box back after F4, so the capture has none of it
char s_note[2048] = {};
bool s_first_show = false;
bool s_mouse_was_captured = false;
std::vector<std::string> s_dir_snapshot;  // capture folders present when F4 landed
std::chrono::steady_clock::time_point s_launch{};
std::chrono::steady_clock::time_point s_saved_until{};
std::string s_saved_name;

std::string Stamp() {
    SYSTEMTIME tm;
    GetLocalTime(&tm);
    char s[32];
    std::snprintf(s, sizeof(s), "%04u-%02u-%02u_%02u%02u%02u", tm.wYear, tm.wMonth, tm.wDay, tm.wHour,
                  tm.wMinute, tm.wSecond);
    return s;
}

std::filesystem::path CaptureParent() { return app::ExeDir() / diag::CaptureDir(); }

std::vector<std::string> ListDirs(const std::filesystem::path& parent) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(parent, ec)) {
        if (e.is_directory(ec)) out.push_back(e.path().filename().string());
    }
    return out;
}

// the F4 capture's folder: the one that was not there when F4 landed (ours triggers first,
// so with several new ones the earliest is ours)
std::filesystem::path FindCapture() {
    std::error_code ec;
    std::filesystem::path best;
    std::filesystem::file_time_type best_t = std::filesystem::file_time_type::max();
    for (const auto& e : std::filesystem::directory_iterator(CaptureParent(), ec)) {
        if (!e.is_directory(ec)) continue;
        bool known = false;
        for (const auto& k : s_dir_snapshot)
            if (k == e.path().filename().string()) {
                known = true;
                break;
            }
        if (known) continue;
        std::error_code ec2;
        auto t = std::filesystem::last_write_time(e.path(), ec2);
        if (!ec2 && t < best_t) {
            best_t = t;
            best = e.path();
        }
    }
    return best;
}

void Close() {
    s_open = false;
    if (s_mouse_was_captured) {
        input::ToggleMouse();
        s_mouse_was_captured = false;
    }
}

void OpenBox() {
    if (s_open) return;
    s_open = true;
    s_skip = 2;  // the capture below records the next present; the box waits two frames
    s_note[0] = 0;
    s_first_show = true;
    s_mouse_was_captured = input::MouseCaptured();
    if (s_mouse_was_captured) input::ToggleMouse();
    s_dir_snapshot = ListDirs(CaptureParent());
    gpu::g_capture = true;
}

std::string TailLines(const std::string& path, size_t n) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return "(no log file)\n";
    long long pos = (long long)in.tellg();
    const size_t kChunk = 65536;
    std::string buf;
    size_t lines = 0;
    while (pos > 0 && lines <= n) {
        size_t take = (size_t)(pos > (long long)kChunk ? kChunk : pos);
        pos -= take;
        in.seekg(pos);
        std::string chunk(take, 0);
        in.read(chunk.data(), (std::streamsize)take);
        buf = chunk + buf;
        lines = 0;
        for (char c : buf)
            if (c == '\n') lines++;
    }
    size_t start = 0, seen = 0;
    for (size_t i = buf.size(); i-- > 0;) {
        if (buf[i] == '\n') {
            if (++seen > n) {
                start = i + 1;
                break;
            }
        }
    }
    return buf.substr(start);
}

void Save() {
    const std::string stamp = Stamp();
    std::error_code ec;
    std::filesystem::path dir = app::ExeDir() / "reports" / stamp;
    std::filesystem::create_directories(dir, ec);
    SYSTEMTIME tm;
    GetLocalTime(&tm);
    char when[64];
    std::snprintf(when, sizeof(when), "%04u-%02u-%02u %02u:%02u:%02u", tm.wYear, tm.wMonth, tm.wDay,
                  tm.wHour, tm.wMinute, tm.wSecond);
    long long secs =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - s_launch).count();
    {
        std::ofstream out(dir / "note.txt", std::ios::binary | std::ios::trunc);
        if (out) {
            out << s_note << "\n\nreported: " << when << " local\nseconds since launch: " << secs << "\n";
        }
    }
    if (std::filesystem::path cap = FindCapture(); !cap.empty()) {
        for (const auto& e : std::filesystem::directory_iterator(cap, ec)) {
            std::filesystem::rename(e.path(), dir / e.path().filename(), ec);
            ec.clear();
        }
        std::filesystem::remove(cap, ec);
    }
    {
        std::ofstream out(dir / "log.txt", std::ios::binary | std::ios::trunc);
        if (out) out << TailLines(logx::Path(), 300);
    }
    std::filesystem::copy_file(app::ExeDir() / "settings.toml", dir / "settings.toml",
                               std::filesystem::copy_options::overwrite_existing, ec);
    LOGR("report: saved %s", (std::string("reports\\") + stamp).c_str());
    s_saved_name = "reports\\" + stamp;
    s_saved_until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    Close();
}

void Cancel() {
    if (std::filesystem::path cap = FindCapture(); !cap.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(cap, ec);
    }
    Close();
}

void Ui(double) {
    if (s_want_open.exchange(false) && !s_open && !menu::MenuOpen()) OpenBox();
    // F4 reaches here too when the box is down (the window thread may miss it while ImGui eats keys)
    if (!s_open && !menu::MenuOpen() && ImGui::IsKeyPressed(ImGuiKey_F4, false)) OpenBox();
    if (!s_open) {
        if (std::chrono::steady_clock::now() < s_saved_until) {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(16, io.DisplaySize.y - 48), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(420, 32), ImGuiCond_Always);
            if (ImGui::Begin("##saved", nullptr,
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoScrollbar)) {
                ImGui::TextUnformatted(("Report saved: " + s_saved_name).c_str());
            }
            ImGui::End();
        }
        return;
    }
    if (s_skip > 0) {
        s_skip--;
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    float k = std::clamp(io.DisplaySize.y / 1080.0f, 0.6f, 3.0f);  // the menu's ui scale
    ImGui::SetNextWindowSize(ImVec2(560 * k, 360 * k), ImGuiCond_Always);
    if (ImGui::Begin("Report a problem", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        ImGui::TextUnformatted("Describe what you saw. The game gets no input while this is open.");
        if (s_first_show) {
            ImGui::SetKeyboardFocusHere();
            s_first_show = false;
        }
        ImGui::InputTextMultiline("##note", s_note, sizeof(s_note), ImVec2(-1, 200 * k));
        if (ImGui::Button("Save (Ctrl+Enter)")) Save();
        ImGui::SameLine();
        if (ImGui::Button("Cancel (Esc)")) Cancel();
    }
    ImGui::End();
    if (!s_open) return;  // Save/Cancel closed it above
    bool ctrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) Save();
    else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        Cancel();
}

bool Message(void*, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t*) {
    if (msg == WM_KEYDOWN && wp == VK_F4 && !(lp & (1 << 30)) && !s_open) s_want_open = true;
    return false;
}

void Window(void*) {
    s_launch = std::chrono::steady_clock::now();
    // reports capture without XBE_CAPTURE_DIR (a player will not set it); run_capture.bat
    // already sets it, so this changes nothing for QA. Read before diag::CaptureDir's
    // once-only static below initialises.
#ifdef _WIN32
    if (_wgetenv(L"XBE_CAPTURE_DIR") == nullptr) _wputenv(L"XBE_CAPTURE_DIR=captures");
#else
    if (!std::getenv("XBE_CAPTURE_DIR")) setenv("XBE_CAPTURE_DIR", "captures", 1);
#endif
}

void Pad(int, input::Pad& pad) {
    // zero the inputs but keep the port connected: a fully zeroed pad (connected
    // false) makes the game stop with "reconnect the controller"
    if (s_open) {
        bool connected = pad.connected;
        pad = input::Pad{};
        pad.connected = connected;
    }
}

}  // namespace

// the game's own REPORT A PROBLEM row: the same box F4 opens
void Open() { OpenBox(); }
bool ReportOpen() { return s_open; }
void ReportWindow(void* hwnd) { Window(hwnd); }
bool ReportMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    return Message(hwnd, msg, wp, lp, result);
}
void ReportUi(double fps) { Ui(fps); }
void ReportPad(int port, input::Pad& pad) { Pad(port, pad); }

}  // namespace report
