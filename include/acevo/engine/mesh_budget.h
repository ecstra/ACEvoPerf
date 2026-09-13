#pragma once
#include "acevo/common.h"

// The mesh streamer's budget while force_canonical_pool_sizes fixes the pool sizes, patched in
// memory. Called once from DllMain while the process is still single threaded. Does nothing unless
// [developer] mesh_budget_mb is set, and patches nothing unless every byte it depends on matches the
// build it was written against.
void InstallMeshBudget();
