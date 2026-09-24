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
// Two more things go wrong once the pool is contended, in any race with a field of cars (BUG-020).
//
// A texture ranked by its least important use. Every object that draws a texture asks for it with
// its own priority, and the kick has to keep one record per texture level. It sorts the records of
// one level by ascending priority and keeps the first, so the lowest request wins. The player's
// car asks at 60000 and an AI car of the same model 50 m away asks for the same livery at 11250,
// so the player's own car falls out of the pool whenever it has company, and a grass texture drawn
// both under the camera and a kilometre away is ranked as if it were only far away. In a thirty car
// race every one of 7036 records whose requests differed kept the lowest. The fix sorts those
// records highest first, so the engine's own dedupe keeps the most important request.
//
// A load that is all or nothing. The load step sums every level between the current one and the
// admitted one and starts nothing unless the whole sum fits the free space at that texture's turn.
// A 4096 texture at its coarsest level needs 340 tiles in one piece, the space left at its turn is
// usually a few dozen to a couple of hundred, so it waits at 256 by 256 while smaller textures take
// the space. The player's livery waited 72 kicks after a race start with enough room for the three
// levels below its finest on most of them. The fix loads the levels that fit, and the rest follow
// on later kicks as space comes back.
//
// The seven sites, each the rewrite of one rel32 displacement:
//
//   S1  the lea that hands the kick job its function, through a stub that counts kicks
//   S2  the selected update's read of the current level, which also sees the admitted level
//   S3  the selected update's tail jump into dropLevels, a drop to a lower admitted level
//   S4  the kick's call of dropLevels(texture, 0) for a texture not admitted at all
//   D   the jump taken when a load does not fit the free pool space, through a stub that counts it
//       or, with partial loads on, one that finds the levels that do fit
//   R   the call of the sort in front of the record dedupe
//
// Why this is safe to do:
//
//   - Every region the hooks rely on is compared with this build byte for byte before anything
//     is written, and none of those ranges carries a base relocation. A game update that touches
//     any of it means nothing is patched and the log names the region that moved.
//   - Only 4 byte displacements change, so no instruction boundary moves. The selected update has
//     a single caller, the kick, and the kick thunk has a single reference, the S1 lea. The sort R
//     calls is shared with other code, so only this one call is moved, and the replacement orders
//     plain 16 byte records by the same keys with priority reversed.
//   - A partial load goes down the engine's own load path with a lower target level and the tiles
//     of just those levels, the same pair the whole step would have used, so the gate, the started
//     count and the load job see a load they already handle. A running job below the admitted level
//     is left to finish, and the next kick asks for the rest.
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
// Declared rather than included: this file has its own static Fnv1a64 and AllocNear, and pulling in
// core/code_patch.h would redeclare those as extern first, which the compiler warns about.
bool CodePatchingIsLate();
#include "acevo/telemetry/streaming_trace.h"
#include <algorithm>
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
    { 0x1F2E0DA, 0x0CA, 0x15B32D00FC948EEDull, "the record dedupe (R)" },
    { 0x1F17490, 0x010, 0x1F4BF375CADBE480ull, "the engine's record sort" },
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
static const Site kSiteSort = { "R", 0x1F2E0F4, 1, 5, 0x1F17490 };      // call the record sort

static const uint32_t kRvaKick = 0x1F2D110;
static const uint32_t kRvaDropLevels = 0x1C82390;
static const uint32_t kRvaLoadPath = 0x1F21381;    // the fall through of D, the load itself
static const uint32_t kRvaRecordSort = 0x1F17490;

// The partial load stub, reached through D when a texture's whole step does not fit. At D the
// engine holds the node in r15, the admitted level in r10d, the gate space in ecx and the tiles of
// the whole step in esi, and neither path after D reads rax, rdx, r8, r9 or r11 before writing
// them. The stub climbs from the current level while the running sum still fits the gate, then
// enters the load path with that level in r10d and its tiles in esi, or counts a denial and takes
// the engine's skip when not even the next level fits. The admitted level itself is never tried,
// the engine has just found that the whole step does not fit. Once a hook has faulted the stub
// takes the engine's skip straight away, like every other hook passing through.
static const BYTE kPartialLoadStub[] = {
    0x80, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00,       // 00  cmp byte [rip+broken], 0
    0x75, 0x75,                                     // 07  jne 7E
    0x49, 0x8B, 0x07,                               // 09  mov rax, [r15]          the texture
    0x4C, 0x8B, 0x98, 0x60, 0x01, 0x00, 0x00,       // 0C  mov r11, [rax+160h]     its level entries
    0x48, 0x8B, 0x90, 0x68, 0x01, 0x00, 0x00,       // 13  mov rdx, [rax+168h]
    0x4C, 0x29, 0xDA,                               // 1A  sub rdx, r11            their size in bytes
    0x44, 0x8B, 0x80, 0x7C, 0x01, 0x00, 0x00,       // 1D  mov r8d, [rax+17Ch]     target so far, the current level
    0x45, 0x89, 0xC1,                               // 24  mov r9d, r8d
    0x31, 0xF6,                                     // 27  xor esi, esi
    0x41, 0xFF, 0xC1,                               // 29  inc r9d
    0x45, 0x39, 0xD1,                               // 2C  cmp r9d, r10d
    0x7D, 0x22,                                     // 2F  jge 53
    0x49, 0x63, 0xC1,                               // 31  movsxd rax, r9d
    0x48, 0x6B, 0xC0, 0x30,                         // 34  imul rax, rax, 48
    0x48, 0x83, 0xC0, 0x30,                         // 38  add rax, 48
    0x48, 0x39, 0xD0,                               // 3C  cmp rax, rdx
    0x77, 0x12,                                     // 3F  ja 53
    0x41, 0x8B, 0x44, 0x03, 0xD8,                   // 41  mov eax, [r11+rax-28h]  the level's tiles
    0x01, 0xF0,                                     // 46  add eax, esi
    0x39, 0xC8,                                     // 48  cmp eax, ecx
    0x7F, 0x07,                                     // 4A  jg 53
    0x89, 0xC6,                                     // 4C  mov esi, eax
    0x45, 0x89, 0xC8,                               // 4E  mov r8d, r9d
    0xEB, 0xD6,                                     // 51  jmp 29
    0x49, 0x8B, 0x07,                               // 53  mov rax, [r15]
    0x44, 0x3B, 0x80, 0x7C, 0x01, 0x00, 0x00,       // 56  cmp r8d, [rax+17Ch]
    0x74, 0x1F,                                     // 5D  je 7E
    0x45, 0x89, 0xC2,                               // 5F  mov r10d, r8d
    0x48, 0xFF, 0x05, 0x00, 0x00, 0x00, 0x00,       // 62  inc qword [rip+partial loads]
    0x48, 0x01, 0x35, 0x00, 0x00, 0x00, 0x00,       // 69  add [rip+partial tiles], rsi
    0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,             // 70  jmp [rip]
    0, 0, 0, 0, 0, 0, 0, 0,                         // 76    the load path
    0x48, 0xFF, 0x05, 0x00, 0x00, 0x00, 0x00,       // 7E  inc qword [rip+denied]
    0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,             // 85  jmp [rip]
    0, 0, 0, 0, 0, 0, 0, 0,                         // 8B    the engine's skip
};

struct StubPatch {
    uint8_t dispAt;     // a rip relative disp32, or an absolute qword when nextIp is 0
    uint8_t nextIp;
};

static const StubPatch kPartialBroken = { 0x02, 0x07 };
static const StubPatch kPartialCount = { 0x65, 0x69 };
static const StubPatch kPartialTiles = { 0x6C, 0x70 };
static const StubPatch kPartialLoad = { 0x76, 0 };
static const StubPatch kPartialDenied = { 0x81, 0x85 };
static const StubPatch kPartialSkip = { 0x8B, 0 };

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
constexpr ptrdiff_t kFeedback = 0xD0;
constexpr ptrdiff_t kCap = 0x148;
constexpr ptrdiff_t kAvail = 0x150;
constexpr ptrdiff_t kRecords = 0x154;
constexpr ptrdiff_t kAdmitted = 0x158;
constexpr ptrdiff_t kAdmittedTiles = 0x15C;
constexpr ptrdiff_t kRejected = 0x160;
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
using RecordSortFn = void (*)(BYTE* first, BYTE* last, ptrdiff_t count, uint8_t predicate);

static bool g_installed = false;
static DropLevelsFn g_dropLevels = nullptr;
static RecordSortFn g_recordSort = nullptr;
static const uint64_t* g_kickCount = nullptr;       // bumped by the S1 stub
static const uint64_t* g_deniedCount = nullptr;     // bumped by the D stub
static const uint64_t* g_partialCount = nullptr;    // bumped by the partial load stub
static const uint64_t* g_partialTiles = nullptr;
static BYTE* g_brokenFlag = nullptr;                // the partial load stub's copy of g_broken
static std::atomic<uint64_t> g_rankSorts{0};
static std::atomic<bool> g_broken{false};
static std::atomic<bool> g_inHook{false};
static std::atomic<bool> g_overlapSaid{false};

// The tile pool as the last kick read it, for the report. The report runs on the timeline thread,
// which outlives the engine's allocator at exit, and catching a fault there is not enough, the
// game's crash handler sees it first and writes a crash report naming the mod (BUG-022).
static std::atomic<int64_t> g_poolUsed{0}, g_poolTotal{0}, g_poolPending{0};
static std::atomic<bool> g_poolRead{false};

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
static uint64_t s_partialAtKickStart = 0;
static uint64_t s_partialLastKick = 0;
static uint64_t s_partialTilesAtKickStart = 0;
static uint64_t s_partialTilesLastKick = 0;
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
    if (!alloc) return fb;
    uint32_t id = (uint32_t)(At<int32_t>(tex, texture::kFeedbackSlot) - At<int32_t>(alloc, allocator::kFeedbackBase));
    if (id >= At<uint32_t>(fbObject, feedback::kIds)) return fb;

    fb.age = At<uint16_t>(At<BYTE*>(fbObject, feedback::kAges), (ptrdiff_t)id * 2);
    if (fb.age >= At<uint32_t>(fbObject, feedback::kAgeLimit)) return fb;

    fb.mip = At<int32_t>(At<BYTE*>(fbObject, feedback::kMips), (ptrdiff_t)id * 4);
    fb.count = At<uint32_t>(At<BYTE*>(fbObject, feedback::kCounts), (ptrdiff_t)id * 4);
    return fb;
}

struct PoolFigures {
    int64_t used = 0;
    int64_t total = 0;
    int64_t pending = 0;
};

// Only from inside a kick, where the streamer's allocator is alive.
static bool ReadPool(const BYTE* str, PoolFigures* figures)
{
    const BYTE* alloc = At<BYTE*>(str, streamer::kAllocator);
    const BYTE* tilePool = alloc ? At<BYTE*>(alloc, allocator::kPool) : nullptr;
    if (!tilePool) return false;

    figures->total = At<int32_t>(tilePool, pool::kTotal);
    figures->used = (int64_t)At<uint64_t>(tilePool, pool::kUsed);
    figures->pending = At<int32_t>(alloc, allocator::kPending);
    return true;
}

// A refused drop's tiles are already counted in used, so this is the margin as it stands.
static bool PoolKeepsMargin(const BYTE* str)
{
    PoolFigures figures;
    if (!ReadPool(str, &figures)) return false;
    return figures.used + figures.pending + kPoolMarginTiles <= figures.total;
}

// ---------------------------------------------------------------------------
// The decisions
// ---------------------------------------------------------------------------

static void Broken(const char* where)
{
    if (g_brokenFlag) *g_brokenFlag = 1;
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
    uint64_t partial = *g_partialCount;
    s_partialLastKick = partial - s_partialAtKickStart;
    s_partialAtKickStart = partial;
    uint64_t partialTiles = *g_partialTiles;
    s_partialTilesLastKick = partialTiles - s_partialTilesAtKickStart;
    s_partialTilesAtKickStart = partialTiles;
    PoolFigures figures;
    if (ReadPool(str, &figures)) {
        g_poolUsed.store(figures.used);
        g_poolTotal.store(figures.total);
        g_poolPending.store(figures.pending);
        g_poolRead.store(true);
    }
    g_kicks++;

    TraceRow("kick", "%llu,%d,%d,%d,%d,%d,%d,%lld,%u,%u,%llu,%u,%u,%llu,%llu",
        (unsigned long long)now, At<int32_t>(str, streamer::kCap), At<int32_t>(str, streamer::kAvail),
        At<int32_t>(str, streamer::kRecords), At<int32_t>(str, streamer::kAdmitted), At<int32_t>(str, streamer::kAdmittedTiles),
        At<int32_t>(str, streamer::kRejected), (long long)GateSpace(frame), (unsigned)At<uint8_t>(frame, kick::kGate),
        s_tally.wantsFiner, (unsigned long long)s_deniedLastKick, s_tally.drops, s_tally.refused,
        (unsigned long long)s_partialLastKick, (unsigned long long)s_partialTilesLastKick);

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

// An access violation the hooks' __except catches leaves g_inHook set, since /EHsc runs no destructor for
// it, which does not matter while Broken latches every hook off on the same path.
struct HookGuard {
    bool entered;
    HookGuard() : entered(!g_inHook.exchange(true)) {}
    ~HookGuard() { if (entered) g_inHook.store(false); }
};

// Two hooks at once would share the texture table and the tally unguarded, so the second goes straight to
// the engine for that one call. It once switched every fix off for the session instead, which cost far
// more than the one event it loses.
static void SkipOverlap(const char* where)
{
    if (!g_overlapSaid.exchange(true))
        Log("[streamer] a second hook arrived in %s while one was running, so that call went straight to the engine "
            "and the fixes stay on. Only the first time is logged.", where);
}

static void OnLevel(const BYTE* tex, int admitted, int current, const BYTE* context)
{
    HookGuard guard;
    if (!guard.entered) { SkipOverlap("S2"); return; }

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
    if (!guard.entered) { SkipOverlap(admitted ? "S3" : "S4"); return false; }
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

// The kick's record, as the admission loop and the dedupe read it.
struct Record {
    uint64_t pathHash;
    uint16_t priority;
    uint8_t requested;      // 1 in every record a kick builds, the engine orders set before clear
    uint8_t pad0;
    uint16_t tiles;
    uint8_t level;
    uint8_t pad1;
};
static_assert(sizeof(Record) == 16, "the kick's records are 16 bytes");

static void SortRecordsHighestFirst(BYTE* first, BYTE* last)
{
    std::sort((Record*)first, (Record*)last, [](const Record& a, const Record& b) {
        if (a.pathHash != b.pathHash) return a.pathHash < b.pathHash;
        if (a.level != b.level) return a.level < b.level;
        if ((a.requested != 0) != (b.requested != 0)) return a.requested != 0;
        return a.priority > b.priority;
    });
}

// Replaces the one call of the engine's sort in front of the record dedupe. The dedupe keeps the
// first record of each texture level, so ordering them highest first keeps the most important
// request. With the fix off, or once a hook has faulted, the engine's own sort runs as before.
static void HookRecordSort(BYTE* first, BYTE* last, ptrdiff_t count, uint8_t predicate)
{
    bool sorted = false;
    if (g_cfg.streamerRankFix && !g_broken.load(std::memory_order_relaxed)) {
        __try {
            SortRecordsHighestFirst(first, last);
            sorted = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Broken("R");
        }
    }
    if (sorted) {
        g_rankSorts++;
        return;
    }
    g_recordSort(first, last, count, predicate);
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

    BYTE* Copy(const BYTE* code, size_t size)
    {
        BYTE* start = at;
        memcpy(at, code, size);
        at += size;
        return start;
    }
};

static void PatchStub(BYTE* stub, StubPatch patch, const BYTE* target)
{
    if (!patch.nextIp) {
        memcpy(stub + patch.dispAt, &target, 8);
        return;
    }
    int32_t disp = (int32_t)(target - (stub + patch.nextIp));
    memcpy(stub + patch.dispAt, &disp, 4);
}

void InstallStreamerHooks()
{
    bool anyFix = g_cfg.streamerReloadFix || g_cfg.streamerRankFix || g_cfg.streamerPartialLoads;
    if (!g_cfg.streamingTrace && !anyFix) return;

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

    const Site* sites[] = { &kSiteDenied, &kSiteLevel, &kSiteDrop, &kSiteDrop0, &kSiteKick, &kSiteSort };
    const size_t siteCount = sizeof sites / sizeof sites[0];
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
    g_partialCount = (const uint64_t*)(counters + 16);
    g_partialTiles = (const uint64_t*)(counters + 24);
    g_brokenFlag = counters + 32;
    g_dropLevels = (DropLevelsFn)(base + kRvaDropLevels);
    g_recordSort = (RecordSortFn)(base + kRvaRecordSort);

    // mov edx, [rbx+8] is the admitted level, r14 the context. mov r8, r12 is the streamer and
    // mov r9, rbp the kick frame, at both drops.
    Emitter emit{ cave };
    BYTE* deniedStub = emit.at;
    if (g_cfg.streamerPartialLoads) {
        emit.Copy(kPartialLoadStub, sizeof kPartialLoadStub);
        PatchStub(deniedStub, kPartialBroken, g_brokenFlag);
        PatchStub(deniedStub, kPartialCount, counters + 16);
        PatchStub(deniedStub, kPartialTiles, counters + 24);
        PatchStub(deniedStub, kPartialLoad, base + kRvaLoadPath);
        PatchStub(deniedStub, kPartialDenied, counters + 8);
        PatchStub(deniedStub, kPartialSkip, base + kSiteDenied.target);
    } else {
        emit.IncrementQword(counters + 8);
        emit.JumpTo(base + kSiteDenied.target);
    }
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
    BYTE* sortStub = emit.at;
    emit.JumpTo(&HookRecordSort);

    DWORD old = 0;
    if (!VirtualProtect(cave, page, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[streamer] could not make the stubs executable, nothing patched");
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), cave, page);

    // Every displacement is computed before any write, so a stub out of reach patches nothing.
    BYTE* stubs[] = { deniedStub, levelStub, dropStub, drop0Stub, kickStub, sortStub };
    int32_t rels[siteCount] = {};
    for (size_t i = 0; i < siteCount; ++i) {
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
    if (CodePatchingIsLate())
        Log("[streamer] WARNING: patching the exe's code after start up, with the game's own threads running. One executing these bytes mid write will crash. See WriteCode.");
    if (!VirtualProtect(lo, hi - lo, PAGE_EXECUTE_READWRITE, &old)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[streamer] could not make the exe's code writable, nothing patched");
        return;
    }
    for (size_t i = 0; i < siteCount; ++i) memcpy(base + sites[i]->rva + sites[i]->dispOffset, &rels[i], 4);
    DWORD ignored = 0;
    VirtualProtect(lo, hi - lo, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), lo, hi - lo);

    g_installed = true;
    Log("[streamer] hooked the texture streamer: kicks, admitted levels, drops, loads turned away for space and the record ranking. "
        "Reload fix %s. Rank fix %s. Partial loads %s. Streaming trace %s.",
        g_cfg.streamerReloadFix ? "on, flipping textures keep their finer mip while the engine's tile margin is free" : "off, it only reports what it would refuse",
        g_cfg.streamerRankFix ? "on, a texture level keeps its most important request" : "off",
        g_cfg.streamerPartialLoads ? "on, a texture loads the levels that fit when its whole step does not" : "off",
        g_cfg.streamingTrace ? "on" : "off");
}

// ---------------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------------

static void Report(const char* when)
{
    Log("[streamer]%s kicks %llu | wanted a finer level %llu, loads turned away for space %llu, loads cut to what fits %llu (%.1f MB) | "
        "drops %llu (%llu not admitted at all) | refused %llu drops holding %.1f MB that would have been reloaded, would refuse %llu, "
        "flips pinned %llu | kicks ranked by the most important request %llu | tile pool %lld of %lld used, %lld pending%s",
        when, (unsigned long long)g_kicks.load(), (unsigned long long)g_wantsFiner.load(),
        (unsigned long long)(g_deniedCount ? *g_deniedCount : 0),
        (unsigned long long)(g_partialCount ? *g_partialCount : 0), (g_partialTiles ? *g_partialTiles : 0) * 64.0 / 1024.0,
        (unsigned long long)g_drops.load(), (unsigned long long)g_dropsUnadmitted.load(),
        (unsigned long long)g_refused.load(), g_refusedTiles.load() * 64.0 / 1024.0,
        (unsigned long long)g_wouldRefuse.load(), (unsigned long long)g_pins.load(),
        (unsigned long long)g_rankSorts.load(),
        (long long)g_poolUsed.load(), (long long)g_poolTotal.load(), (long long)g_poolPending.load(),
        g_poolRead.load() ? "" : " (not read yet)");
}

void StreamerTick()
{
    TraceFlush();
    if (!g_installed) return;

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
}
