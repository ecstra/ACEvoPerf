#pragma once
#include "acevo/common.h"

// Can the proxy collapse runs of single subresource texture requests into one multi subresource
// request? The game sends tens of thousands of DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION
// requests per session, each naming one resource, one subresource index and a box. DirectStorage
// can take a whole range of subresources in a single request instead, but only when the source
// holds their data back to back in subresource order.
//
// This counts how often consecutive requests on a queue actually line up that way, so the question
// is answered from the game's own behaviour before any merging is written. Off unless
// [profile] merge_survey=1, and it only ever counts, it never changes a request.
struct MergeSurvey {
    // A run is a maximal sequence of requests that could have travelled as one request.
    uint64_t texRequests = 0;      // TEXTURE_REGION requests seen
    uint64_t runs = 0;             // runs found, a lone request is a run of one
    uint64_t savings = 0;          // requests that continued a run, the count merging would remove

    // Why a run ended, which says what is worth fixing if the answer is disappointing.
    uint64_t brokeResource = 0;    // a different resource
    uint64_t brokeSubresource = 0; // subresource index not one past the previous
    uint64_t brokeSource = 0;      // source bytes not contiguous with the previous request
    uint64_t brokeBarrier = 0;     // a submit, a status or a fence signal came between

    uint64_t boxAtOrigin = 0;      // region starts at 0,0,0, a hint that it is a whole subresource
    uint64_t otherDest = 0;        // requests with some other destination type, for context
    uint32_t longestRun = 0;
    uint64_t runLength[6] = {};    // 1, 2, 3 to 4, 5 to 8, 9 to 16, 17 and over

    void OnRequest(const DSTORAGE_REQUEST* request);
    void OnBarrier();              // submit, status or signal, none of which a merged run may cross
    void Report(const char* queueName);

private:
    // The tail of the run in progress.
    const void* resource = nullptr;
    UINT subresource = 0;
    const void* sourceEnd = nullptr;   // memory source: one past the last byte
    IDStorageFile* file = nullptr;     // file source: the file and the offset one past the end
    UINT64 fileEnd = 0;
    bool fromMemory = false;
    uint32_t runLen = 0;
    bool reported = false;   // Close and the destructor both ask for the final report

    void CloseRun();
};
