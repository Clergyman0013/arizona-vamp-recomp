// Button prompts for the controller in use. The game's prompts ("^MenuSelect SELECT") draw a button as a lower-case
// character of its own font (sub_001EA201: an action's button through the table at 0x43BBB0; ordinary text is made
// upper case, so a-z are free): 'a' X, 'b' Y, 'c' A, 'd' B. Both copies of the font, white and the red of a picked
// row, are 256x256 P8 textures with those four pictures side by side at the same place. As the renderer decodes
// them, the four cells are drawn again for a PlayStation or Switch pad, by position: the bottom button is the
// game's A. The pictures are drawn here, shapes and letters, nothing taken from elsewhere.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include "core/input.h"
#include "core/log.h"
#include "gpu/texture.h"

namespace prompts {

uint32_t g_style = 0;  // settings.toml button_prompts: 0 auto, 1 Xbox, 2 PlayStation, 3 Switch

namespace {

enum Style : uint32_t { kAuto, kXbox, kPlayStation, kSwitch };

constexpr uint32_t kFmtP8 = 0x0B, kSize = 256;
// the cells, left edge, 13 x 14 texels from row 35, in the font's order X Y A B
constexpr uint32_t kTop = 35, kW = 13, kH = 14;
constexpr uint32_t kLeft[4] = {71, 93, 115, 138};
enum Cell { kWest, kNorth, kSouth, kEast };  // the pad positions of X Y A B

std::atomic<uint32_t> s_drawn{kXbox};  // what the decode draws
uint32_t s_seen = kXbox;               // what the textures were last decoded with
std::mutex s_lock;
std::vector<uint32_t> s_fonts;  // the font textures' data addresses

struct Rgb {
    float r, g, b;
};

// pixel (x, y) of the image: B G R A
uint8_t* At(uint8_t* px, uint32_t pitch, uint32_t x, uint32_t y) { return px + size_t(y) * pitch + size_t(x) * 4; }

// the font, by the colours of its four pictures: blue X, yellow Y, green A, red B
bool IsFont(uint8_t* px, uint32_t pitch) {
    for (int c = 0; c < 4; c++) {
        int n = 0;
        for (uint32_t y = kTop; y < kTop + kH; y++)
            for (uint32_t x = kLeft[c]; x < kLeft[c] + kW; x++) {
                const uint8_t* p = At(px, pitch, x, y);
                int b = p[0], g = p[1], r = p[2];
                if (p[3] < 128) continue;
                bool hit = c == 0 ? b > r + 40 : c == 1 ? r > b + 60 && g > b + 60 : c == 2 ? g > r + 30 && g > b + 20 : r > g + 60 && r > b + 60;
                n += hit;
            }
        if (n < 12) return false;
    }
    return true;
}

float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

// distance from p to the segment a-b
float Segment(float px, float py, float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay;
    float t = Clamp01(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy));
    return std::hypot(px - ax - t * dx, py - ay - t * dy);
}

// 5x7 letters for the Switch pad, a row a string
const char* const kLetters[4][7] = {
    {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."},  // west Y
    {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"},  // north X
    {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."},  // south B
    {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"},  // east A
};

// how much of the symbol covers the point, the cell's centre at 0, 0 (texels)
float Symbol(uint32_t style, int cell, float x, float y) {
    constexpr float kLine = 0.65f;  // half the stroke
    if (style == kSwitch) {
        int col = int(std::floor(x + 2.5f)), row = int(std::floor(y + 3.5f));
        return col >= 0 && col < 5 && row >= 0 && row < 7 && kLetters[cell][row][col] == '#';
    }
    float d;
    switch (cell) {
        case kSouth:  // cross
            d = std::min(Segment(x, y, -2.6f, -2.6f, 2.6f, 2.6f), Segment(x, y, -2.6f, 2.6f, 2.6f, -2.6f));
            break;
        case kEast:  // circle
            d = std::fabs(std::hypot(x, y) - 2.8f);
            break;
        case kWest:  // square
            d = std::fabs(std::max(std::fabs(x), std::fabs(y)) - 2.5f);
            break;
        default: {  // triangle, point up
            const float ax = 0, ay = -3.0f, bx = -3.0f, by = 2.2f, cx = 3.0f, cy = 2.2f;
            d = std::min({Segment(x, y, ax, ay, bx, by), Segment(x, y, bx, by, cx, cy), Segment(x, y, cx, cy, ax, ay)});
            break;
        }
    }
    return d <= kLine;
}

void DrawCell(uint8_t* px, uint32_t pitch, uint32_t style, int cell) {
    // PlayStation: the symbols in their usual colours; Switch: white letters
    static const Rgb kPsColours[4] = {{0.91f, 0.60f, 0.85f}, {0.30f, 0.85f, 0.70f}, {0.52f, 0.70f, 0.96f}, {0.96f, 0.42f, 0.42f}};
    const Rgb ink = style == kSwitch ? Rgb{0.97f, 0.97f, 0.97f} : kPsColours[cell];
    const Rgb disc{0.12f, 0.12f, 0.13f}, rim{0.45f, 0.45f, 0.47f};
    constexpr float kRadius = 6.45f, kRim = 0.9f;
    constexpr int kSub = 4;  // samples per texel and axis
    const float cx = float(kLeft[cell]) + kW / 2.0f, cy = float(kTop) + kH / 2.0f;
    for (uint32_t y = kTop; y < kTop + kH; y++)
        for (uint32_t x = kLeft[cell]; x < kLeft[cell] + kW; x++) {
            float r = 0, g = 0, b = 0, a = 0;
            for (int sy = 0; sy < kSub; sy++)
                for (int sx = 0; sx < kSub; sx++) {
                    float fx = float(x) + (sx + 0.5f) / kSub - cx, fy = float(y) + (sy + 0.5f) / kSub - cy;
                    float dist = std::hypot(fx, fy);
                    if (dist > kRadius) continue;
                    // letters sit on whole texels: their grid is offset by half a texel up
                    float sym = Symbol(style, cell, fx, style == kSwitch ? fy + 0.5f : fy);
                    const Rgb& c = sym > 0 ? ink : dist > kRadius - kRim ? rim : disc;
                    r += c.r;
                    g += c.g;
                    b += c.b;
                    a += 1;
                }
            uint8_t* p = At(px, pitch, x, y);
            constexpr float kSamples = kSub * kSub;
            if (a > 0) {  // straight alpha: the colour of the covered part
                p[0] = uint8_t(b / a * 255 + 0.5f);
                p[1] = uint8_t(g / a * 255 + 0.5f);
                p[2] = uint8_t(r / a * 255 + 0.5f);
            } else {
                p[0] = p[1] = p[2] = uint8_t(disc.r * 255);
            }
            p[3] = uint8_t(a / kSamples * 255 + 0.5f);
        }
}

uint32_t Wanted() {
    if (g_style != kAuto) return std::min<uint32_t>(g_style, kSwitch);
    switch (input::PadFamily(0)) {
        case input::Family::PlayStation: return kPlayStation;
        case input::Family::Switch: return kSwitch;
        default: return kXbox;
    }
}

}  // namespace

// render thread, each texture level decoded
void Texture(uint32_t data, uint32_t format, uint32_t width, uint32_t height, uint32_t level, uint8_t* bgra, uint32_t pitch) {
    if (format != kFmtP8 || width != kSize || height != kSize || level != 0) return;
    if (!IsFont(bgra, pitch)) return;
    {
        std::lock_guard lock(s_lock);
        if (std::find(s_fonts.begin(), s_fonts.end(), data) == s_fonts.end()) {
            s_fonts.push_back(data);
            LOGR("prompts: button font at %08X", data);
        }
    }
    uint32_t style = s_drawn;
    if (style == kXbox) return;
    for (int c = 0; c < 4; c++) DrawCell(bgra, pitch, style, c);
}

// each frame: a new pad or setting has the fonts decoded again
void Frame() {
    uint32_t want = Wanted();
    if (want == s_seen) return;
    s_seen = want;
    s_drawn = want;
    static const char* const kNames[] = {"auto", "Xbox", "PlayStation", "Switch"};
    LOGR("prompts: %s buttons", kNames[want]);
    std::lock_guard lock(s_lock);
    for (uint32_t va : s_fonts) gpu::MarkDirtyData(va);
}

}  // namespace prompts
