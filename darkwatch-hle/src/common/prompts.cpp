// Button prompts for the controller in use. The game's prompts ("^MenuSelect SELECT") draw a button as a lower-case
// character of its own font (sub_001EA201: an action's button through the table at 0x43BBB0; ordinary text is made
// upper case, so a-z are free): 'a' X, 'b' Y, 'c' A, 'd' B. Both copies of the font, white and the red of a picked
// row, are 256x256 P8 textures with those four pictures side by side at the same place. As the renderer decodes
// them, the four cells are drawn again for a PlayStation or Switch pad, by position: the bottom button is the
// game's A. The pictures are drawn here, shapes and letters, nothing taken from elsewhere.
//
// The in-level hints ("USE X TO MELEE ATTACK THE DOOR LOCK") draw from a second font, also 256x256 P8, whose same
// four pictures are smaller and further left (centres x 32.5, 54.5, 75, 96 on row 40, radius 5); it gets the same
// treatment, the symbols scaled to its size.
//
// After the four buttons both fonts have the triggers as boxes marked R and L and the white and black buttons
// (the menu font has no black one on that row). With a keyboard and mouse every one of these shows the first key
// the player bound to it (settings [keys]): a letter or two, an arrow, or a mouse with the bound button lit. On a
// PlayStation or Switch pad the trigger boxes read R2 / L2 or ZR / ZL.
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "core/input.h"
#include "core/log.h"
#include "gpu/texture.h"

namespace prompts {

uint32_t g_style = 0;  // settings.toml button_prompts: 0 auto, 1 Xbox, 2 PlayStation, 3 Switch, 4 keyboard and mouse

namespace {

enum Style : uint32_t { kAuto, kXbox, kPlayStation, kSwitch, kKeys };

constexpr uint32_t kFmtP8 = 0x0B, kSize = 256;

// the pictures a font has, the four buttons first in the font's order X Y A B (pad positions west north south east)
enum Slot { kWest, kNorth, kSouth, kEast, kRight, kLeft, kWhite, kBlack, kSlots };
// the controls bound to each, as the settings file names them
const char* const kSlotControl[kSlots] = {"x", "y", "a", "b", "right_trigger", "left_trigger", "white", "black"};

// a picture: its centre, and its half width and height (a round button's are its radius); hw 0 for none
struct Pic {
    float cx, cy, hw, hh;
    bool round;
};
struct Layout {
    Pic pic[kSlots];
    bool whole_cell;  // the menu font: a button's whole 13 x 14 cell is drawn, outside the button clear
};
// the menu font: button cells 13 x 14 texels from row 35, left edges 71, 93, 115, 138; R 159-167, white 174-186,
// L 227-236
constexpr Layout kMenuFont{{{77.5f, 42.0f, 6.45f, 6.45f, true},
                            {99.5f, 42.0f, 6.45f, 6.45f, true},
                            {121.5f, 42.0f, 6.45f, 6.45f, true},
                            {144.5f, 42.0f, 6.45f, 6.45f, true},
                            {163.5f, 42.0f, 4.5f, 6.0f, false},
                            {232.0f, 42.0f, 5.0f, 6.0f, false},
                            {180.5f, 42.5f, 6.5f, 6.5f, true},
                            {0, 0, 0, 0, true}},
                           true};
// the hint font: R 110-118, white 125-136, L 176-184, black 192-204, rows 34-45
constexpr Layout kHintFont{{{32.5f, 40.0f, 5.0f, 5.0f, true},
                            {54.5f, 40.0f, 5.0f, 5.0f, true},
                            {75.0f, 40.0f, 5.0f, 5.0f, true},
                            {96.0f, 40.0f, 5.0f, 5.0f, true},
                            {114.5f, 40.0f, 4.5f, 6.0f, false},
                            {180.5f, 40.0f, 4.5f, 6.0f, false},
                            {131.0f, 40.0f, 6.0f, 6.0f, true},
                            {198.5f, 40.0f, 6.5f, 6.5f, true}},
                           false};
constexpr float kUnit = 6.45f;  // the symbols below are drawn for a button of this radius, scaled to each picture

// what a key picture shows
struct Label {
    std::string text;  // one or two characters
    int mouse = 0;     // 1 left, 2 right, 3 middle button of a mouse
    int arrow = 0;     // 1 up, 2 down, 3 left, 4 right
    bool operator==(const Label&) const = default;
};

std::atomic<uint32_t> s_drawn{kXbox};  // what the decode draws
uint32_t s_seen = kXbox;               // what the textures were last decoded with
std::mutex s_lock;
std::vector<uint32_t> s_fonts;  // the font textures' data addresses
Label s_labels[kSlots];         // the keyboard's, under s_lock

struct Rgb {
    float r, g, b;
};

// pixel (x, y) of the image: B G R A
uint8_t* At(uint8_t* px, uint32_t pitch, uint32_t x, uint32_t y) { return px + size_t(y) * pitch + size_t(x) * 4; }

// the font, by the colours of its four pictures: blue X, yellow Y, green A, red B
bool IsFont(uint8_t* px, uint32_t pitch, const Layout& l) {
    for (int c = 0; c < 4; c++) {
        const Pic& p = l.pic[c];
        int n = 0;
        uint32_t x0 = uint32_t(p.cx - p.hw), y0 = uint32_t(p.cy - p.hh);
        uint32_t x1 = uint32_t(p.cx + p.hw), y1 = uint32_t(p.cy + p.hh);
        for (uint32_t y = y0; y <= y1; y++)
            for (uint32_t x = x0; x <= x1; x++) {
                const uint8_t* q = At(px, pitch, x, y);
                int b = q[0], g = q[1], r = q[2];
                if (q[3] < 128) continue;
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

// 3x5 letters and digits for keys and trigger names, the five rows one after another
const char* Glyph(char c) {
    static const char* const kAz[26] = {
        "010101111101101", "110101110101110", "011100100100011", "110101101101110", "111100110100111",
        "111100110100100", "011100101101011", "101101111101101", "111010010010111", "001001001101010",
        "101101110101101", "100100100100111", "101111111101101", "110101101101101", "010101101101010",
        "110101110100100", "010101101110011", "110101110101101", "011100010001110", "111010010010010",
        "101101101101111", "101101101101010", "101101111111101", "101101010101101", "101101010010010",
        "111001010100111"};
    static const char* const kDigits[10] = {"111101101101111", "010110010010111", "110001010100111", "110001010001110",
                                            "101101111001001", "111100110001110", "011100111101111", "111001010010010",
                                            "111101111101111", "111101111001110"};
    if (c >= 'A' && c <= 'Z') return kAz[c - 'A'];
    if (c >= '0' && c <= '9') return kDigits[c - '0'];
    if (c == ',') return "000000000010100";
    if (c == '.') return "000000000000010";
    return "000000000000000";
}

// a line of one or two characters, centred on 0, 0
bool Text(const std::string& s, float x, float y) {
    const float p = s.size() > 1 ? 1.1f : 1.6f;  // a font pixel, in symbol units
    const float w = (4.0f * float(s.size()) - 1.0f) * p, h = 5.0f * p;
    int col = int(std::floor((x + w / 2) / p)), row = int(std::floor((y + h / 2) / p));
    if (col < 0 || row < 0 || row >= 5 || col % 4 == 3) return false;
    size_t ch = size_t(col / 4);
    return ch < s.size() && Glyph(s[ch])[row * 3 + col % 4] == '1';
}

// an arrow head pointing up, down, left or right
bool Arrow(int dir, float x, float y) {
    if (dir == 2) y = -y;
    if (dir == 3 || dir == 4) std::swap(x, y);
    if (dir == 4) y = -y;
    // up: apex at (0, -3.5), base from (-3.5, 2.5) to (3.5, 2.5)
    return y <= 2.5f && y >= -3.5f && std::fabs(x) <= (y + 3.5f) * 3.5f / 6.0f;
}

// a mouse, body and buttons, with one button lit: 1 the mouse's line, 2 the lit button
int Mouse(int lit, float x, float y) {
    constexpr float rx = 3.4f, ry = 4.9f, line = 0.75f;
    float e = std::hypot(x / rx, y / ry);
    if (e > 1.0f) return 0;
    bool top = y < -0.6f;
    bool lit_here = top && ((lit == 1 && x < -0.4f) || (lit == 2 && x > 0.4f) || (lit == 3 && std::fabs(x) < 0.8f && y > -3.4f));
    if (lit_here) return 2;
    if (e > 1.0f - line / ry || (top && std::fabs(x) < 0.4f) || std::fabs(y + 0.6f) < 0.4f) return 1;
    return 0;
}

// what covers the point, the picture's centre at 0, 0 (symbol units): 0 the background, 1 ink, 2 a lit button
int Symbol(uint32_t style, int slot, const Label& key, float x, float y) {
    constexpr float kLine = 0.65f;  // half the stroke
    if (style == kKeys) {
        if (key.mouse) return Mouse(key.mouse, x, y);
        if (key.arrow) return Arrow(key.arrow, x, y);
        return Text(key.text, x, y);
    }
    if (slot >= kRight) return Text(style == kSwitch ? (slot == kRight ? "ZR" : "ZL") : (slot == kRight ? "R2" : "L2"), x, y);
    if (style == kSwitch) {
        int col = int(std::floor(x + 2.5f)), row = int(std::floor(y + 3.5f));
        return col >= 0 && col < 5 && row >= 0 && row < 7 && kLetters[slot][row][col] == '#';
    }
    float d;
    switch (slot) {
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

void DrawPic(uint8_t* px, uint32_t pitch, uint32_t style, int slot, const Layout& l, const Label& key) {
    const Pic& p = l.pic[slot];
    // PlayStation: the symbols in their usual colours; Switch, keys and trigger names: white
    static const Rgb kPsColours[4] = {{0.91f, 0.60f, 0.85f}, {0.30f, 0.85f, 0.70f}, {0.52f, 0.70f, 0.96f}, {0.96f, 0.42f, 0.42f}};
    const Rgb ink = style == kPlayStation && slot < kRight ? kPsColours[slot] : Rgb{0.97f, 0.97f, 0.97f};
    const Rgb lit{0.96f, 0.42f, 0.42f};
    const Rgb disc{0.12f, 0.12f, 0.13f}, rim{0.45f, 0.45f, 0.47f};
    const float size = std::min(p.hw, p.hh), rim_width = 0.9f * size / kUnit, scale = kUnit / size;
    constexpr int kSub = 4;  // samples per texel and axis
    // the menu font's whole button cell; elsewhere only the texels the picture covers, the rest of the font stays
    const bool whole = l.whole_cell && slot < kRight;
    uint32_t x0 = whole ? uint32_t(p.cx - 6.5f) : uint32_t(std::floor(p.cx - p.hw));
    uint32_t y0 = whole ? uint32_t(p.cy - 7.0f) : uint32_t(std::floor(p.cy - p.hh));
    uint32_t x1 = whole ? x0 + 13 : uint32_t(std::ceil(p.cx + p.hw));
    uint32_t y1 = whole ? y0 + 14 : uint32_t(std::ceil(p.cy + p.hh));
    for (uint32_t y = y0; y < y1; y++)
        for (uint32_t x = x0; x < x1; x++) {
            float r = 0, g = 0, b = 0, a = 0;
            for (int sy = 0; sy < kSub; sy++)
                for (int sx = 0; sx < kSub; sx++) {
                    float fx = float(x) + (sx + 0.5f) / kSub - p.cx, fy = float(y) + (sy + 0.5f) / kSub - p.cy;
                    // inside the picture, and how far from its edge
                    float edge;
                    if (p.round) {
                        float dist = std::hypot(fx, fy);
                        if (dist > p.hw) continue;
                        edge = p.hw - dist;
                    } else {
                        if (std::fabs(fx) > p.hw || std::fabs(fy) > p.hh) continue;
                        edge = std::min(p.hw - std::fabs(fx), p.hh - std::fabs(fy));
                    }
                    // letters sit on whole texels in the menu font's button: their grid is offset by half a texel up
                    float ux = fx * scale, uy = fy * scale;
                    int sym = Symbol(style, slot, key, ux, style == kSwitch && slot < kRight ? uy + 0.5f : uy);
                    const Rgb& c = sym == 2 ? lit : sym ? ink : edge < rim_width ? rim : disc;
                    r += c.r;
                    g += c.g;
                    b += c.b;
                    a += 1;
                }
            if (a == 0 && !whole) continue;
            uint8_t* q = At(px, pitch, x, y);
            constexpr float kSamples = kSub * kSub;
            if (a > 0) {  // straight alpha: the colour of the covered part
                q[0] = uint8_t(b / a * 255 + 0.5f);
                q[1] = uint8_t(g / a * 255 + 0.5f);
                q[2] = uint8_t(r / a * 255 + 0.5f);
            } else {
                q[0] = q[1] = q[2] = uint8_t(disc.r * 255);
            }
            q[3] = uint8_t(a / kSamples * 255 + 0.5f);
        }
}

// a key as the settings file names it, as its picture shows it
Label LabelFor(std::string name) {
    Label k;
    if (name == "Mouse1") k.mouse = 1;
    else if (name == "Mouse2") k.mouse = 2;
    else if (name == "Mouse3") k.mouse = 3;
    else if (name == "Up") k.arrow = 1;
    else if (name == "Down") k.arrow = 2;
    else if (name == "Left") k.arrow = 3;
    else if (name == "Right") k.arrow = 4;
    if (k.mouse || k.arrow) return k;
    static const char* const kShort[][2] = {
        {"Space", "SP"},  {"Enter", "EN"},  {"Escape", "ES"},   {"Tab", "TB"},    {"Backspace", "BS"}, {"LShift", "SH"},
        {"RShift", "SH"}, {"Shift", "SH"},  {"LCtrl", "CT"},    {"RCtrl", "CT"},  {"Ctrl", "CT"},      {"LAlt", "AL"},
        {"RAlt", "AL"},   {"Alt", "AL"},    {"Insert", "IN"},   {"Delete", "DL"}, {"Home", "HM"},      {"End", "ED"},
        {"PageUp", "PU"}, {"PageDown", "PD"}, {"Mouse4", "M4"}, {"Mouse5", "M5"}, {"Comma", ","},      {"Period", "."}};
    for (const auto& s : kShort)
        if (name == s[0]) {
            k.text = s[1];
            return k;
        }
    if (name.size() > 3 && name.compare(0, 3, "Num") == 0) name = "N" + name.substr(3);  // Num5: N5
    if (name.size() > 2) name = name.substr(name.size() - 2);                             // F10: 10
    for (char& c : name) c = char(std::toupper(static_cast<unsigned char>(c)));
    k.text = name;
    return k;
}

// each picture's control, the first key bound to it
void ReadLabels(Label out[kSlots]) {
    auto bindings = input::Bindings();
    for (int s = 0; s < kSlots; s++) {
        out[s] = {};
        for (const auto& [control, keys] : bindings) {
            if (control != kSlotControl[s]) continue;
            std::string first = keys.substr(0, keys.find(','));
            first.erase(0, first.find_first_not_of(' '));
            first.erase(first.find_last_not_of(' ') + 1);
            out[s] = LabelFor(first);
        }
    }
}

uint32_t Wanted() {
    if (g_style != kAuto) return std::min<uint32_t>(g_style, kKeys);
    switch (input::PadFamily(0)) {
        case input::Family::PlayStation: return kPlayStation;
        case input::Family::Switch: return kSwitch;
        case input::Family::Keyboard: return kKeys;
        default: return kXbox;
    }
}

}  // namespace

// render thread, each texture level decoded
void Texture(uint32_t data, uint32_t format, uint32_t width, uint32_t height, uint32_t level, uint8_t* bgra, uint32_t pitch) {
    if (format != kFmtP8 || width != kSize || height != kSize || level != 0) return;
    const Layout* l = IsFont(bgra, pitch, kMenuFont) ? &kMenuFont : IsFont(bgra, pitch, kHintFont) ? &kHintFont : nullptr;
    if (!l) return;
    Label keys[kSlots];
    {
        std::lock_guard lock(s_lock);
        if (std::find(s_fonts.begin(), s_fonts.end(), data) == s_fonts.end()) {
            s_fonts.push_back(data);
            LOGR("prompts: %s button font at %08X", l == &kMenuFont ? "menu" : "hint", data);
        }
        std::copy(std::begin(s_labels), std::end(s_labels), keys);
    }
    uint32_t style = s_drawn;
    if (style == kXbox) return;
    // pads: the four buttons and the trigger names; keys: every picture
    int slots = style == kKeys ? kSlots : kWhite;
    for (int s = 0; s < slots; s++)
        if (l->pic[s].hw > 0) DrawPic(bgra, pitch, style, s, *l, keys[s]);
}

// each frame: a new pad, setting or key binding has the fonts decoded again
void Frame() {
    uint32_t want = Wanted();
    Label keys[kSlots];
    if (want == kKeys) ReadLabels(keys);
    bool rebound = false;
    {
        std::lock_guard lock(s_lock);
        if (want == kKeys && !std::equal(std::begin(keys), std::end(keys), std::begin(s_labels))) {
            std::copy(std::begin(keys), std::end(keys), s_labels);
            rebound = true;
        }
    }
    if (want == s_seen && !rebound) return;
    s_seen = want;
    s_drawn = want;
    static const char* const kNames[] = {"auto", "Xbox", "PlayStation", "Switch", "keyboard and mouse"};
    LOGR("prompts: %s buttons", kNames[want]);
    std::lock_guard lock(s_lock);
    for (uint32_t va : s_fonts) gpu::MarkDirtyData(va);
}

}  // namespace prompts
