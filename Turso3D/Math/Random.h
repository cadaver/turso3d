// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Math.h"

namespace Turso3D
{

/// Set the random seed. The default seed is 1.
TURSO3D_API void SetRandomSeed(unsigned seed);
/// Return the current random seed.
TURSO3D_API unsigned RandomSeed();
/// Return a random number between 0-32767. Should operate similarly to MSVC rand().
TURSO3D_API int Rand();
/// Return a standard normal distributed number.
TURSO3D_API float RandStandardNormal();

/// Return a random float between 0.0 (inclusive) and 1.0 (exclusive.)
inline float Random() { return Rand() / 32768.0f; }
/// Return a random float between 0.0 and range, inclusive from both ends.
inline float Random(float range) { return Rand() * range / 32767.0f; }
/// Return a random float between min and max, inclusive from both ends.
inline float Random(float min, float max) { return Rand() * (max - min) / 32767.0f + min; }
/// Return a random integer between 0 and range - 1.
inline int Random(int range) { return (Rand() * (range - 1) + 16384) / 32767; }
/// Return a random integer between min and max - 1.
inline int Random(int min, int max) { return (Rand() * (max - min - 1) + 16384) / 32767 + min; }
/// Return a random normal distributed number with the given mean value and variance.
inline float RandomNormal(float meanValue, float variance) { return RandStandardNormal() * sqrtf(variance) + meanValue; }

}
