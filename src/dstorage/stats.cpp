#include "acevo/dstorage/stats.h"

std::atomic<uint64_t> g_reqByDest[5];
std::atomic<uint64_t> g_bytesByDest[5];
std::atomic<uint64_t> g_submitsTotal{0};
std::atomic<uint64_t> g_tileBatches{0};
std::atomic<uint64_t> g_tileBatchMax{0};

const char* DestName(UINT64 d)
{
    switch (d) {
    case DSTORAGE_REQUEST_DESTINATION_MEMORY: return "MEMORY";
    case DSTORAGE_REQUEST_DESTINATION_BUFFER: return "BUFFER";
    case DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION: return "TEXTURE_REGION";
    case DSTORAGE_REQUEST_DESTINATION_MULTIPLE_SUBRESOURCES: return "MULTI_SUBRES";
    case DSTORAGE_REQUEST_DESTINATION_TILES: return "TILES";
    default: return "?";
    }
}
