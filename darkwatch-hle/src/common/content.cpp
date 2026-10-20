// Content the Xbox disc has but switches off: the War Train chapter and the Splitter grenade pickups. Each is a
// platform selector in the game's binary XML, a 2-byte string index: War Train's xbox flag is false, and the
// campaign's Splitter pickups are marked for the other platforms while their Xbox dynamite stand-ins are marked xbox.
// Swapping the three indices as the files are read brings both back (with the dynamite stand-ins off, so a spot has
// one pickup, not two). The level list and the pickup templates are in two packs, packfile.pck and the single-player
// stream SinglStr.pck, which the campaign reads them from; both copies change. An index changes only when the bytes
// around it are the USA disc's.
// Leads from Darkwatch: Darker Edition (github.com/linkssy2/DarkwatchDarkerEdition), which edits packfile.pck's.
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>

#include "core/log.h"

namespace content {

bool g_bonus = true;  // settings.toml bonus_content; read at boot, so a change takes a restart

namespace {

const char* const kPacks[] = {"packfile.pck", "singlstr.pck"};
// the original bytes from 6 before each index
constexpr uint8_t kWarTrain[16] = {0x27, 0, 0, 0, 0, 0, 0x13, 0, 0x28, 0, 0, 0, 0, 0, 0x29, 0};
constexpr uint8_t kDynamite[16] = {0x04, 0, 0x05, 0, 0xF0, 0, 0xF1, 0, 0x08, 0, 0x06, 0, 0x01, 0, 0x02, 0};
constexpr uint8_t kSplitter[16] = {0x04, 0, 0xDF, 0, 0xF0, 0, 0xF3, 0, 0x07, 0, 0x06, 0, 0x01, 0, 0x02, 0};
struct Spot {
    uint8_t pack;  // kPacks
    uint64_t at;   // file offset of the 2-byte index
    const uint8_t* near;
    uint16_t to;
    const char* what;
};
const Spot kSpots[] = {
    {0, 49996321, kWarTrain, 0x29, "War Train chapter on Xbox"},
    {0, 109291330, kDynamite, 0xF3, "Xbox dynamite stand-ins off"},
    {0, 109292054, kSplitter, 0xF1, "Splitter pickups on Xbox"},
    {1, 622143462, kWarTrain, 0x29, "War Train chapter on Xbox"},
    {1, 623238402, kDynamite, 0xF3, "Xbox dynamite stand-ins off"},
    {1, 623239126, kSplitter, 0xF1, "Splitter pickups on Xbox"},
};

// the pack a path names (its file name, after any folder or drive), -1 for another file
int Pack(const std::string& xbox) {
    size_t at = xbox.find_last_of("\\:");
    std::string name = xbox.substr(at == std::string::npos ? 0 : at + 1);
    for (char& c : name) c = char(std::tolower(static_cast<unsigned char>(c)));
    for (int i = 0; i < 2; i++)
        if (name == kPacks[i]) return i;
    return -1;
}

}  // namespace

// any thread, after each read of a game file
void Read(const std::string& xbox, uint64_t pos, uint8_t* data, uint32_t len) {
    if (!g_bonus) return;
    int pack = Pack(xbox);
    if (pack < 0) return;
    for (const Spot& e : kSpots) {
        if (e.pack != pack || e.at < pos || e.at + 2 > pos + len) continue;  // not all in this read
        // the bytes around it that this read holds must be the disc's
        bool same = true;
        for (int i = 0; i < 16 && same; i++) {
            uint64_t o = e.at - 6 + i;
            if (o >= pos && o < pos + len) same = data[o - pos] == e.near[i];
        }
        if (!same) {
            LOGW("content: %s at %llu is not the USA disc's, left as it is", kPacks[pack], (unsigned long long)e.at);
            continue;
        }
        data[e.at - pos] = uint8_t(e.to);
        data[e.at - pos + 1] = uint8_t(e.to >> 8);
        LOGR("content: %s (%s)", e.what, kPacks[pack]);
    }
}

}  // namespace content
