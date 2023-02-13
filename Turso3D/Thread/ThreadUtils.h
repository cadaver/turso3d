// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

// Check if is running in the main thread.
bool IsMainThread();
// Return hardware CPU count, for determining e.g. amount of worker threads.
unsigned CPUCount();
