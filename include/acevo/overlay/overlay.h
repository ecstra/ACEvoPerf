#pragma once
#include "acevo/common.h"

// Loose files under <game>\<overlay folder>\<package path> shadow entries of content.kspkg.
namespace overlay {
    // Collect the loose files and hook the game's file I/O. Call from DllMain.
    void Install();
}

// Rewrite a DirectStorage request that targets a virtual offset onto the loose file.
// Returns false when the request is not affected.
bool OverlayRedirect(const DSTORAGE_REQUEST* request, DSTORAGE_REQUEST* redirected);
