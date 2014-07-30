// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Rect.h"
#include "../Base/Vector.h"

namespace Turso3D
{

/// Rectangular area allocator.
class TURSO3D_API AreaAllocator
{
public:
    /// Default construct with empty size.
    AreaAllocator();
    /// Construct with given width and height.
    AreaAllocator(int width, int height);
    /// Construct with given width and height, and set the maximum allowed to grow to.
    AreaAllocator(int width, int height, int maxWidth, int maxHeight);
    
    /// Reset to given width and height and remove all previous allocations.
    void Reset(int width, int height);
    /// Try to allocate an area. Return true if successful, with x & y coordinates filled.
    bool Allocate(int width, int height, int& x, int& y);
    /// Return the current width.
    int GetWidth() const { return size.x; }
    /// Return the current height.
    int GetHeight() const { return size.y; }

private:
    /// Free rectangles.
    PODVector<IntRect> freeAreas;
    /// Current size.
    IntVector2 size;
    /// Maximum size allowed to grow to. It is zero when it is not allowed to grow.
    IntVector2 maxSize;
    /// The dimension used for next growth. Used internally.
    bool doubleWidth;
};

}
