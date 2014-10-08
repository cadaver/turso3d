// For conditions of distribution and use, see copyright notice in License.txt

#include "AreaAllocator.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

AreaAllocator::AreaAllocator() :
    size(IntVector2::ZERO),
    maxSize(IntVector2::ZERO),
    doubleWidth(true)
{
    Reset(0, 0);
}

AreaAllocator::AreaAllocator(int width, int height) :
    size(width, height),
    maxSize(IntVector2::ZERO),
    doubleWidth(true)
{
    Reset(width, height);
}

AreaAllocator::AreaAllocator(int width, int height, int maxWidth, int maxHeight) :
    size(width, height),
    maxSize(maxWidth, maxHeight),
    doubleWidth(true)
{
    Reset(width, height);
}

void AreaAllocator::Reset(int width, int height)
{
    freeAreas.Clear();
    
    IntRect initialArea(0, 0, width, height);
    freeAreas.Push(initialArea);
}

bool AreaAllocator::Allocate(int width, int height, int& x, int& y)
{
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;
    
    Vector<IntRect>::Iterator best;
    int bestFreeArea;
    
    for(;;)
    {
        best = freeAreas.End();
        bestFreeArea = M_MAX_INT;

        for (Vector<IntRect>::Iterator it = freeAreas.Begin(); it != freeAreas.End(); ++it)
        {
            int freeWidth = it->Width();
            int freeHeight = it->Height();
            
            if (freeWidth >= width && freeHeight >= height)
            {
                // Calculate rank for free area. Lower is better
                int freeArea = freeWidth * freeHeight;
                
                if (freeArea < bestFreeArea)
                {
                    best = it;
                    bestFreeArea = freeArea;
                }
            }
        }
        
        if (best == freeAreas.End())
        {
            if (doubleWidth && size.x < maxSize.x)
            {
                int oldWidth = size.x;
                size.x <<= 1;
                // If no allocations yet, simply expand the single free area
                IntRect& first = freeAreas.Front();
                if (freeAreas.Size() == 1 && first.left == 0 && first.top == 0 && first.right == oldWidth && first.bottom == size.y)
                    first.right = size.x;
                else
                {
                    IntRect newArea(oldWidth, 0, size.x, size.y);
                    freeAreas.Push(newArea);
                }
            }
            else if (!doubleWidth && size.y < maxSize.y)
            {
                int oldHeight = size.y;
                size.y <<= 1;
                // If no allocations yet, simply expand the single free area
                IntRect& first = freeAreas.Front();
                if (freeAreas.Size() == 1 && first.left == 0 && first.top == 0 && first.right == size.x && first.bottom == oldHeight)
                    first.bottom = size.y;
                else
                {
                    IntRect newArea(0, oldHeight, size.x, size.y);
                    freeAreas.Push(newArea);
                }
            }
            else
                return false;
            
            doubleWidth = !doubleWidth;
        }
        else
            break;
    }
    
    IntRect reserved(best->left, best->top, best->left + width, best->top + height);
    x = best->left;
    y = best->top;
    
    // Reserve the area by splitting up the remaining free area
    best->left = reserved.right;
    if (best->Height() > 2 * height)
    {
        IntRect splitArea(reserved.left, reserved.bottom, best->right, best->bottom);
        best->bottom = reserved.bottom;
        freeAreas.Push(splitArea);
    }

    return true;
}

}
