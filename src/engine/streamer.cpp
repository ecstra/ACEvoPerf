// The engine's texture streamer, see include/acevo/engine/streamer.h.
//
// What goes wrong. About once a second the streamer runs a kick: it asks every texture how much
// detail it needs, admits the answers in priority order until the tile pool budget runs out,
// loads what was admitted and drops what was not. For material textures the answer comes from
// GPU feedback, and the shaders measure the level of detail against whatever mip is loaded right
// now (GetDimensions of the bound view), while the kick reads that number as if it had been
// measured against the full texture. So a texture near the budget edge cannot settle. With the
// coarse mip loaded the reading asks for the finer one, the finer one loads, the same view then
// reads as needing less, and the finer mip is dropped again. Parked with the camera still that is
// 14 to 22 MB/s of disk reads, uploads and tile remaps, every two kicks, forever (TODO-018).
//
// Why the conversion is not simply corrected. The shader clamps the reading at zero, so a coarse
// mip cannot report "finer than this", and the engine has to probe upward to find out. A
// corrected conversion would stop textures sharpening. The fix keeps what the probe found
// instead. It acts only on the flip itself, recognised from the readings: a texture dropped from
// level f to c on a fresh reading, then reloaded on a fresh reading at the coarse view that is
// the first one shifted down by the levels it lost, which is what a reading taken against the
// bound view does and a real change of view does not. That texture's next drop back to c is
// refused, and it settles at the sharper of its two states, which the screen already showed half
// the time. It lets go when its reading says the view moved away, when its screen coverage
// changes, when it stops being admitted at all, or when the drop leaves the flip.
//
// Parked at the Nurburgring pit exit on 2026-09-13 the recorded session replays to all of the
// churn removed, and a fifth to a quarter of the reload traffic while driving, where most drops
// are real changes of view and pass through.
//
// The five sites, each the rewrite of one rel32 displacement:
//
//   S1  the lea that hands the kick job its function, through a stub that counts kicks
//   S2  the selected update's read of the current level, which also sees the admitted level
//   S3  the selected update's tail jump into dropLevels, a drop to a lower admitted level
//   S4  the kick's call of dropLevels(texture, 0) for a texture not admitted at all
//   D   the jump taken when a load does not fit the free pool space, through a stub that counts it
//
// Why this is safe to do:
//
//   - Every region the hooks rely on is compared with this build byte for byte before anything
//     is written, and none of those ranges carries a base relocation. A game update that touches
//     any of it means nothing is patched and the log names the region that moved.
//   - Only 4 byte displacements change, so no instruction boundary moves. The selected update has
//     a single caller, the kick, and the kick thunk has a single reference, the S1 lea.
//   - Kicks are serialised, the scheduler waits on the previous kick's job before it queues the
//     next one, so the per texture state below needs no lock. A second hook arriving while one is
//     running would mean that is wrong, and everything then passes straight to the engine.
//   - A refused drop keeps tiles the engine would have freed, and the engine's tile allocator
//     stalls rather than evicts when the pool runs dry. The pool is a fixed heap that sits full in
//     normal play, parked the streamer turns about 120 loads away for space on every kick, so
//     contention cannot be the test or the fix would never act, and the flipping texture holds
//     those tiles half the time today anyway. The streamer's own loads go through a gate that
//     keeps a 1024 tile margin free, the margin is what loads outside that gate live on, so a drop
//     is only refused while used plus pending tiles still leave that margin free.
//   - A fault inside a hook turns every hook into a straight pass through for the session.
//   - It is applied from DllMain, before the game's entry point runs.
#include "acevo/engine/streamer.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/render/frame_stats.h"
#include "acevo/telemetry/streaming_trace.h"
#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>

// ---------------------------------------------------------------------------
// The build this was written against
// ---------------------------------------------------------------------------

// AssettoCorsaEVO.exe 0.9.1, the Steam build of 2026-09-11.
static const DWORD kTimeDateStamp = 0x6A9EC72A;
static const DWORD kSizeOfImage = 0x06CDD000;

struct Region {
    uint32_t rva;
    uint32_t length;
    uint64_t fnv1a64;
    const char* what;
};

static const Region kRegions[] = {
    { 0x1F211E0, 0x280, 0xCAAD89909D58A3FCull, "the selected texture update (S2, S3, D)" },
    { 0x1F2EB51, 0x0B0, 0x14DB9ABBFD8FFC9Aull, "the update loop and the context it passes" },
    { 0x1F2ED9D, 0x021, 0x36A6C1A21032774Full, "the drop of textures not admitted (S4)" },
    { 0x1F2E303, 0x1A4, 0x973EED67A29B7AC8ull, "the admission loop and the kick fields" },
    { 0x1F2E753, 0x01B, 0x844E4B2160B74063ull, "the started tiles count" },
    { 0x1F29390, 0x025, 0xCB01831FC00DA507ull, "the kick scheduling (S1)" },
    { 0x1F2D110, 0x040, 0xD4C805E7381B0F01ull, "the kick frame setup" },
    { 0x0186AFB, 0x005, 0x069BD015A2C1F9A7ull, "the kick thunk" },
    { 0x00DC2D1, 0x005, 0x41E9FC80D024AF03ull, "the level getter thunk" },
    { 0x0110342, 0x005, 0xB2975DF0DB013625ull, "the dropLevels thunk" },
    { 0x1C83730, 0x018, 0xB289C19830C0E2D2ull, "the level getter" },
    { 0x06692D0, 0x004, 0xCC8F62DA6A2DA724ull, "the atomic address helper" },
    { 0x1C82390, 0x020, 0x7B4C90357780EB8Eull, "dropLevels" },
    { 0x1C84470, 0x013, 0x72B26E4E10B396B7ull, "the feedback id" },
    { 0x1F4E7C0, 0x022, 0x12D4AFE89F83908Dull, "the feedback count getter" },
    { 0x1F4E7F0, 0x02F, 0xC52DA26172EBB19Eull, "the feedback mip getter" },
    { 0x1C84890, 0x00B, 0xF653FF13C1B4B00Bull, "the pool capacity getter" },
    { 0x1C84850, 0x018, 0x73C1BA48A014B4BEull, "the pool pending getter" },
    { 0x278BFD0, 0x016, 0x7DAA70DF7484B254ull, "the pool used getter" },
};

// The layouts the census reads, checked only when it is on. A mismatch turns the census off and
// leaves the hooks alone.
static const Region kCensusRegions[] = {
    { 0x1F2D86B, 0x14B, 0x68F1E8D2DB315760ull, "the demand vector, deduped and hashed" },
    { 0x1F2DF49, 0x043, 0xA9F2F0580EC72C5Aull, "the record push" },
    { 0x1F2E1B7, 0x14C, 0xA9590430BD6494C9ull, "the node walk that sums pending and resident tiles" },
    { 0x1C842C0, 0x018, 0x6A8762D55253BFCAull, "the pending tiles getter" },
    { 0x1C84730, 0x018, 0x20A3B5BA80C06B5Aull, "the resident tiles getter" },
    { 0x1F2A840, 0x047, 0x3E869317CB8F2CC9ull, "the texture category getter" },
};

struct Site {
    const char* what;
    uint32_t rva;
    uint8_t dispOffset;
    uint8_t length;
    uint32_t target;
};

static const Site kSiteDenied = { "D", 0x1F2137B, 2, 6, 0x1F2141B };    // jg past the load
static const Site kSiteLevel = { "S2", 0x1F212C1, 1, 5, 0x00DC2D1 };    // call the level getter
static const Site kSiteDrop = { "S3", 0x1F21451, 1, 5, 0x0110342 };     // jmp dropLevels
static const Site kSiteDrop0 = { "S4", 0x1F2EDB9, 1, 5, 0x0110342 };    // call dropLevels
static const Site kSiteKick = { "S1", 0x1F2939B, 3, 7, 0x0186AFB };     // lea rax, kick thunk

static const uint32_t kRvaKick = 0x1F2D110;
static const uint32_t kRvaDropLevels = 0x1C82390;

// ---------------------------------------------------------------------------
// Engine layouts, each one read by code inside the regions above
// ---------------------------------------------------------------------------

namespace texture {
constexpr ptrdiff_t kPath = 0x8;                // std::wstring, the package path
constexpr ptrdiff_t kAllocator = 0x50;
constexpr ptrdiff_t kResourceWrapper = 0x60;    // the ID3D12Resource* sits at +0x10 of it
constexpr ptrdiff_t kFeedbackSlot = 0x88;
constexpr ptrdiff_t kLevelsBegin = 0x160;       // vector of 48 byte level entries, 0 is coarsest
constexpr ptrdiff_t kLevelsEnd = 0x168;
constexpr ptrdiff_t kCurrentLevel = 0x17C;
constexpr ptrdiff_t kPendingTiles = 0x180;
constexpr ptrdiff_t kResidentTiles = 0x184;
constexpr ptrdiff_t kCategory = 0x270;          // the material kind the streamer turns into request flags
constexpr ptrdiff_t kLevelEntrySize = 48;
constexpr ptrdiff_t kLevelTiles = 0x8;
}

namespace allocator {
constexpr ptrdiff_t kFeedbackBase = 0xA8B0;
constexpr ptrdiff_t kPool = 0xD2A8;
constexpr ptrdiff_t kPending = 0xD2B0;
}

namespace pool {
constexpr ptrdiff_t kTotal = 0x8;
constexpr ptrdiff_t kUsed = 0x10;
}

namespace streamer {
constexpr ptrdiff_t kAllocator = 0x0;
constexpr ptrdiff_t kNodes = 0x40;              // std::map head of the tracked textures, keyed by path hash
constexpr ptrdiff_t kNodeCount = 0x48;
constexpr ptrdiff_t kRecordBuffer = 0x50;       // 16 byte records, cut to the admitted ones after admission
constexpr ptrdiff_t kRecordBufferCapacity = 0x60;
constexpr ptrdiff_t kAdmittedLevels = 0x68;     // {hash, level} sorted by hash
constexpr ptrdiff_t kAdmittedLevelsEnd = 0x70;
constexpr ptrdiff_t kDemand = 0xB0;             // 24 byte {texture, distance, flags}, one per texture
constexpr ptrdiff_t kDemandEnd = 0xB8;
constexpr ptrdiff_t kFeedback = 0xD0;
constexpr ptrdiff_t kCap = 0x148;
constexpr ptrdiff_t kAvail = 0x150;
constexpr ptrdiff_t kRecords = 0x154;
constexpr ptrdiff_t kAdmitted = 0x158;
constexpr ptrdiff_t kAdmittedTiles = 0x15C;
constexpr ptrdiff_t kRejected = 0x160;
}

namespace node {    // an MSVC std::map node
constexpr ptrdiff_t kLeft = 0x0;
constexpr ptrdiff_t kParent = 0x8;
constexpr ptrdiff_t kRight = 0x10;
constexpr ptrdiff_t kIsNil = 0x19;
constexpr ptrdiff_t kHash = 0x20;
constexpr ptrdiff_t kTexture = 0x28;
}

namespace feedback {
constexpr ptrdiff_t kAgeLimit = 0x0;
constexpr ptrdiff_t kIds = 0x10;
constexpr ptrdiff_t kMips = 0x40;
constexpr ptrdiff_t kCounts = 0x58;
constexpr ptrdiff_t kAges = 0x70;
}

// The kick's frame, rbp inside the kick, and the context it hands the selected update.
namespace kick {
constexpr ptrdiff_t kStreamer = -0x40;
constexpr ptrdiff_t kAvail = 0x10;
constexpr ptrdiff_t kStarted = 0x14;
constexpr ptrdiff_t kGate = 0xE8;
constexpr ptrdiff_t kContextToFrame = 0x30;
constexpr ptrdiff_t kContextStreamer = 0x0;
constexpr ptrdiff_t kContextAvailPtr = 0x18;
}

// A texture that reloads what it dropped within this many kicks is flipping. Parked the reload
// comes on the next kick, or up to five later when kicks pass without loads.
static const uint64_t kFlipWindowKicks = 6;
static const uint64_t kForgetKicks = 256;
// The streamer's own admission keeps this many tiles back for loads outside it.
static const int64_t kPoolMarginTiles = 1024;
// Screen coverage is the feedback sample count. Parked it holds to the sample, so a change beyond
// this share, or two samples for small textures, means the view changed and the pin lets go.
static const double kCoverageSlack = 0.25;
static const uint32_t kCoverageSlackMin = 2;

template <typename T>
static T At(const BYTE* p, ptrdiff_t offset)
{
    T value;
    memcpy(&value, p + offset, sizeof value);
    return value;
}

static uint64_t Fnv1a64(const BYTE* p, size_t n)
{
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

// ---------------------------------------------------------------------------
// State, touched only from inside kicks unless it is atomic
// ---------------------------------------------------------------------------

using DropLevelsFn = void (*)(BYTE* texture, int keepLevel);

static bool g_installed = false;
static DropLevelsFn g_dropLevels = nullptr;
static const uint64_t* g_kickCount = nullptr;       // bumped by the S1 stub
static const uint64_t* g_deniedCount = nullptr;     // bumped by the D stub
static std::atomic<bool> g_broken{false};
static std::atomic<bool> g_inHook{false};
static std::atomic<BYTE*> g_allocator{nullptr};

static std::atomic<uint64_t> g_kicks{0}, g_wantsFiner{0}, g_drops{0}, g_dropsUnadmitted{0};
static std::atomic<uint64_t> g_refused{0}, g_refusedTiles{0}, g_wouldRefuse{0}, g_pins{0};

enum class Verdict : int {
    NotPinned = 0,
    Refuse = 1,
    LeftTheFlip = 2,
    StaleReading = 3,
    MovedAway = 4,
    CoverageChanged = 5,
    PoolMargin = 6,
    NotAdmitted = 7,
};

struct TextureState {
    uint64_t lastSeenKick = 0;
    uint64_t dropKick = 0;
    uint64_t pathHash = 0;
    const void* resource = nullptr;
    int levels = -1;
    int dropFrom = -1;
    int dropTo = -1;
    int dropMip = -1;           // the feedback reading the last drop was made on, -1 when stale
    int pinFine = -1;           // the level held while pinned, -1 when not pinned
    int pinCoarse = -1;         // the level the flip dropped to
    int pinMip = -1;            // the reading at the fine view when the flip was recognised
    uint32_t pinCount = 0;      // the screen coverage then, in feedback samples
    bool described = false;
};

struct KickTally {
    uint32_t wantsFiner = 0;
    uint32_t drops = 0;
    uint32_t refused = 0;
    uint32_t pins = 0;
};

static std::unordered_map<const BYTE*, TextureState> s_textures;
static uint64_t s_kick = 0;
static uint64_t s_deniedAtKickStart = 0;
static uint64_t s_deniedLastKick = 0;
static KickTally s_tally;

// ---------------------------------------------------------------------------
// Reading the engine
// ---------------------------------------------------------------------------

static int LevelCount(const BYTE* tex)
{
    return (int)((At<BYTE*>(tex, texture::kLevelsEnd) - At<BYTE*>(tex, texture::kLevelsBegin)) / texture::kLevelEntrySize);
}

// An MSVC std::wstring, empty when its size and capacity do not look like one.
static std::wstring_view PathOf(const BYTE* tex)
{
    size_t size = At<size_t>(tex, texture::kPath + 0x10);
    size_t reserved = At<size_t>(tex, texture::kPath + 0x18);
    if (size >= 512 || reserved < size) return {};
    const wchar_t* chars = reserved > 7 ? At<wchar_t*>(tex, texture::kPath) : (const wchar_t*)(tex + texture::kPath);
    return { chars, size };
}

static const void* ResourceOf(const BYTE* tex)
{
    const BYTE* wrapper = At<BYTE*>(tex, texture::kResourceWrapper);
    return wrapper ? At<void*>(wrapper, 0x10) : nullptr;
}

// Tiles of levels above `from` up to and including `to`, the same sum the selected update makes
// before a load.
static int64_t TilesBetween(const BYTE* tex, int from, int to)
{
    const BYTE* levels = At<BYTE*>(tex, texture::kLevelsBegin);
    int count = LevelCount(tex);
    int64_t tiles = 0;
    for (int level = std::max(from + 1, 0); level <= to && level < count; ++level)
        tiles += At<int32_t>(levels + level * texture::kLevelEntrySize, texture::kLevelTiles);
    return tiles;
}

// The space the load gate grants from here on, cap minus used minus pending minus the margin
// minus what this kick has already started loading.
static int64_t GateSpace(const BYTE* frame)
{
    return (int64_t)At<int32_t>(frame, kick::kAvail) - At<int32_t>(frame, kick::kStarted);
}

struct Feedback {
    int mip = -1;
    uint32_t count = 0;
    uint32_t age = 0xFFFF;
};

// The same test the engine's getters make: a reading older than the age limit does not exist.
static Feedback ReadFeedback(const BYTE* str, const BYTE* tex)
{
    Feedback fb;
    const BYTE* fbObject = At<BYTE*>(str, streamer::kFeedback);
    if (!fbObject) return fb;

    const BYTE* alloc = At<BYTE*>(tex, texture::kAllocator);
    uint32_t id = (uint32_t)(At<int32_t>(tex, texture::kFeedbackSlot) - At<int32_t>(alloc, allocator::kFeedbackBase));
    if (id >= At<uint32_t>(fbObject, feedback::kIds)) return fb;

    fb.age = At<uint16_t>(At<BYTE*>(fbObject, feedback::kAges), (ptrdiff_t)id * 2);
    if (fb.age >= At<uint32_t>(fbObject, feedback::kAgeLimit)) return fb;

    fb.mip = At<int32_t>(At<BYTE*>(fbObject, feedback::kMips), (ptrdiff_t)id * 4);
    fb.count = At<uint32_t>(At<BYTE*>(fbObject, feedback::kCounts), (ptrdiff_t)id * 4);
    return fb;
}

// A refused drop's tiles are already counted in used, so this is the margin as it stands.
static bool PoolKeepsMargin(const BYTE* str)
{
    const BYTE* alloc = At<BYTE*>(str, streamer::kAllocator);
    const BYTE* tilePool = alloc ? At<BYTE*>(alloc, allocator::kPool) : nullptr;
    if (!tilePool) return false;

    int64_t total = At<int32_t>(tilePool, pool::kTotal);
    int64_t used = (int64_t)At<uint64_t>(tilePool, pool::kUsed);
    int64_t pending = At<int32_t>(alloc, allocator::kPending);
    return used + pending + kPoolMarginTiles <= total;
}

// ---------------------------------------------------------------------------
// The census, [developer] streamer_census=1, a development instrument for BUG-020
// ---------------------------------------------------------------------------

// Each kick's demand, the records the engine ranked from it and the tiles its tracked textures
// hold, so a session can be ranked again offline. acevo_perf_census.bin, little endian blocks:
//
//   TEXD  texture, path hash, category, levels, tiles of levels 0 to 7, path length, UTF-8 path
//   KICK  kick, seconds, cap, avail, records, admitted, demand, tracked entries, pool used, pool
//         pending, resident and pending tiles of every tracked texture, resident tiles of those
//         not admitted at all, tiles above the admitted level, gate space, then each demand entry
//         {texture, distance, flags}, each record as the engine keeps it {hash, priority, 1, tiles,
//         level}, and on every 8th kick each tracked texture {texture, resident, pending, current
//         level, admitted level or -1}
static bool g_censusOn = false;
static SRWLOCK g_censusLock = SRWLOCK_INIT;
static std::string g_censusPending;
static HANDLE g_censusFile = INVALID_HANDLE_VALUE;
static uint64_t g_censusBytes = 0;
static std::atomic<uint64_t> g_censusDropped{0};
static std::unordered_map<const BYTE*, uint64_t> s_described;    // texture to path hash, kicks only
static const size_t kCensusMaxPendingBytes = 256u * 1024u * 1024u;
static const size_t kDemandEntrySize = 24;
static const size_t kRecordSize = 16;

template <typename T>
static void Put(std::string& out, T value)
{
    out.append((const char*)&value, sizeof value);
}

static void CensusDescribe(std::string& out, const BYTE* tex, uint64_t pathHash, std::wstring_view path)
{
    char utf8[520];
    int n = path.empty() ? 0 : WideCharToMultiByte(CP_UTF8, 0, path.data(), (int)path.size(), utf8, sizeof utf8, nullptr, nullptr);
    int levels = std::clamp(LevelCount(tex), 0, 255);
    const BYTE* entries = At<BYTE*>(tex, texture::kLevelsBegin);

    out.append("TEXD", 4);
    Put(out, (uint64_t)(uintptr_t)tex);
    Put(out, pathHash);
    Put(out, At<int32_t>(tex, texture::kCategory));
    Put(out, (uint8_t)levels);
    for (int level = 0; level < 8; ++level) {
        int tiles = level < levels ? At<int32_t>(entries + level * texture::kLevelEntrySize, texture::kLevelTiles) : 0;
        Put(out, (uint16_t)std::clamp(tiles, 0, 0xFFFF));
    }
    Put(out, (uint16_t)std::max(n, 0));
    out.append(utf8, std::max(n, 0));
}

static int AdmittedLevelOf(const BYTE* admitted, size_t count, uint64_t hash)
{
    size_t lo = 0;
    size_t hi = count;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (At<uint64_t>(admitted + mid * kRecordSize, 0) < hash) lo = mid + 1;
        else hi = mid;
    }
    if (lo == count || At<uint64_t>(admitted + lo * kRecordSize, 0) != hash) return -1;
    return At<int32_t>(admitted + lo * kRecordSize, 8);
}

static void CensusKickBody(const BYTE* str, const BYTE* frame, uint64_t now)
{
    std::string block;
    block.reserve(256 * 1024);

    const BYTE* demand = At<BYTE*>(str, streamer::kDemand);
    const BYTE* demandEnd = At<BYTE*>(str, streamer::kDemandEnd);
    size_t demandCount = demandEnd > demand ? (size_t)(demandEnd - demand) / kDemandEntrySize : 0;
    if (demandCount > 1000000) return;

    for (size_t i = 0; i < demandCount; ++i) {
        const BYTE* tex = At<BYTE*>(demand + i * kDemandEntrySize, 0);
        if (!tex) continue;
        std::wstring_view path = PathOf(tex);
        uint64_t pathHash = Fnv1a64((const BYTE*)path.data(), path.size() * sizeof(wchar_t));
        auto [known, inserted] = s_described.try_emplace(tex, pathHash);
        if (!inserted && known->second == pathHash) continue;
        known->second = pathHash;
        CensusDescribe(block, tex, pathHash, path);
    }

    const BYTE* records = At<BYTE*>(str, streamer::kRecordBuffer);
    const BYTE* recordsCapacity = At<BYTE*>(str, streamer::kRecordBufferCapacity);
    size_t recordCount = At<uint32_t>(str, streamer::kRecords);
    size_t recordRoom = recordsCapacity > records ? (size_t)(recordsCapacity - records) / kRecordSize : 0;
    recordCount = std::min(recordCount, recordRoom);

    const BYTE* admitted = At<BYTE*>(str, streamer::kAdmittedLevels);
    const BYTE* admittedEnd = At<BYTE*>(str, streamer::kAdmittedLevelsEnd);
    size_t admittedCount = admittedEnd > admitted ? (size_t)(admittedEnd - admitted) / kRecordSize : 0;

    // The same walk the kick makes to credit tracked tiles to its budget, in hash order.
    bool listTracked = (now & 7) == 0;
    std::string tracked;
    uint32_t trackedEntries = 0;
    int64_t resident = 0, pending = 0, notAdmitted = 0, aboveAdmitted = 0;
    const BYTE* head = At<BYTE*>(str, streamer::kNodes);
    size_t nodeCount = At<size_t>(str, streamer::kNodeCount);
    const BYTE* at = At<BYTE*>(head, node::kLeft);
    for (size_t visited = 0; at != head && visited < nodeCount; ++visited) {
        const BYTE* tex = At<BYTE*>(at, node::kTexture);
        if (tex) {
            int32_t texResident = At<int32_t>(tex, texture::kResidentTiles);
            int32_t texPending = At<int32_t>(tex, texture::kPendingTiles);
            int current = At<int32_t>(tex, texture::kCurrentLevel);
            int level = AdmittedLevelOf(admitted, admittedCount, At<uint64_t>(at, node::kHash));
            resident += texResident;
            pending += texPending;
            if (level < 0) notAdmitted += texResident;
            else if (current > level) aboveAdmitted += TilesBetween(tex, level, current);

            if (listTracked) {
                Put(tracked, (uint64_t)(uintptr_t)tex);
                Put(tracked, texResident);
                Put(tracked, (int16_t)std::clamp(texPending, -32768, 32767));
                Put(tracked, (int8_t)std::clamp(current, -128, 127));
                Put(tracked, (int8_t)std::clamp(level, -128, 127));
                ++trackedEntries;
            }
        }

        const BYTE* right = At<BYTE*>(at, node::kRight);
        if (!At<uint8_t>(right, node::kIsNil)) {
            at = right;
            while (!At<uint8_t>(At<BYTE*>(at, node::kLeft), node::kIsNil)) at = At<BYTE*>(at, node::kLeft);
            continue;
        }
        const BYTE* parent = At<BYTE*>(at, node::kParent);
        while (!At<uint8_t>(parent, node::kIsNil) && at == At<BYTE*>(parent, node::kRight)) {
            at = parent;
            parent = At<BYTE*>(at, node::kParent);
        }
        at = parent;
    }

    const BYTE* alloc = At<BYTE*>(str, streamer::kAllocator);
    const BYTE* tilePool = At<BYTE*>(alloc, allocator::kPool);

    block.append("KICK", 4);
    Put(block, now);
    Put(block, NowSec());
    Put(block, At<int32_t>(str, streamer::kCap));
    Put(block, At<int32_t>(str, streamer::kAvail));
    Put(block, (uint32_t)recordCount);
    Put(block, At<uint32_t>(str, streamer::kAdmitted));
    Put(block, (uint32_t)demandCount);
    Put(block, trackedEntries);
    Put(block, (int64_t)At<uint64_t>(tilePool, pool::kUsed));
    Put(block, (int64_t)At<int32_t>(alloc, allocator::kPending));
    Put(block, resident);
    Put(block, pending);
    Put(block, notAdmitted);
    Put(block, aboveAdmitted);
    Put(block, GateSpace(frame));
    for (size_t i = 0; i < demandCount; ++i) {
        const BYTE* entry = demand + i * kDemandEntrySize;
        Put(block, (uint64_t)(uintptr_t)At<BYTE*>(entry, 0));
        Put(block, At<float>(entry, 0x10));
        Put(block, At<uint16_t>(entry, 0x14));
        Put(block, (uint16_t)0);
    }
    block.append((const char*)records, recordCount * kRecordSize);
    block.append(tracked);

    AcquireSRWLockExclusive(&g_censusLock);
    if (g_censusPending.size() + block.size() <= kCensusMaxPendingBytes) g_censusPending.append(block);
    else g_censusDropped++;
    ReleaseSRWLockExclusive(&g_censusLock);
}

// A fault while reading turns the census off, the hooks carry on.
static void CensusKick(const BYTE* str, const BYTE* frame, uint64_t now)
{
    if (!g_censusOn) return;
    __try {
        CensusKickBody(str, frame, now);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_censusOn = false;
        Log("[streamer] the census faulted reading kick %llu and is off for the rest of the session", (unsigned long long)now);
    }
}

static void CensusWrite(const std::string& blocks)
{
    if (blocks.empty()) return;
    if (g_censusFile == INVALID_HANDLE_VALUE) {
        std::wstring path = g_dir + L"acevo_perf_census.bin";
        g_censusFile = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (g_censusFile == INVALID_HANDLE_VALUE) return;
    }
    DWORD written = 0;
    WriteFile(g_censusFile, blocks.data(), (DWORD)blocks.size(), &written, nullptr);
    g_censusBytes += written;
}

static void CensusFlush()
{
    std::string blocks;
    AcquireSRWLockExclusive(&g_censusLock);
    blocks.swap(g_censusPending);
    ReleaseSRWLockExclusive(&g_censusLock);
    CensusWrite(blocks);
}

// The same rule as the trace at exit, a thread that died holding the lock leaves the rest unwritten.
static void CensusFinalFlush()
{
    std::string blocks;
    if (!TryAcquireSRWLockExclusive(&g_censusLock)) return;
    blocks.swap(g_censusPending);
    ReleaseSRWLockExclusive(&g_censusLock);
    CensusWrite(blocks);
    if (g_censusFile == INVALID_HANDLE_VALUE) return;

    CloseHandle(g_censusFile);
    g_censusFile = INVALID_HANDLE_VALUE;
    Log("[streamer] census wrote %.1f MB, %llu kicks dropped because the writer fell behind",
        g_censusBytes / 1048576.0, (unsigned long long)g_censusDropped.load());
}

// ---------------------------------------------------------------------------
// The decisions
// ---------------------------------------------------------------------------

static void Broken(const char* where)
{
    if (!g_broken.exchange(true))
        Log("[streamer] a hook faulted in %s. Every hook passes straight through to the engine for the rest of the session, "
            "so the game streams exactly as it would without the mod.", where);
}

static void DescribeTexture(const BYTE* tex, int levels)
{
    char path[520] = "";
    const void* resource = nullptr;
    __try {
        std::wstring_view chars = PathOf(tex);
        if (!chars.empty()) {
            int n = WideCharToMultiByte(CP_UTF8, 0, chars.data(), (int)chars.size(), path, sizeof path - 1, nullptr, nullptr);
            path[n > 0 ? n : 0] = 0;
        }
        resource = ResourceOf(tex);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        path[0] = 0;
    }
    for (char* c = path; *c; ++c)
        if (*c == ',' || *c == '"' || (unsigned char)*c < ' ') *c = '_';
    TraceRow("tex", "%p,%p,%d,%s", tex, resource, levels, path);
}

static void Prune(uint64_t now)
{
    for (auto it = s_textures.begin(); it != s_textures.end();)
        it = (now - it->second.lastSeenKick > kForgetKicks) ? s_textures.erase(it) : std::next(it);
}

static uint64_t BeginEvent(const BYTE* str, const BYTE* frame)
{
    uint64_t now = *g_kickCount;
    if (now == s_kick) return now;

    uint64_t denied = *g_deniedCount;
    s_deniedLastKick = denied - s_deniedAtKickStart;
    s_deniedAtKickStart = denied;
    g_allocator.store((BYTE*)At<BYTE*>(str, streamer::kAllocator));
    g_kicks++;

    TraceRow("kick", "%llu,%d,%d,%d,%d,%d,%d,%lld,%u,%u,%llu,%u,%u",
        (unsigned long long)now, At<int32_t>(str, streamer::kCap), At<int32_t>(str, streamer::kAvail),
        At<int32_t>(str, streamer::kRecords), At<int32_t>(str, streamer::kAdmitted), At<int32_t>(str, streamer::kAdmittedTiles),
        At<int32_t>(str, streamer::kRejected), (long long)GateSpace(frame), (unsigned)At<uint8_t>(frame, kick::kGate),
        s_tally.wantsFiner, (unsigned long long)s_deniedLastKick, s_tally.drops, s_tally.refused);
    CensusKick(str, frame, now);

    s_tally = KickTally{};
    s_kick = now;
    if ((now & 63) == 0) Prune(now);
    return now;
}

static TextureState& Touch(const BYTE* tex, uint64_t now)
{
    TextureState& state = s_textures[tex];
    int levels = LevelCount(tex);
    std::wstring_view path = PathOf(tex);
    uint64_t pathHash = Fnv1a64((const BYTE*)path.data(), path.size() * sizeof(wchar_t));
    const void* resource = ResourceOf(tex);
    if (state.levels != levels || state.pathHash != pathHash || state.resource != resource) {
        // A different texture at an address a freed one used, which can have the same level
        // count, so nothing carries over unless its path and its D3D12 resource match too.
        state = TextureState{};
        state.levels = levels;
        state.pathHash = pathHash;
        state.resource = resource;
    }
    state.lastSeenKick = now;
    if (!state.described && TraceOn()) {
        DescribeTexture(tex, levels);
        state.described = true;
    }
    return state;
}

struct HookGuard {
    bool entered;
    HookGuard() : entered(!g_inHook.exchange(true)) {}
    ~HookGuard() { if (entered) g_inHook.store(false); }
};

static void OnLevel(const BYTE* tex, int admitted, int current, const BYTE* context)
{
    HookGuard guard;
    if (!guard.entered) { Broken("S2, a second hook while one was running"); return; }

    const BYTE* frame = context + kick::kContextToFrame;
    const BYTE* str = At<BYTE*>(context, kick::kContextStreamer);
    if (At<BYTE*>(context, kick::kContextAvailPtr) != frame + kick::kAvail || At<BYTE*>(frame, kick::kStreamer) != str) {
        Broken("S2, the kick frame is not where it should be");
        return;
    }

    uint64_t now = BeginEvent(str, frame);
    TextureState& state = Touch(tex, now);
    if (admitted <= current) return;

    s_tally.wantsFiner++;
    g_wantsFiner++;

    // The flip's signature. The drop was made on a fresh reading taken against the fine view, and
    // the reading at the coarse view is that one shifted down by the levels lost, so nothing about
    // the view changed, only which mip the shader measured against.
    Feedback fb = ReadFeedback(str, tex);
    bool reload = state.dropKick && now - state.dropKick <= kFlipWindowKicks && admitted <= state.dropFrom && current <= state.dropTo;
    bool flip = reload && state.dropMip >= 1 && fb.mip >= 0 && fb.mip <= std::max(0, state.dropMip - (state.dropFrom - current));
    if (flip) {
        if (state.pinFine < 0) { s_tally.pins++; g_pins++; }
        state.pinFine = admitted;
        state.pinCoarse = state.dropTo;
        state.pinMip = state.dropMip;
        state.pinCount = fb.count;
    }

    if (TraceOn()) {
        TraceRow("want", "%llu,%p,%d,%d,%d,%lld,%d,%u,%u,%d,%lld",
            (unsigned long long)now, tex, current, admitted, state.levels, (long long)TilesBetween(tex, current, admitted),
            fb.mip, fb.count, fb.age, flip ? 2 : reload ? 1 : 0, (long long)GateSpace(frame));
    }
}

static Verdict Judge(const TextureState& state, int keep, int current, const Feedback& fb, const BYTE* str, bool admitted)
{
    if (state.pinFine < 0) return Verdict::NotPinned;
    if (!admitted) return Verdict::NotAdmitted;
    if (current != state.pinFine || keep < state.pinCoarse) return Verdict::LeftTheFlip;
    if (fb.mip < 0) return Verdict::StaleReading;
    if (fb.mip > state.pinMip) return Verdict::MovedAway;
    uint32_t drift = fb.count > state.pinCount ? fb.count - state.pinCount : state.pinCount - fb.count;
    if (drift > std::max(kCoverageSlackMin, (uint32_t)(state.pinCount * kCoverageSlack))) return Verdict::CoverageChanged;
    if (!PoolKeepsMargin(str)) return Verdict::PoolMargin;
    return Verdict::Refuse;
}

// Returns true when the drop is refused.
static bool OnDrop(const BYTE* tex, int keep, const BYTE* str, const BYTE* frame, bool admitted)
{
    HookGuard guard;
    if (!guard.entered) { Broken(admitted ? "S3, a second hook while one was running" : "S4, a second hook while one was running"); return false; }
    if (At<BYTE*>(frame, kick::kStreamer) != str) {
        Broken(admitted ? "S3, the kick frame is not where it should be" : "S4, the kick frame is not where it should be");
        return false;
    }

    uint64_t now = BeginEvent(str, frame);
    TextureState& state = Touch(tex, now);
    int current = At<int32_t>(tex, texture::kCurrentLevel);
    int64_t tiles = TilesBetween(tex, keep, current);
    Feedback fb = ReadFeedback(str, tex);

    Verdict verdict = Judge(state, keep, current, fb, str, admitted);
    bool refuse = verdict == Verdict::Refuse && g_cfg.streamerReloadFix;

    if (refuse) {
        s_tally.refused++;
        g_refused++;
        g_refusedTiles += (uint64_t)tiles;
    } else {
        if (verdict == Verdict::Refuse) g_wouldRefuse++;
        // The pool margin is a reason to let this one drop, not to forget the flip.
        if (verdict != Verdict::Refuse && verdict != Verdict::PoolMargin) state.pinFine = -1;
        state.dropKick = now;
        state.dropFrom = current;
        state.dropTo = keep;
        state.dropMip = fb.mip;
        s_tally.drops++;
        g_drops++;
        if (!admitted) g_dropsUnadmitted++;
    }

    if (TraceOn()) {
        TraceRow(admitted ? "drop" : "drop0", "%llu,%p,%d,%d,%d,%lld,%d,%u,%u,%d,%d,%lld",
            (unsigned long long)now, tex, current, keep, state.levels, (long long)tiles,
            fb.mip, fb.count, fb.age, refuse ? 1 : 0, (int)verdict, (long long)GateSpace(frame));
    }
    return refuse;
}

// ---------------------------------------------------------------------------
// The hooks the stubs jump to. They keep to the calls they replace: the level read returns the
// level, the drops either run dropLevels or return without it.
// ---------------------------------------------------------------------------

static int HookStreamerLevel(BYTE* tex, int admitted, BYTE* context)
{
    int current = At<int32_t>(tex, texture::kCurrentLevel);
    if (g_broken.load(std::memory_order_relaxed)) return current;
    __try {
        OnLevel(tex, admitted, current, context);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Broken("S2");
    }
    return current;
}

static void HookStreamerDrop(BYTE* tex, int keep, BYTE* str, BYTE* frame)
{
    bool refuse = false;
    if (!g_broken.load(std::memory_order_relaxed)) {
        __try {
            refuse = OnDrop(tex, keep, str, frame, true);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Broken("S3");
            refuse = false;
        }
    }
    if (!refuse) g_dropLevels(tex, keep);
}

static void HookStreamerDropUnadmitted(BYTE* tex, int keep, BYTE* str, BYTE* frame)
{
    bool refuse = false;
    if (!g_broken.load(std::memory_order_relaxed)) {
        __try {
            refuse = OnDrop(tex, keep, str, frame, false);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Broken("S4");
            refuse = false;
        }
    }
    if (!refuse) g_dropLevels(tex, keep);
}

// ---------------------------------------------------------------------------
// Installing
// ---------------------------------------------------------------------------

// A 32 bit displacement reaches 2 GB either way, so the stubs have to live near the exe.
static BYTE* AllocNear(BYTE* anchor, size_t size)
{
    SYSTEM_INFO si = {};
    GetSystemInfo(&si);
    const uintptr_t granularity = si.dwAllocationGranularity ? si.dwAllocationGranularity : 0x10000;
    const uintptr_t reach = 0x60000000ull;   // well inside 2 GB, leaving room for the exe itself

    for (uintptr_t delta = granularity; delta < reach; delta += granularity) {
        const uintptr_t base = (uintptr_t)anchor;
        const uintptr_t candidates[2] = { base + delta, base > delta ? base - delta : 0 };
        for (uintptr_t addr : candidates) {
            if (!addr) continue;
            void* p = VirtualAlloc((void*)(addr & ~(granularity - 1)), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (p) return (BYTE*)p;
        }
    }
    return nullptr;
}

struct Emitter {
    BYTE* at;

    void Bytes(std::initializer_list<BYTE> bytes)
    {
        for (BYTE b : bytes) *at++ = b;
    }

    // inc qword ptr [rip + disp32]
    void IncrementQword(const BYTE* counter)
    {
        Bytes({ 0x48, 0xFF, 0x05 });
        int32_t disp = (int32_t)(counter - (at + 4));
        memcpy(at, &disp, 4);
        at += 4;
    }

    // jmp qword ptr [rip], followed by the absolute target
    void JumpTo(void* target)
    {
        Bytes({ 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 });
        memcpy(at, &target, 8);
        at += 8;
    }
};

void InstallStreamerHooks()
{
    if (!g_cfg.streamingTrace && !g_cfg.streamerReloadFix && !g_cfg.streamerCensus) return;

    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    auto nt = (IMAGE_NT_HEADERS64*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kTimeDateStamp || nt->OptionalHeader.SizeOfImage != kSizeOfImage) {
        Log("[streamer] this is not the game build the streamer hooks were written for (stamp %08X, image %08X), nothing patched. "
            "The game streams exactly as it would without the mod's fix.",
            (unsigned)nt->FileHeader.TimeDateStamp, (unsigned)nt->OptionalHeader.SizeOfImage);
        return;
    }

    for (const Region& region : kRegions) {
        if (Fnv1a64(base + region.rva, region.length) != region.fnv1a64) {
            Log("[streamer] %s at rva 0x%07X is not the code this was written against, nothing patched. "
                "The game streams exactly as it would without the mod's fix.", region.what, (unsigned)region.rva);
            return;
        }
    }

    g_censusOn = g_cfg.streamerCensus;
    for (const Region& region : kCensusRegions) {
        if (!g_censusOn || Fnv1a64(base + region.rva, region.length) == region.fnv1a64) continue;
        Log("[streamer] %s at rva 0x%07X is not the code the census was written against, census off", region.what, (unsigned)region.rva);
        g_censusOn = false;
    }

    const Site* sites[] = { &kSiteDenied, &kSiteLevel, &kSiteDrop, &kSiteDrop0, &kSiteKick };
    for (const Site* site : sites) {
        int32_t rel = At<int32_t>(base + site->rva, site->dispOffset);
        if ((int64_t)site->rva + site->length + rel != (int64_t)site->target) {
            Log("[streamer] site %s at rva 0x%07X does not point where it should, nothing patched", site->what, (unsigned)site->rva);
            return;
        }
    }

    const size_t page = 0x1000;
    BYTE* cave = AllocNear(base + kRvaKick, 2 * page);
    if (!cave) {
        Log("[streamer] no free memory within reach of the exe, nothing patched");
        return;
    }
    BYTE* counters = cave + page;
    g_kickCount = (const uint64_t*)counters;
    g_deniedCount = (const uint64_t*)(counters + 8);
    g_dropLevels = (DropLevelsFn)(base + kRvaDropLevels);

    // mov edx, [rbx+8] is the admitted level, r14 the context. mov r8, r12 is the streamer and
    // mov r9, rbp the kick frame, at both drops.
    Emitter emit{ cave };
    BYTE* deniedStub = emit.at;
    emit.IncrementQword(counters + 8);
    emit.JumpTo(base + kSiteDenied.target);
    BYTE* levelStub = emit.at;
    emit.Bytes({ 0x8B, 0x53, 0x08, 0x4D, 0x8B, 0xC6 });
    emit.JumpTo(&HookStreamerLevel);
    BYTE* dropStub = emit.at;
    emit.Bytes({ 0x4D, 0x8B, 0xC4, 0x4C, 0x8B, 0xCD });
    emit.JumpTo(&HookStreamerDrop);
    BYTE* drop0Stub = emit.at;
    emit.Bytes({ 0x4D, 0x8B, 0xC4, 0x4C, 0x8B, 0xCD });
    emit.JumpTo(&HookStreamerDropUnadmitted);
    BYTE* kickStub = emit.at;
    emit.IncrementQword(counters);
    emit.JumpTo(base + kRvaKick);

    DWORD old = 0;
    if (!VirtualProtect(cave, page, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[streamer] could not make the stubs executable, nothing patched");
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), cave, page);

    // Every displacement is computed before any write, so a stub out of reach patches nothing.
    BYTE* stubs[] = { deniedStub, levelStub, dropStub, drop0Stub, kickStub };
    int32_t rels[5] = {};
    for (int i = 0; i < 5; ++i) {
        int64_t rel = stubs[i] - (base + sites[i]->rva + sites[i]->length);
        if (rel > INT32_MAX || rel < INT32_MIN) {
            VirtualFree(cave, 0, MEM_RELEASE);
            Log("[streamer] site %s is out of reach of its stub, nothing patched", sites[i]->what);
            return;
        }
        rels[i] = (int32_t)rel;
    }

    BYTE* lo = base + sites[0]->rva;
    BYTE* hi = lo;
    for (const Site* site : sites) {
        lo = std::min(lo, base + site->rva);
        hi = std::max(hi, base + site->rva + site->length);
    }
    if (!VirtualProtect(lo, hi - lo, PAGE_EXECUTE_READWRITE, &old)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[streamer] could not make the exe's code writable, nothing patched");
        return;
    }
    for (int i = 0; i < 5; ++i) memcpy(base + sites[i]->rva + sites[i]->dispOffset, &rels[i], 4);
    DWORD ignored = 0;
    VirtualProtect(lo, hi - lo, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), lo, hi - lo);

    g_installed = true;
    Log("[streamer] hooked the texture streamer: kicks, admitted levels, drops and loads turned away for space. "
        "Reload fix %s, streaming trace %s.",
        g_cfg.streamerReloadFix ? "on, flipping textures keep their finer mip while the engine's tile margin is free" : "off, it only reports what it would refuse",
        g_cfg.streamingTrace ? "on" : "off");
    if (g_censusOn) Log("[streamer] census on, every kick's demand and ranked records go to acevo_perf_census.bin");
}

// ---------------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------------

static bool ReadPoolLine(int64_t* used, int64_t* total, int64_t* pending)
{
    const BYTE* alloc = g_allocator.load();
    if (!alloc) return false;
    __try {
        const BYTE* tilePool = At<BYTE*>(alloc, allocator::kPool);
        if (!tilePool) return false;
        *total = At<int32_t>(tilePool, pool::kTotal);
        *used = (int64_t)At<uint64_t>(tilePool, pool::kUsed);
        *pending = At<int32_t>(alloc, allocator::kPending);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static void Report(const char* when)
{
    int64_t used = 0, total = 0, pending = 0;
    bool havePool = ReadPoolLine(&used, &total, &pending);
    Log("[streamer]%s kicks %llu | wanted a finer level %llu, loads turned away for space %llu | drops %llu (%llu not admitted at all) | "
        "refused %llu drops holding %.1f MB that would have been reloaded, would refuse %llu, flips pinned %llu | tile pool %lld of %lld used, %lld pending%s",
        when, (unsigned long long)g_kicks.load(), (unsigned long long)g_wantsFiner.load(),
        (unsigned long long)(g_deniedCount ? *g_deniedCount : 0),
        (unsigned long long)g_drops.load(), (unsigned long long)g_dropsUnadmitted.load(),
        (unsigned long long)g_refused.load(), g_refusedTiles.load() * 64.0 / 1024.0,
        (unsigned long long)g_wouldRefuse.load(), (unsigned long long)g_pins.load(),
        (long long)used, (long long)total, (long long)pending, havePool ? "" : " (not read yet)");
}

void StreamerTick()
{
    TraceFlush();
    if (!g_installed) return;
    CensusFlush();

    static uint64_t lastReport = GetTickCount64();
    uint64_t now = GetTickCount64();
    if (now - lastReport < (uint64_t)g_cfg.statsIntervalS * 1000ull) return;
    lastReport = now;
    Report("");
}

void StreamerDetach()
{
    if (g_installed) Report(" at exit:");
    TraceFinalFlush();
    CensusFinalFlush();
}
