// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "IntRect.h"

#include <vector>

/// Rectangular area allocator.
class AreaAllocator
{
public:
    /// Default construct with empty size.
    AreaAllocator();
    /// Construct with given width and height.
    AreaAllocator(int width, int height, bool fastMode = true);
    /// Construct with given width and height, and set the maximum allowed to grow to.
    AreaAllocator(int width, int height, int maxWidth, int maxHeight, bool fastMode = true);

    /// Reset to given width and height and remove all previous allocations.
    void Reset(int width, int height, int maxWidth = 0, int maxHeight = 0, bool fastMode = true);
    /// Try to allocate a rectangle. Return true on success, with x & y coordinates filled.
    bool Allocate(int width, int height, int& x, int& y);
    /// Attempt a specific allocation. Return true on success.
    bool AllocateSpecific(const IntRect& reserved);

    /// Return the current size.
    const IntVector2& Size() const { return size; }
    /// Return the current width.
    int Width() const { return size.x; }
    /// Return the current height.
    int Height() const { return size.y; }
    /// Return the maximum size.
    const IntVector2& MaxSize() const { return maxSize; }
    /// Return the maximum width.
    int MaxWidth() const { return maxSize.x; }
    /// Return the maximum height.
    int MaxHeight() const { return maxSize.y; }
    /// Return whether uses fast mode. Fast mode uses a simpler allocation scheme which may waste free space, but is OK for eg. fonts.
    bool IsFastMode() const { return fastMode; }

private:
    /// Remove space from a free rectangle. Return true if the original rectangle should be erased from the free list. Not called in fast mode.
    bool SplitRect(IntRect original, const IntRect& reserve);
    /// Clean up redundant free space. Not called in fast mode.
    void Cleanup();

    /// Free rectangles.
    std::vector<IntRect> freeAreas;
    /// Current size.
    IntVector2 size;
    /// Maximum size allowed to grow to. It is zero when it is not allowed to grow.
    IntVector2 maxSize;
    /// The dimension used for next growth. Used internally.
    bool doubleWidth;
    /// Fast mode flag.
    bool fastMode;
};
