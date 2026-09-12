// See include/acevo/dstorage/merge_survey.h.
#include "acevo/dstorage/merge_survey.h"
#include "acevo/core/log.h"

void MergeSurvey::CloseRun()
{
    if (!runLen) return;

    ++runs;
    savings += runLen - 1;
    if (runLen > longestRun) longestRun = runLen;

    int bucket = runLen == 1 ? 0
               : runLen == 2 ? 1
               : runLen <= 4 ? 2
               : runLen <= 8 ? 3
               : runLen <= 16 ? 4
                              : 5;
    ++runLength[bucket];
    runLen = 0;
    resource = nullptr;
}

void MergeSurvey::OnBarrier()
{
    // A submit, a status entry or a fence signal all pin the order the game expects, so a run
    // cannot reach across one even when the requests either side would have fitted together.
    if (runLen > 1) ++brokeBarrier;
    CloseRun();
}

void MergeSurvey::OnRequest(const DSTORAGE_REQUEST* request)
{
    if (!request) return;

    if (request->Options.DestinationType != DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION) {
        ++otherDest;
        CloseRun();     // anything else in the stream ends the run the same way a barrier does
        return;
    }

    ++texRequests;

    const DSTORAGE_DESTINATION_TEXTURE_REGION& dest = request->Destination.Texture;
    const D3D12_BOX& box = dest.Region;
    if (box.left == 0 && box.top == 0 && box.front == 0) ++boxAtOrigin;

    const bool memorySource = request->Options.SourceType == DSTORAGE_REQUEST_SOURCE_MEMORY;

    // Does this request continue the run, meaning the same resource, the very next subresource,
    // and source bytes that carry straight on from the previous request?
    bool continues = false;
    if (runLen && resource == dest.Resource && fromMemory == memorySource) {
        if (dest.SubresourceIndex != subresource + 1) {
            ++brokeSubresource;
        } else if (memorySource
                       ? (request->Source.Memory.Source == sourceEnd)
                       : (request->Source.File.Source == file && request->Source.File.Offset == fileEnd)) {
            continues = true;
        } else {
            ++brokeSource;
        }
    } else if (runLen) {
        ++brokeResource;
    }

    if (!continues) CloseRun();

    resource = dest.Resource;
    subresource = dest.SubresourceIndex;
    fromMemory = memorySource;
    if (memorySource) {
        sourceEnd = (const uint8_t*)request->Source.Memory.Source + request->Source.Memory.Size;
    } else {
        file = request->Source.File.Source;
        fileEnd = request->Source.File.Offset + request->Source.File.Size;
    }
    ++runLen;
}

void MergeSurvey::Report(const char* queueName)
{
    if (reported) return;
    reported = true;

    CloseRun();
    if (!texRequests) return;

    const double pct = 100.0 * (double)savings / (double)texRequests;
    Log("[merge] queue '%s': %llu texture requests in %llu runs, %llu of them (%.1f%%) could have "
        "ridden along with the request before them. Longest run %u, box at the origin %llu, other "
        "destinations %llu.",
        queueName, (unsigned long long)texRequests, (unsigned long long)runs,
        (unsigned long long)savings, pct, longestRun,
        (unsigned long long)boxAtOrigin, (unsigned long long)otherDest);
    Log("[merge] queue '%s': run lengths 1:%llu 2:%llu 3-4:%llu 5-8:%llu 9-16:%llu 17+:%llu",
        queueName, (unsigned long long)runLength[0], (unsigned long long)runLength[1],
        (unsigned long long)runLength[2], (unsigned long long)runLength[3],
        (unsigned long long)runLength[4], (unsigned long long)runLength[5]);
    Log("[merge] queue '%s': runs ended by a different resource %llu, a non consecutive subresource "
        "%llu, a gap in the source %llu, a submit or status or signal %llu",
        queueName, (unsigned long long)brokeResource, (unsigned long long)brokeSubresource,
        (unsigned long long)brokeSource, (unsigned long long)brokeBarrier);
}
