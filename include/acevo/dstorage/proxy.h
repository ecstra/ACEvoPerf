#pragma once
#include "acevo/common.h"

// The four exports the game imports from dstorage.dll live in src/dstorage/proxy.cpp
// (declared through exports.def). Everything else the rest of the mod needs:
void InitDStorageProxy();

// The real IDStorageFactory behind the proxy, null until the game has asked for it.
IDStorageFactory* RealDStorageFactory();
