#pragma once
#include "acevo/common.h"

// The four exports the game imports from dstorage.dll live in src/dstorage/proxy.cpp
// (declared through exports.def). Everything else the rest of the mod needs:
void InitDStorageProxy();

// The real IDStorageFactory behind the proxy, null until the game has asked for it, and null again
// if the game ever drops its last reference on the proxy.
//
// The caller owns a reference and has to release it, or hold it deliberately. Reading the pointer
// under the lock is not enough on its own, because the caller uses it after the lock is gone and
// the last release can land in that window. The overlay is the one caller and holds its reference
// for the process on purpose, which is also what keeps the layer working through that release.
IDStorageFactory* RealDStorageFactory();
