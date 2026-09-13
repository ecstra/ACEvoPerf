#pragma once
#include "acevo/common.h"

// acevo_perf_streaming.csv, one row per streaming event, written only while [developer]
// streaming_trace=1. Rows are buffered in memory and written once a second by the timeline
// thread, so the engine hooks that produce them never touch the disk. The columns after the kind
// are generic and the kind says what they hold, see .agent/docs/ops/telemetry.md.
bool TraceOn();
void TraceRow(const char* kind, const char* fmt, ...);
void TraceFlush();
void TraceFinalFlush();
