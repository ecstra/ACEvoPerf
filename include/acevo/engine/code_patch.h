#pragma once
#include "acevo/common.h"
#include <initializer_list>

// What the in-memory patches of AssettoCorsaEVO.exe share. Each compares every region it relies on
// with the build below before anything is written, see src/engine/streamer.cpp for the full rules.

// AssettoCorsaEVO.exe 0.9.1, the Steam build of 2026-09-11.
constexpr DWORD kPatchedBuildTimeDateStamp = 0x6A9EC72A;
constexpr DWORD kPatchedBuildSizeOfImage = 0x06CDD000;

struct CodeRegion {
    uint32_t rva;
    uint32_t length;
    uint64_t fnv1a64;
    const char* what;
};

// One rel32 displacement to rewrite: the instruction at rva, its displacement at dispOffset, and
// where it has to point today.
struct CodeSite {
    const char* what;
    uint32_t rva;
    uint8_t dispOffset;
    uint8_t length;
    uint32_t target;
};

uint64_t Fnv1a64(const BYTE* p, size_t n);

// A 32 bit displacement reaches 2 GB either way, so the stubs have to live near the exe.
BYTE* AllocNear(BYTE* anchor, size_t size);

struct Emitter {
    BYTE* at;

    void Bytes(std::initializer_list<BYTE> bytes)
    {
        for (BYTE b : bytes) *at++ = b;
    }

    void Displacement(const BYTE* target)
    {
        int32_t disp = (int32_t)(target - (at + 4));
        memcpy(at, &disp, 4);
        at += 4;
    }

    // inc qword ptr [rip + disp32]
    void IncrementQword(const BYTE* counter)
    {
        Bytes({ 0x48, 0xFF, 0x05 });
        Displacement(counter);
    }

    // mov eax, dword ptr [rip + disp32]
    void LoadEax(const BYTE* slot)
    {
        Bytes({ 0x8B, 0x05 });
        Displacement(slot);
    }

    // jmp qword ptr [rip], followed by the absolute target
    void JumpTo(void* target)
    {
        Bytes({ 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 });
        memcpy(at, &target, 8);
        at += 8;
    }
};
