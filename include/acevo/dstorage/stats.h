#pragma once
#include "acevo/common.h"

// Process wide DirectStorage counters, written by the queue proxies and read by the
// timeline thread and the hitch logger. Indexed by DSTORAGE_REQUEST_DESTINATION_TYPE.
extern std::atomic<uint64_t> g_reqByDest[5];
extern std::atomic<uint64_t> g_bytesByDest[5];
extern std::atomic<uint64_t> g_submitsTotal;
extern std::atomic<uint64_t> g_tileBatches;
extern std::atomic<uint64_t> g_tileBatchMax;

const char* DestName(UINT64 destinationType);
