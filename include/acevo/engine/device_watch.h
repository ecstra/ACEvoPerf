#pragma once
#include "acevo/common.h"

// Logs what Windows tells the process about devices: interface arrivals and removals
// (HID, USB, audio, monitor) and default audio endpoint changes, each with the time
// since attach. The game recreates its DirectInput devices and restarts its audio on
// such events, which costs a frame of several hundred milliseconds (BUG-013).
void StartDeviceWatch();
