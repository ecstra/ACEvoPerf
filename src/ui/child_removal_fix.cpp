#include "acevo/ui/child_removal_fix.h"
#include "acevo/core/code_patch.h"
#include "acevo/core/log.h"
#include <unordered_map>

// Read from cohtml.WindowsDesktop.dll 1.61.0.3, measured with the UI probe on 2026-09-15 (BUG-029).
//
// When an element loses a child, Cohtml invalidates the element for the change of its child list: the
// removal (0x38FCB4) calls the element's slot 49 (0x37B600), which invalidates with kind 0. That marks the
// element itself and every node under it that the kind 0 invalidation set matches, and the feature set's
// constructor (0x3E8C10) makes that set match every node, whatever the stylesheets hold. The style update
// then restyles every marked node and what is under it. On the HUD the wrong way label's data-bind-if takes
// the label out of the HUD's top level when the car stops going the wrong way, and that restyled all 879
// HUD nodes, 21 to 24 ms, every time.
//
// A removal can only change the style of the element's other children, through the rules that look at
// siblings: the :first-child, :last-child, :only-child and :nth-child pseudo-classes (subtypes 5 to 8 in the
// simple selector matcher at 0x3ED9D0) and the + and ~ combinators (2 and 3 in 0x3EE1D0). Cohtml 1.61 has
// no :empty, :has or :not, so neither the element nor anything else under it depends on its child list,
// and a child's own subtree changes only through the child. So this part keeps, for each feature set, the
// compounds that hold such a pseudo-class or sit right of + or ~ (the element matched there is the one
// whose siblings are looked at), from the selectors Cohtml adds to the set (0x3E9F41). On a removal it marks
// only the children that could match one of them, with Cohtml's own mark (0x37B5A0), the call Cohtml's
// cheaper path makes per child when a style scope has no longer rules. A compound with no tag, id, class or
// attribute to tell elements apart, a feature set not seen from its construction, or an element that is
// not a plain element takes Cohtml's own invalidation.

static const DWORD kCohtmlTimeDateStamp = 0x675439C7;
static const DWORD kCohtmlSizeOfImage = 0x0070F000;

struct Region {
    uint32_t rva;
    uint32_t length;
    uint64_t fnv1a64;
    const char* what;
};

static const Region kRegions[] = {
    { 0x38FCA7, 0x1D, 0x878956827B6E76C1ull, "the removal's child list invalidation" },
    { 0x3E9EA9, 0x9D, 0xBBFE4D44794862CFull, "the rule filing's feature add" },
    { 0x3E8F80, 0x80, 0x2A3FE73218BB7F00ull, "the feature add's walk over compounds" },
    { 0x3E8E27, 0x63, 0xAE089B120D46FC7Bull, "the feature set constructor's child list set" },
    { 0x37BC85, 0x7A, 0x5FBCC7DD55F643E3ull, "the invalidation's feature set lookup" },
    { 0x37B5A0, 0x5C, 0xC6927D04B0E4F526ull, "the mark of one node" },
    { 0x3EDA01, 0x4F, 0xA4533F967AFAE66Dull, "the matcher's class and id checks" },
    { 0x3EDA50, 0x26, 0x8A19B6FA743ACF6Cull, "the matcher's tag check" },
    { 0x3EDAF9, 0xC8, 0x32FF532EBEA7734Dull, "the matcher's pseudo-classes" },
    { 0x3ED357, 0x67, 0x4F9A00AD5E2D52BAull, "the matcher's attribute read" },
    { 0x3EE225, 0x90, 0x1BB527C869397416ull, "the matcher's combinators" },
    { 0x1AE4C0, 0x48, 0xBF112458BCEE183Dull, "the custom element name getter" },
};

static const uint32_t kRvaFeatureSetConstructor = 0x3E8C10;
static const uint32_t kRvaAddRuleFeatures = 0x3E8F80;
static const uint32_t kRvaMarkNode = 0x37B5A0;
static const uint32_t kRvaCustomNameGetter = 0x1AE4C0;
static const uint32_t kRvaClassAttributeAtom = 0x6AF318;    // the attribute the matcher reads from the class list
static const uint32_t kRvaRemovalInvalidation = 0x38FCB4;   // call qword ptr [rax+0x188], six bytes
static const uint32_t kRvaAddRuleFeaturesCall = 0x3E9F41;
static const uint32_t kConstructorCalls[] = { 0x23C811, 0x26DA15, 0x26F007, 0x34B422, 0x3805BA, 0x3C6A54 };
static const int kSlotChildListInvalidation = 49;
static const int kSlotNameGetter = 0x130 / 8;

namespace node {
    static const ptrdiff_t kKind = 0x20;            // bit 0 set for an element, 0x600 for a pseudo element
    static const ptrdiff_t kTree = 0x24;            // bit 0 set while the node is in the document
    static const ptrdiff_t kType = 0x28;
    static const ptrdiff_t kDocument = 0x38;
    static const ptrdiff_t kChildren = 0xB8;
    static const ptrdiff_t kChildCount = 0xC0;
    static const ptrdiff_t kAttributes = 0xD8;      // entries at +0x18, count at +0x20, 0x20 bytes each, name first
    static const ptrdiff_t kStyleScope = 0xE8;
    static const ptrdiff_t kId = 0x1E8;
    static const ptrdiff_t kClasses = 0x1F0;
    static const ptrdiff_t kClassCount = 0x1F8;
    static const ptrdiff_t kCustomName = 0x238;     // bit 0 of the first byte set when the name is kept in place
}

// A selector starts with its rightmost compound, the others follow at +0x40 and the combinators between
// them at +0x50. A compound starts with its first simple selector, the others follow at +0x30. The walk in
// 0x3E8F80 and the matcher read them this way.
namespace selector {
    static const ptrdiff_t kCompounds = 0x40;       // compound 1 onwards, 0x40 bytes each
    static const ptrdiff_t kCompoundCount = 0x48;   // compounds after the rightmost
    static const ptrdiff_t kCombinators = 0x50;     // combinator i joins compound i to compound i + 1
    static const ptrdiff_t kCombinatorCount = 0x58;
    static const ptrdiff_t kSimples = 0x30;         // simple selector 1 onwards, 0x30 bytes each
    static const ptrdiff_t kSimpleCount = 0x38;
    static const ptrdiff_t kValue = 8;              // tag type, id, class or attribute atom, pseudo-class subtype
    static const ptrdiff_t kCustomName = 0x10;      // a custom tag's name
}

enum SimpleType : BYTE { kUniversal = 0, kTag = 1, kId = 2, kClass = 3, kPseudoClass = 5, kAttribute = 7 };
static const BYTE kFirstChild = 5;
static const BYTE kNthChild = 8;
static const BYTE kAdjacentSibling = 2;
static const BYTE kGeneralSibling = 3;

// What one compound needs of an element, all of it. A custom tag also needs its name.
struct Compound {
    bool hasType = false;
    BYTE type = 0;
    std::string customName;         // lower case
    const void* id = nullptr;
    std::vector<const void*> classes;
    std::vector<const void*> attributes;

    bool operator==(const Compound& other) const
    {
        return hasType == other.hasType && type == other.type && customName == other.customName && id == other.id &&
            classes == other.classes && attributes == other.attributes;
    }
};

struct FeatureSet {
    bool anyChild = false;          // a compound that could match any element
    std::vector<Compound> compounds;
};

static const size_t kMaxFeatureSets = 4096;

static std::unordered_map<const BYTE*, FeatureSet> g_featureSets;    // under g_featureSetsLock
static SRWLOCK g_featureSetsLock = SRWLOCK_INIT;

typedef void* (*PFN_ConstructFeatureSet)(void* set, void* arg2, void* arg3);
typedef uint64_t (*PFN_AddRuleFeatures)(void* set, const BYTE* selector, void* arg3);
typedef void (*PFN_NodeCall)(BYTE* node);

static PFN_ConstructFeatureSet g_constructFeatureSet = nullptr;
static PFN_AddRuleFeatures g_addRuleFeatures = nullptr;
static PFN_NodeCall g_markNode = nullptr;
static void* g_customNameGetter = nullptr;
static const void* const* g_classAttributeAtom = nullptr;

static std::atomic<uint32_t> g_narrowed{0}, g_marked{0}, g_full{0};

static bool IsCustomType(BYTE type)
{
    return type == 0x97 || type == 0x99;
}

static const BYTE* CompoundAt(const BYTE* selector, uint32_t index)
{
    if (!index) return selector;
    return *(const BYTE* const*)(selector + selector::kCompounds) + (index - 1) * 0x40;
}

static const BYTE* SimpleAt(const BYTE* compound, uint32_t index)
{
    if (!index) return compound;
    return *(const BYTE* const*)(compound + selector::kSimples) + (index - 1) * 0x30;
}

static bool LooksAtPosition(const BYTE* compound)
{
    uint32_t simples = *(const uint32_t*)(compound + selector::kSimpleCount) + 1;
    for (uint32_t i = 0; i < simples; ++i) {
        const BYTE* simple = SimpleAt(compound, i);
        if (simple[0] == kPseudoClass && simple[selector::kValue] >= kFirstChild && simple[selector::kValue] <= kNthChild) return true;
    }
    return false;
}

static bool IsKeptApart(const char* name)
{
    return _stricmp(name, "class") == 0 || _stricmp(name, "id") == 0 || _stricmp(name, "style") == 0;
}

static void AddUnique(std::vector<const void*>& atoms, const void* atom)
{
    if (std::find(atoms.begin(), atoms.end(), atom) == atoms.end()) atoms.push_back(atom);
}

// Called under the exclusive lock.
static void NoteCompound(FeatureSet& set, const BYTE* compound)
{
    Compound needs;
    bool tellsApart = false;
    uint32_t simples = *(const uint32_t*)(compound + selector::kSimpleCount) + 1;
    for (uint32_t i = 0; i < simples; ++i) {
        const BYTE* simple = SimpleAt(compound, i);
        const void* atom = *(const void* const*)(simple + selector::kValue);
        switch (simple[0]) {
        case kTag: {
            needs.hasType = true;
            needs.type = simple[selector::kValue];
            // Without a name the type alone still has to match, which every custom element of it does.
            const char* name = IsCustomType(needs.type) ? *(const char* const*)(simple + selector::kCustomName) : nullptr;
            for (; name && *name; ++name) needs.customName.push_back((char)tolower((unsigned char)*name));
            tellsApart = true;
            break;
        }
        case kId:
            needs.id = atom;
            tellsApart = true;
            break;
        case kClass:
            AddUnique(needs.classes, atom);
            tellsApart = true;
            break;
        case kAttribute:
            // The matcher reads the class attribute from the class list, not the attribute entries, and the id
            // and style may live outside them too, so those tell nothing. Atoms are the names' characters.
            if (!atom || atom == *g_classAttributeAtom || IsKeptApart((const char*)atom)) continue;
            AddUnique(needs.attributes, atom);
            tellsApart = true;
            break;
        }
    }
    if (!tellsApart) {
        set.anyChild = true;
        return;
    }
    std::sort(needs.classes.begin(), needs.classes.end());
    std::sort(needs.attributes.begin(), needs.attributes.end());
    if (std::find(set.compounds.begin(), set.compounds.end(), needs) == set.compounds.end()) set.compounds.push_back(std::move(needs));
}

static void NoteSelector(const BYTE* setAddress, const BYTE* rule)
{
    AcquireSRWLockExclusive(&g_featureSetsLock);
    auto found = g_featureSets.find(setAddress);
    if (found != g_featureSets.end() && !found->second.anyChild) {
        FeatureSet& set = found->second;
        uint32_t compounds = *(const uint32_t*)(rule + selector::kCompoundCount) + 1;
        const BYTE* combinators = *(const BYTE* const*)(rule + selector::kCombinators);
        uint32_t combinatorCount = *(const uint32_t*)(rule + selector::kCombinatorCount);
        for (uint32_t i = 0; i < compounds && !set.anyChild; ++i) {
            const BYTE* compound = CompoundAt(rule, i);
            bool seesSiblings = i < combinatorCount && (combinators[i] == kAdjacentSibling || combinators[i] == kGeneralSibling);
            if (seesSiblings || LooksAtPosition(compound)) NoteCompound(set, compound);
        }
    }
    ReleaseSRWLockExclusive(&g_featureSetsLock);
}

static void* Hook_ConstructFeatureSet(void* set, void* arg2, void* arg3)
{
    void* result = g_constructFeatureSet(set, arg2, arg3);

    AcquireSRWLockExclusive(&g_featureSetsLock);
    // Sets that were freed are never told apart from live ones, so a full table starts over and the live
    // sets it forgets take the engine's own invalidation until their pages load again.
    if (g_featureSets.size() >= kMaxFeatureSets && !g_featureSets.count((const BYTE*)set)) {
        g_featureSets.clear();
        Log("[children] %zu style scopes were seen, the table starts over and the open pages take the engine's own restyle", kMaxFeatureSets);
    }
    g_featureSets[(const BYTE*)set] = FeatureSet();
    ReleaseSRWLockExclusive(&g_featureSetsLock);
    return result;
}

static uint64_t Hook_AddRuleFeatures(void* set, const BYTE* rule, void* arg3)
{
    NoteSelector((const BYTE*)set, rule);
    return g_addRuleFeatures(set, rule, arg3);
}

// The lookup 0x37BC60 makes before it invalidates, the feature set of the element's style scope. Null where
// Cohtml takes the path without it.
static const BYTE* FeatureSetOf(const BYTE* element)
{
    const BYTE* document = *(const BYTE* const*)(element + node::kDocument);
    const BYTE* styles = *(const BYTE* const*)(document + 0x248);
    if (!styles[0x1A0]) return nullptr;

    uint64_t scope = *(const uint64_t*)(element + node::kStyleScope);
    const BYTE* buckets = *(const BYTE* const*)(document + 0x288);
    BYTE shift = document[0x298] & 63;
    const BYTE* entry = buckets + ((scope * 0x9E3779B97F4A7C15ull) >> shift) * 0x18;
    for (int8_t distance = 0; (int8_t)entry[0] >= distance; ++distance, entry += 0x18) {
        if (*(const uint64_t*)(entry + 8) == scope) return *(const BYTE* const*)(entry + 0x10);
    }
    int64_t last = *(const int64_t*)(document + 0x290) + (int8_t)document[0x299];
    return *(const BYTE* const*)(buckets + last * 0x18 + 0x10);
}

static bool NameIs(const BYTE* element, const std::string& name)
{
    // Another name getter keeps the name somewhere else, so such an element could be anything.
    if ((*(void* const* const*)element)[kSlotNameGetter] != g_customNameGetter) return true;
    const char* kept = (element[node::kCustomName] & 1) ? (const char*)(element + node::kCustomName + 1)
                                                        : *(const char* const*)(element + node::kCustomName + 0x10);
    if (!kept) return false;
    size_t i = 0;
    for (; kept[i] && i < name.size(); ++i) {
        if (tolower((unsigned char)kept[i]) != (unsigned char)name[i]) return false;
    }
    return !kept[i] && i == name.size();
}

static bool HasAtom(const void* const* atoms, uint32_t count, const void* atom)
{
    for (uint32_t i = 0; i < count; ++i) if (atoms[i] == atom) return true;
    return false;
}

static bool HasAttribute(const BYTE* element, const void* atom)
{
    const BYTE* attributes = *(const BYTE* const*)(element + node::kAttributes);
    if (!attributes) return false;
    const BYTE* entries = *(const BYTE* const*)(attributes + 0x18);
    uint32_t count = *(const uint32_t*)(attributes + 0x20);
    for (uint32_t i = 0; i < count; ++i) if (*(const void* const*)(entries + i * 0x20) == atom) return true;
    return false;
}

static bool CouldMatch(const Compound& needs, const BYTE* element)
{
    if (needs.hasType) {
        if (element[node::kType] != needs.type) return false;
        if (!needs.customName.empty() && !NameIs(element, needs.customName)) return false;
    }
    if (needs.id && *(const void* const*)(element + node::kId) != needs.id) return false;
    const void* const* classes = *(const void* const* const*)(element + node::kClasses);
    uint32_t classCount = *(const uint32_t*)(element + node::kClassCount);
    for (const void* atom : needs.classes) if (!HasAtom(classes, classCount, atom)) return false;
    for (const void* atom : needs.attributes) if (!HasAttribute(element, atom)) return false;
    return true;
}

// True when the removal was handled here. Runs on the thread that changes the page, which is the only one
// that touches its nodes. The children are marked under the shared lock, Cohtml's mark never files rules.
static bool MarkChildrenThatLookAtPosition(const BYTE* parent)
{
    uint32_t kind = *(const uint32_t*)(parent + node::kKind);
    if (!(kind & 1) || (kind & 0x600)) return false;
    const BYTE* setAddress = FeatureSetOf(parent);
    if (!setAddress) return false;

    AcquireSRWLockShared(&g_featureSetsLock);
    auto found = g_featureSets.find(setAddress);
    bool narrowed = found != g_featureSets.end() && !found->second.anyChild;
    uint32_t marked = 0;
    if (narrowed) {
        const FeatureSet& set = found->second;
        BYTE* const* children = *(BYTE* const* const*)(parent + node::kChildren);
        uint32_t childCount = *(const uint32_t*)(parent + node::kChildCount);
        for (uint32_t i = 0; i < childCount && !set.compounds.empty(); ++i) {
            BYTE* child = children[i];
            if (!(*(const uint32_t*)(child + node::kTree) & 1) || !(*(const uint32_t*)(child + node::kKind) & 1)) continue;
            bool looks = false;
            for (const Compound& needs : set.compounds) {
                if (CouldMatch(needs, child)) {
                    looks = true;
                    break;
                }
            }
            if (!looks) continue;
            g_markNode(child);
            marked++;
        }
    }
    ReleaseSRWLockShared(&g_featureSetsLock);

    if (!narrowed) return false;
    g_narrowed.fetch_add(1, std::memory_order_relaxed);
    g_marked.fetch_add(marked, std::memory_order_relaxed);
    return true;
}

static void Hook_ChildRemoved(BYTE* parent)
{
    if (MarkChildrenThatLookAtPosition(parent)) return;
    g_full.fetch_add(1, std::memory_order_relaxed);
    auto invalidateChildList = (PFN_NodeCall)(*(void** const*)parent)[kSlotChildListInvalidation];
    invalidateChildList(parent);
}

static bool CallsTarget(const BYTE* base, uint32_t rva, uint32_t target)
{
    if (base[rva] != 0xE8) return false;
    int32_t rel;
    memcpy(&rel, base + rva + 1, 4);
    return (int64_t)rva + 5 + rel == (int64_t)target;
}

void InstallChildRemovalFix()
{
    BYTE* cohtml = (BYTE*)GetModuleHandleW(L"cohtml.WindowsDesktop.dll");
    if (!cohtml) {
        Log("[children] cohtml.WindowsDesktop.dll is not loaded, nothing patched");
        return;
    }
    auto nt = (IMAGE_NT_HEADERS64*)(cohtml + ((IMAGE_DOS_HEADER*)cohtml)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kCohtmlTimeDateStamp || nt->OptionalHeader.SizeOfImage != kCohtmlSizeOfImage) {
        Log("[children] this is not the Cohtml build the child removal fix was written for (stamp %08X, image %08X), nothing patched",
            (unsigned)nt->FileHeader.TimeDateStamp, (unsigned)nt->OptionalHeader.SizeOfImage);
        return;
    }
    for (const Region& region : kRegions) {
        if (Fnv1a64(cohtml + region.rva, region.length) != region.fnv1a64) {
            Log("[children] %s at rva 0x%06X is not the code this was written against, nothing patched", region.what, (unsigned)region.rva);
            return;
        }
    }
    for (uint32_t site : kConstructorCalls) {
        if (!CallsTarget(cohtml, site, kRvaFeatureSetConstructor)) {
            Log("[children] the feature set construction at rva 0x%06X is not the call it was read as, nothing patched", (unsigned)site);
            return;
        }
    }
    if (!CallsTarget(cohtml, kRvaAddRuleFeaturesCall, kRvaAddRuleFeatures)) {
        Log("[children] the feature add at rva 0x%06X is not the call it was read as, nothing patched", (unsigned)kRvaAddRuleFeaturesCall);
        return;
    }

    const size_t page = 0x1000;
    BYTE* cave = AllocNear(cohtml + kRvaRemovalInvalidation, page);
    if (!cave) {
        Log("[children] no free memory within reach of Cohtml, nothing patched");
        return;
    }

    // jmp qword ptr [rip], followed by the absolute target
    BYTE* next = cave;
    auto emitJump = [&next](void* target) {
        BYTE* start = next;
        const BYTE opcode[6] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
        memcpy(next, opcode, sizeof opcode);
        memcpy(next + sizeof opcode, &target, 8);
        next += sizeof opcode + 8;
        return start;
    };
    BYTE* constructJump = emitJump((void*)&Hook_ConstructFeatureSet);
    BYTE* addJump = emitJump((void*)&Hook_AddRuleFeatures);
    BYTE* removedJump = emitJump((void*)&Hook_ChildRemoved);

    DWORD old = 0;
    if (!VirtualProtect(cave, page, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[children] could not make the jumps executable, nothing patched");
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), cave, page);

    // Every call is encoded before any is written, so a jump out of reach patches nothing.
    struct Patch {
        uint32_t rva;
        BYTE code[6];
        size_t length;
    };
    const size_t constructorCount = sizeof kConstructorCalls / sizeof kConstructorCalls[0];
    Patch patches[8] = {};
    size_t patchCount = 0;
    bool reachable = true;
    for (uint32_t site : kConstructorCalls) {
        Patch& patch = patches[patchCount++];
        patch.rva = site;
        patch.length = 5;
        reachable &= EncodeRel32(0xE8, cohtml + site, constructJump, patch.code);
    }
    Patch& add = patches[patchCount++];
    add.rva = kRvaAddRuleFeaturesCall;
    add.length = 5;
    reachable &= EncodeRel32(0xE8, cohtml + kRvaAddRuleFeaturesCall, addJump, add.code);
    // The removal's call through the vtable is six bytes, the call to the hook five and a nop.
    Patch& removed = patches[patchCount++];
    removed.rva = kRvaRemovalInvalidation;
    removed.length = 6;
    reachable &= EncodeRel32(0xE8, cohtml + kRvaRemovalInvalidation, removedJump, removed.code);
    removed.code[5] = 0x90;
    if (!reachable) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[children] the hooks are out of reach of Cohtml, nothing patched");
        return;
    }

    g_constructFeatureSet = (PFN_ConstructFeatureSet)(cohtml + kRvaFeatureSetConstructor);
    g_addRuleFeatures = (PFN_AddRuleFeatures)(cohtml + kRvaAddRuleFeatures);
    g_markNode = (PFN_NodeCall)(cohtml + kRvaMarkNode);
    g_customNameGetter = cohtml + kRvaCustomNameGetter;
    g_classAttributeAtom = (const void* const*)(cohtml + kRvaClassAttributeAtom);

    // The feature sets are followed from their construction before any removal looks them up, and the
    // process is still single threaded, so no set is built half seen.
    size_t written = 0;
    for (size_t i = 0; i < patchCount; ++i) {
        if (!WriteCode(cohtml + patches[i].rva, patches[i].code, patches[i].length)) {
            Log("[children] could not patch Cohtml at rva 0x%06X", (unsigned)patches[i].rva);
            if (i <= constructorCount) {
                Log("[children] the style scopes cannot all be followed, the removal is left to Cohtml");
                return;
            }
            continue;
        }
        written++;
    }
    Log("[children] child removal fix on at %zu of %zu places, removing a child restyles only the children whose rules look at their position",
        written, patchCount);
}

ChildRemovalCounts ChildRemovalFixTakeCounts()
{
    ChildRemovalCounts counts;
    counts.narrowed = g_narrowed.exchange(0, std::memory_order_relaxed);
    counts.marked = g_marked.exchange(0, std::memory_order_relaxed);
    counts.full = g_full.exchange(0, std::memory_order_relaxed);
    return counts;
}
