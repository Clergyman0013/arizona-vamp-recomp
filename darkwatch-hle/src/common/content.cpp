// Changes to the game's packs as they are read; the disc's files stay as they are.
//
// Content the Xbox disc has but switches off: the War Train chapter and the Splitter grenade pickups. Each is a
// platform selector in the game's binary XML, a 2-byte string index: War Train's xbox flag is false, and the
// campaign's Splitter pickups are marked for the other platforms while their Xbox dynamite stand-ins are marked xbox.
// Swapping the three indices as the files are read brings both back (with the dynamite stand-ins off, so a spot has
// one pickup, not two). The level list and the pickup templates are in two packs, packfile.pck and the single-player
// stream SinglStr.pck, which the campaign reads them from; both copies change.
// Leads from Darkwatch: Darker Edition (github.com/linkssy2/DarkwatchDarkerEdition), which edits packfile.pck's.
//
// The front end's Xbox Live status line (sub_000F023A picks XBL_LiveAware_*: "Sign-in failed.", "Not signed in", ...)
// has no Live to report on: its four texts in packfile.pck's string table become spaces, the same length.
//
// A spot changes only when its bytes are the USA disc's.
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>

#include "core/log.h"

namespace content {

bool g_bonus = true;  // settings.toml bonus_content; read at boot, so a change takes a restart

namespace {

const char* const kPacks[] = {"packfile.pck", "singlstr.pck"};
// the original bytes from 6 before each index, and with the index changed
constexpr uint8_t kWarTrain[16] = {0x27, 0, 0, 0, 0, 0, 0x13, 0, 0x28, 0, 0, 0, 0, 0, 0x29, 0};
constexpr uint8_t kWarTrainOn[16] = {0x27, 0, 0, 0, 0, 0, 0x29, 0, 0x28, 0, 0, 0, 0, 0, 0x29, 0};
constexpr uint8_t kDynamite[16] = {0x04, 0, 0x05, 0, 0xF0, 0, 0xF1, 0, 0x08, 0, 0x06, 0, 0x01, 0, 0x02, 0};
constexpr uint8_t kDynamiteOff[16] = {0x04, 0, 0x05, 0, 0xF0, 0, 0xF3, 0, 0x08, 0, 0x06, 0, 0x01, 0, 0x02, 0};
constexpr uint8_t kSplitter[16] = {0x04, 0, 0xDF, 0, 0xF0, 0, 0xF3, 0, 0x07, 0, 0x06, 0, 0x01, 0, 0x02, 0};
constexpr uint8_t kSplitterOn[16] = {0x04, 0, 0xDF, 0, 0xF0, 0, 0xF1, 0, 0x07, 0, 0x06, 0, 0x01, 0, 0x02, 0};
constexpr char kSpaces[] = "                                ";  // 32
struct Spot {
    uint8_t pack;  // kPacks
    bool bonus;    // only with g_bonus
    uint64_t at;   // file offset of the first byte
    const void* from;
    const void* to;
    uint32_t len;
    const char* what;
};
const Spot kSpots[] = {
    {0, true, 49996315, kWarTrain, kWarTrainOn, 16, "War Train chapter on Xbox"},
    {0, true, 109291324, kDynamite, kDynamiteOff, 16, "Xbox dynamite stand-ins off"},
    {0, true, 109292048, kSplitter, kSplitterOn, 16, "Splitter pickups on Xbox"},
    {1, true, 622143456, kWarTrain, kWarTrainOn, 16, "War Train chapter on Xbox"},
    {1, true, 623238396, kDynamite, kDynamiteOff, 16, "Xbox dynamite stand-ins off"},
    {1, true, 623239120, kSplitter, kSplitterOn, 16, "Splitter pickups on Xbox"},
    {0, false, 104504452, "Sign-in failed.", kSpaces, 15, "no Xbox Live status"},
    {0, false, 104504493, "Signed in: ^0", kSpaces, 13, "no Xbox Live status"},
    {0, false, 104504535, "Not signed in", kSpaces, 13, "no Xbox Live status"},
    {0, false, 104504580, "Not signed in: Pass code needed.", kSpaces, 32, "no Xbox Live status"},
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

// any thread, after each read of a game file; a spot split across two reads changes part by part
void Read(const std::string& xbox, uint64_t pos, uint8_t* data, uint32_t len) {
    int pack = Pack(xbox);
    if (pack < 0) return;
    for (const Spot& e : kSpots) {
        if (e.pack != pack || (e.bonus && !g_bonus)) continue;
        uint64_t lo = e.at > pos ? e.at : pos, hi = e.at + e.len < pos + len ? e.at + e.len : pos + len;
        if (lo >= hi) continue;  // none of it in this read
        auto from = static_cast<const uint8_t*>(e.from), to = static_cast<const uint8_t*>(e.to);
        if (std::memcmp(data + (lo - pos), from + (lo - e.at), size_t(hi - lo)) != 0) {
            LOGW("content: %s at %llu is not the USA disc's, left as it is", kPacks[pack], (unsigned long long)e.at);
            continue;
        }
        std::memcpy(data + (lo - pos), to + (lo - e.at), size_t(hi - lo));
        LOGR("content: %s (%s)", e.what, kPacks[pack]);
    }
}

}  // namespace content
