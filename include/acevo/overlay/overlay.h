#pragma once
#include "acevo/common.h"

// Loose files under <game>\<overlay folder>\<package path> shadow entries of content.kspkg.
namespace overlay {
    // Collect the loose files and hook the game's file I/O. Call from DllMain.
    void Install();

    // Whether the layer has anything to serve, which is what OverlayRedirect tests first. Settled
    // by Install at attach, long before the game creates a queue, so a caller deciding whether the
    // redirect will ever be needed can ask. The proxy does, because the redirect only runs from
    // inside QueueProxy and without this the queue would go unwrapped whenever the statistics are
    // off, taking the whole layer with it.
    bool Active();
}

// Rewrite a DirectStorage request that targets a virtual offset onto the loose file.
// Returns false when the request is not affected, and also when it targets a replaced entry whose
// file cannot be opened or no factory is there to open it with. The caller then enqueues it as it
// came, at its virtual offset, and DirectStorage fails it without writing its buffer.
bool OverlayRedirect(const DSTORAGE_REQUEST* request, DSTORAGE_REQUEST* redirected);
