#include "../IO/Log.h"
#include "../Math/IntRect.h"
#include "Camera.h"
#include "OcclusionBuffer.h"

#include <cstring>
#include <tracy/Tracy.hpp>

static const unsigned CLIPMASK_X_POS = 0x1;
static const unsigned CLIPMASK_X_NEG = 0x2;
static const unsigned CLIPMASK_Y_POS = 0x4;
static const unsigned CLIPMASK_Y_NEG = 0x8;
static const unsigned CLIPMASK_Z_POS = 0x10;
static const unsigned CLIPMASK_Z_NEG = 0x20;

OcclusionBuffer::OcclusionBuffer() :
    buffer(nullptr),
    width(0),
    height(0),
    numTriangles(0),
    maxTriangles(OCCLUSION_DEFAULT_MAX_TRIANGLES),
    depthHierarchyDirty(true)
{
}

OcclusionBuffer::~OcclusionBuffer()
{
}

bool OcclusionBuffer::SetSize(int newWidth, int newHeight)
{
    // Force the height to an even amount of pixels for better mip generation
    if (height & 1)
        ++height;
    
    if (newWidth == width && newHeight == height)
        return true;
    
    ZoneScoped;

    if (newWidth <= 0 || newHeight <= 0)
        return false;
    
    if (!IsPowerOfTwo(width))
    {
        LOGERROR("Occlusion buffer width is not a power of two");
        return false;
    }
    
    width = newWidth;
    height = newHeight;
    
    // Reserve extra memory in case 3D clipping is not exact
    fullBuffer = new int[width * (height + 2) + 2];
    buffer = fullBuffer.Get() + width + 1;
    mipBuffers.clear();
    
    // Build buffers for mip levels
    for (;;)
    {
        newWidth = (newWidth + 1) / 2;
        newHeight = (newHeight + 1) / 2;
        
        mipBuffers.push_back(SharedArrayPtr<DepthValue>(new DepthValue[newWidth * newHeight]));
        
        if (newWidth <= OCCLUSION_MIN_SIZE && newHeight <= OCCLUSION_MIN_SIZE)
            break;
    }
    
    LOGDEBUGF("Set occlusion buffer size %dx%d with %d mip levels", width, height, (int)mipBuffers.size());
    
    CalculateViewport();
    return true;
}

void OcclusionBuffer::SetView(Camera* camera)
{
    if (!camera)
        return;
    
    view = camera->ViewMatrix();
    projection = camera->ProjectionMatrix(false);
    viewProj = projection * view;
    CalculateViewport();
}

void OcclusionBuffer::SetMaxTriangles(unsigned triangles)
{
    maxTriangles = triangles;
}

void OcclusionBuffer::Reset()
{
    numTriangles = 0;
}

void OcclusionBuffer::Clear()
{
    ZoneScoped;

    numTriangles = 0;
    if (!buffer)
        return;
    
    int* dest = buffer;
    size_t count = width * height;
    
    while (count--)
        *dest++ = OCCLUSION_Z_SCALE;
    
    depthHierarchyDirty = true;
}

bool OcclusionBuffer::Draw(const Matrix3x4& model, const void* vertexData, size_t vertexSize, size_t vertexStart, size_t vertexCount)
{
    ZoneScoped;

    const unsigned char* srcData = ((const unsigned char*)vertexData) + vertexStart * vertexSize;
    
    Matrix4 modelViewProj = viewProj * model;
    depthHierarchyDirty = true;
    
    // Theoretical max. amount of vertices if each of the 6 clipping planes doubles the triangle count
    Vector4 vertices[64 * 3];
    
    // 16-bit indices
    unsigned index = 0;
    while (index + 2 < vertexCount)
    {
        if (numTriangles >= maxTriangles)
            return false;
        
        const Vector3& v0 = *((const Vector3*)(&srcData[index * vertexSize]));
        const Vector3& v1 = *((const Vector3*)(&srcData[(index + 1) * vertexSize]));
        const Vector3& v2 = *((const Vector3*)(&srcData[(index + 2) * vertexSize]));
        
        vertices[0] = ModelTransform(modelViewProj, v0);
        vertices[1] = ModelTransform(modelViewProj, v1);
        vertices[2] = ModelTransform(modelViewProj, v2);
        DrawTriangle(vertices);
        
        index += 3;
    }
    
    return true;
}

bool OcclusionBuffer::Draw(const Matrix3x4& model, const void* vertexData, size_t vertexSize, const void* indexData, size_t indexSize, size_t indexStart, size_t indexCount)
{
    ZoneScoped;

    const unsigned char* srcData = (const unsigned char*)vertexData;
    
    Matrix4 modelViewProj = viewProj * model;
    depthHierarchyDirty = true;
    
    // Theoretical max. amount of vertices if each of the 6 clipping planes doubles the triangle count
    Vector4 vertices[64 * 3];
    
    // 16-bit indices
    if (indexSize == sizeof(unsigned short))
    {
        const unsigned short* indices = ((const unsigned short*)indexData) + indexStart;
        const unsigned short* indicesEnd = indices + indexCount;
        
        while (indices < indicesEnd)
        {
            if (numTriangles >= maxTriangles)
                return false;
            
            const Vector3& v0 = *((const Vector3*)(&srcData[indices[0] * vertexSize]));
            const Vector3& v1 = *((const Vector3*)(&srcData[indices[1] * vertexSize]));
            const Vector3& v2 = *((const Vector3*)(&srcData[indices[2] * vertexSize]));
            
            vertices[0] = ModelTransform(modelViewProj, v0);
            vertices[1] = ModelTransform(modelViewProj, v1);
            vertices[2] = ModelTransform(modelViewProj, v2);
            DrawTriangle(vertices);
            
            indices += 3;
        }
    }
    else
    {
        const unsigned* indices = ((const unsigned*)indexData) + indexStart;
        const unsigned* indicesEnd = indices + indexCount;
        
        while (indices < indicesEnd)
        {
            if (numTriangles >= maxTriangles)
                return false;
            
            const Vector3& v0 = *((const Vector3*)(&srcData[indices[0] * vertexSize]));
            const Vector3& v1 = *((const Vector3*)(&srcData[indices[1] * vertexSize]));
            const Vector3& v2 = *((const Vector3*)(&srcData[indices[2] * vertexSize]));
            
            vertices[0] = ModelTransform(modelViewProj, v0);
            vertices[1] = ModelTransform(modelViewProj, v1);
            vertices[2] = ModelTransform(modelViewProj, v2);
            DrawTriangle(vertices);
            
            indices += 3;
        }
    }
    
    return true;
}

void OcclusionBuffer::BuildDepthHierarchy()
{
    ZoneScoped;

    if (!buffer || numTriangles)
        return;
    
    // Build the first mip level from the pixel-level data
    int mipWidth = (width + 1) / 2;
    int mipHeight = (height + 1) / 2;
    if (mipBuffers.size())
    {
        for (int y = 0; y < mipHeight; ++y)
        {
            int* src = buffer + (y * 2) * width;
            DepthValue* dest = mipBuffers[0].Get() + y * mipWidth;
            DepthValue* end = dest + mipWidth;
            
            if (y * 2 + 1 < height)
            {
                int* src2 = src + width;
                while (dest < end)
                {
                    int minUpper = Min(src[0], src[1]);
                    int minLower = Min(src2[0], src2[1]);
                    dest->min = Min(minUpper, minLower);
                    int maxUpper = Max(src[0], src[1]);
                    int maxLower = Max(src2[0], src2[1]);
                    dest->max = Max(maxUpper, maxLower);
                    
                    src += 2;
                    src2 += 2;
                    ++dest;
                }
            }
            else
            {
                while (dest < end)
                {
                    dest->min = Min(src[0], src[1]);
                    dest->max = Max(src[0], src[1]);
                    
                    src += 2;
                    ++dest;
                }
            }
        }
    }
    
    // Build the rest of the mip levels
    for (unsigned i = 1; i < mipBuffers.size(); ++i)
    {
        int prevWidth = mipWidth;
        int prevHeight = mipHeight;
        mipWidth = (mipWidth + 1) / 2;
        mipHeight = (mipHeight + 1) / 2;
        
        for (int y = 0; y < mipHeight; ++y)
        {
            DepthValue* src = mipBuffers[i - 1].Get() + (y * 2) * prevWidth;
            DepthValue* dest = mipBuffers[i].Get() + y * mipWidth;
            DepthValue* end = dest + mipWidth;
            
            if (y * 2 + 1 < prevHeight)
            {
                DepthValue* src2 = src + prevWidth;
                while (dest < end)
                {
                    int minUpper = Min(src[0].min, src[1].min);
                    int minLower = Min(src2[0].min, src2[1].min);
                    dest->min = Min(minUpper, minLower);
                    int maxUpper = Max(src[0].max, src[1].max);
                    int maxLower = Max(src2[0].max, src2[1].max);
                    dest->max = Max(maxUpper, maxLower);
                    
                    src += 2;
                    src2 += 2;
                    ++dest;
                }
            }
            else
            {
                while (dest < end)
                {
                    dest->min = Min(src[0].min, src[1].min);
                    dest->max = Max(src[0].max, src[1].max);
                    
                    src += 2;
                    ++dest;
                }
            }
        }
    }
    
    depthHierarchyDirty = false;
}

bool OcclusionBuffer::IsVisible(const BoundingBox& worldSpaceBox) const
{
    if (!buffer || !numTriangles)
        return true;
    
    // Transform corners to projection space
    Vector4 vertices[8];
    vertices[0] = ModelTransform(viewProj, worldSpaceBox.min);
    vertices[1] = ModelTransform(viewProj, Vector3(worldSpaceBox.max.x, worldSpaceBox.min.y, worldSpaceBox.min.z));
    vertices[2] = ModelTransform(viewProj, Vector3(worldSpaceBox.min.x, worldSpaceBox.max.y, worldSpaceBox.min.z));
    vertices[3] = ModelTransform(viewProj, Vector3(worldSpaceBox.max.x, worldSpaceBox.max.y, worldSpaceBox.min.z));
    vertices[4] = ModelTransform(viewProj, Vector3(worldSpaceBox.min.x, worldSpaceBox.min.y, worldSpaceBox.max.z));
    vertices[5] = ModelTransform(viewProj, Vector3(worldSpaceBox.max.x, worldSpaceBox.min.y, worldSpaceBox.max.z));
    vertices[6] = ModelTransform(viewProj, Vector3(worldSpaceBox.min.x, worldSpaceBox.max.y, worldSpaceBox.max.z));
    vertices[7] = ModelTransform(viewProj, worldSpaceBox.max);
    
    // Transform to screen space. If any of the corners cross the near plane, assume visible
    float minX, maxX, minY, maxY, minZ;
    
    if (vertices[0].z <= 0.0f)
        return true;
    
    Vector3 projected = ViewportTransform(vertices[0]);
    minX = maxX = projected.x;
    minY = maxY = projected.y;
    minZ = projected.z;
    
    // Project the rest
    for (size_t i = 1; i < 8; ++i)
    {
        if (vertices[i].z <= 0.0f)
            return true;
        
        projected = ViewportTransform(vertices[i]);
        
        if (projected.x < minX) minX = projected.x;
        if (projected.x > maxX) maxX = projected.x;
        if (projected.y < minY) minY = projected.y;
        if (projected.y > maxY) maxY = projected.y;
        if (projected.z < minZ) minZ = projected.z;
    }
    
    // Expand the bounding box 1 pixel in each direction to be conservative and correct rasterization offset
    IntRect rect(
        (int)(minX - 1.5f), (int)(minY - 1.5f),
        (int)(maxX + 0.5f), (int)(maxY + 0.5f)
    );
    
    // If the rect is outside, let frustum culling handle
    if (rect.right < 0 || rect.bottom < 0)
        return true;
    if (rect.left >= width || rect.top >= height)
        return true;
    
    // Clipping of rect
    if (rect.left < 0)
        rect.left = 0;
    if (rect.top < 0)
        rect.top = 0;
    if (rect.right >= width)
        rect.right = width - 1;
    if (rect.bottom >= height)
        rect.bottom = height - 1;
    
    // Convert depth to integer
    int z = (int)(minZ + 0.5f);
    
    if (!depthHierarchyDirty)
    {
        // Start from lowest mip level and check if a conclusive result can be found
        for (int i = (int)mipBuffers.size() - 1; i >= 0; --i)
        {
            int shift = i + 1;
            int mipWidth = width >> shift;
            int left = rect.left >> shift;
            int right = rect.right >> shift;
            
            DepthValue* mipBuffer = mipBuffers[i].Get();
            DepthValue* row = mipBuffer + (rect.top >> shift) * mipWidth;
            DepthValue* endRow = mipBuffer + (rect.bottom >> shift) * mipWidth;
            bool allOccluded = true;
            
            while (row <= endRow)
            {
                DepthValue* src = row + left;
                DepthValue* end = row + right;
                while (src <= end)
                {
                    if (z <= src->min)
                        return true;
                    if (z <= src->max)
                        allOccluded = false;
                    ++src;
                }
                row += mipWidth;
            }
            
            if (allOccluded)
                return false;
        }
    }
    
    // If no conclusive result, finally check the pixel-level data
    int* row = buffer + rect.top * width;
    int* endRow = buffer + rect.bottom * width;
    while (row <= endRow)
    {
        int* src = row + rect.left;
        int* end = row + rect.right;
        while (src <= end)
        {
            if (z <= *src)
                return true;
            ++src;
        }
        row += width;
    }
    
    return false;
}

void OcclusionBuffer::CalculateViewport()
{
    // Add half pixel offset due to 3D frustum culling
    scaleX = 0.5f * width;
    scaleY = -0.5f * height;
    offsetX = 0.5f * width + 0.5f;
    offsetY = 0.5f * height + 0.5f;
}

void OcclusionBuffer::DrawTriangle(Vector4* vertices)
{
    unsigned clipMask = 0;
    unsigned andClipMask = 0;
    bool drawOk = false;
    Vector3 projected[3];
    
    // Build the clip plane mask for the triangle
    for (unsigned i = 0; i < 3; ++i)
    {
        unsigned vertexClipMask = 0;
        
        if (vertices[i].x > vertices[i].w)
            vertexClipMask |= CLIPMASK_X_POS;
        if (vertices[i].x < -vertices[i].w)
            vertexClipMask |= CLIPMASK_X_NEG;
        if (vertices[i].y > vertices[i].w)
            vertexClipMask |= CLIPMASK_Y_POS;
        if (vertices[i].y < -vertices[i].w)
            vertexClipMask |= CLIPMASK_Y_NEG;
        if (vertices[i].z > vertices[i].w)
            vertexClipMask |= CLIPMASK_Z_POS;
        if (vertices[i].z < 0.0f)
            vertexClipMask |= CLIPMASK_Z_NEG;
        
        clipMask |= vertexClipMask;
        
        if (!i)
            andClipMask = vertexClipMask;
        else
            andClipMask &= vertexClipMask;
    }
    
    // If triangle is fully behind any clip plane, can reject quickly
    if (andClipMask)
        return;
    
    // Check if triangle is fully inside
    if (!clipMask)
    {
        projected[0] = ViewportTransform(vertices[0]);
        projected[1] = ViewportTransform(vertices[1]);
        projected[2] = ViewportTransform(vertices[2]);
        
        if (CheckFacing(projected[0], projected[1], projected[2]))
        {
            DrawTriangle2D(projected);
            drawOk = true;
        }
    }
    else
    {
        bool clipTriangles[64];
        
        // Initial triangle
        clipTriangles[0] = true;
        unsigned numClipTriangles = 1;
        
        if (clipMask & CLIPMASK_X_POS)
            ClipVertices(Vector4(-1.0f, 0.0f, 0.0f, 1.0f), vertices, clipTriangles, numClipTriangles);
        if (clipMask & CLIPMASK_X_NEG)
            ClipVertices(Vector4(1.0f, 0.0f, 0.0f, 1.0f), vertices, clipTriangles, numClipTriangles);
        if (clipMask & CLIPMASK_Y_POS)
            ClipVertices(Vector4(0.0f, -1.0f, 0.0f, 1.0f), vertices, clipTriangles, numClipTriangles);
        if (clipMask & CLIPMASK_Y_NEG)
            ClipVertices(Vector4(0.0f, 1.0f, 0.0f, 1.0f), vertices, clipTriangles, numClipTriangles);
        if (clipMask & CLIPMASK_Z_POS)
            ClipVertices(Vector4(0.0f, 0.0f, -1.0f, 1.0f), vertices, clipTriangles, numClipTriangles);
        if (clipMask & CLIPMASK_Z_NEG)
            ClipVertices(Vector4(0.0f, 0.0f, 1.0f, 0.0f), vertices, clipTriangles, numClipTriangles);
        
        // Draw each accepted triangle
        for (unsigned i = 0; i < numClipTriangles; ++i)
        {
            if (clipTriangles[i])
            {
                unsigned index = i * 3;
                projected[0] = ViewportTransform(vertices[index]);
                projected[1] = ViewportTransform(vertices[index + 1]);
                projected[2] = ViewportTransform(vertices[index + 2]);
                
                if (CheckFacing(projected[0], projected[1], projected[2]))
                {
                    DrawTriangle2D(projected);
                    drawOk = true;
                }
            }
        }
    }
    
    if (drawOk)
        ++numTriangles;
}

void OcclusionBuffer::ClipVertices(const Vector4& plane, Vector4* vertices, bool* clipTriangles, unsigned& numClipTriangles)
{
    unsigned num = numClipTriangles;
    
    for (unsigned i = 0; i < num; ++i)
    {
        if (clipTriangles[i])
        {
            unsigned index = i * 3;
            float d0 = plane.DotProduct(vertices[index]);
            float d1 = plane.DotProduct(vertices[index + 1]);
            float d2 = plane.DotProduct(vertices[index + 2]);
            
            // If all vertices behind the plane, reject triangle
            if (d0 < 0.0f && d1 < 0.0f && d2 < 0.0f)
            {
                clipTriangles[i] = false;
                continue;
            }
            // If 2 vertices behind the plane, create a new triangle in-place
            else if (d0 < 0.0f && d1 < 0.0f)
            {
                vertices[index] = ClipEdge(vertices[index], vertices[index + 2], d0, d2);
                vertices[index + 1] = ClipEdge(vertices[index + 1], vertices[index + 2], d1, d2);
            }
            else if (d0 < 0.0f && d2 < 0.0f)
            {
                vertices[index] = ClipEdge(vertices[index], vertices[index + 1], d0, d1);
                vertices[index + 2] = ClipEdge(vertices[index + 2], vertices[index + 1], d2, d1);
            }
            else if (d1 < 0.0f && d2 < 0.0f)
            {
                vertices[index + 1] = ClipEdge(vertices[index + 1], vertices[index], d1, d0);
                vertices[index + 2] = ClipEdge(vertices[index + 2], vertices[index], d2, d0);
            }
            // 1 vertex behind the plane: create one new triangle, and modify one in-place
            else if (d0 < 0.0f)
            {
                unsigned newIdx = numClipTriangles * 3;
                clipTriangles[numClipTriangles] = true;
                ++numClipTriangles;
                
                vertices[newIdx] = ClipEdge(vertices[index], vertices[index + 2], d0, d2);
                vertices[newIdx + 1] = vertices[index] = ClipEdge(vertices[index], vertices[index + 1], d0, d1);
                vertices[newIdx + 2] = vertices[index + 2];
            }
            else if (d1 < 0.0f)
            {
                unsigned newIdx = numClipTriangles * 3;
                clipTriangles[numClipTriangles] = true;
                ++numClipTriangles;
                
                vertices[newIdx + 1] = ClipEdge(vertices[index + 1], vertices[index], d1, d0);
                vertices[newIdx + 2] = vertices[index + 1] = ClipEdge(vertices[index + 1], vertices[index + 2], d1, d2);
                vertices[newIdx] = vertices[index];
            }
            else if (d2 < 0.0f)
            {
                unsigned newIdx = numClipTriangles * 3;
                clipTriangles[numClipTriangles] = true;
                ++numClipTriangles;
                
                vertices[newIdx + 2] = ClipEdge(vertices[index + 2], vertices[index + 1], d2, d1);
                vertices[newIdx] = vertices[index + 2] = ClipEdge(vertices[index + 2], vertices[index], d2, d0);
                vertices[newIdx + 1] = vertices[index + 1];
            }
        }
    }
}

// Code based on Chris Hecker's Perspective Texture Mapping series in the Game Developer magazine
// Also available online at http://chrishecker.com/Miscellaneous_Technical_Articles

/// %Gradients of a software rasterized triangle.
struct Gradients
{
    /// Construct from vertices.
    Gradients(const Vector3* vertices)
    {
        float invdX = 1.0f / (((vertices[1].x - vertices[2].x) *
            (vertices[0].y - vertices[2].y)) -
            ((vertices[0].x - vertices[2].x) *
            (vertices[1].y - vertices[2].y)));
        
        float invdY = -invdX;
        
        dInvZdX = invdX * (((vertices[1].z - vertices[2].z) * (vertices[0].y - vertices[2].y)) -
            ((vertices[0].z - vertices[2].z) * (vertices[1].y - vertices[2].y)));
        
        dInvZdY = invdY * (((vertices[1].z - vertices[2].z) * (vertices[0].x - vertices[2].x)) -
            ((vertices[0].z - vertices[2].z) * (vertices[1].x - vertices[2].x)));
        
        dInvZdXInt = (int)dInvZdX;
    }
    
    /// Integer horizontal gradient.
    int dInvZdXInt;
    /// Horizontal gradient.
    float dInvZdX;
    /// Vertical gradient.
    float dInvZdY;
};

/// %Edge of a software rasterized triangle.
struct Edge
{
    /// Construct from gradients and top & bottom vertices.
    Edge(const Gradients& gradients, const Vector3& top, const Vector3& bottom, int topY)
    {
        float slope = (bottom.x - top.x) / (bottom.y - top.y);
        float yPreStep = (float)(topY + 1) - top.y;
        float xPreStep = slope * yPreStep;
        
        x = (int)((xPreStep + top.x) * OCCLUSION_X_SCALE + 0.5f);
        xStep = (int)(slope * OCCLUSION_X_SCALE + 0.5f);
        invZ = (int)(top.z + xPreStep * gradients.dInvZdX + yPreStep * gradients.dInvZdY + 0.5f);
        invZStep = (int)(slope * gradients.dInvZdX + gradients.dInvZdY + 0.5f);
    }
    
    /// X coordinate.
    int x;
    /// X coordinate step.
    int xStep;
    /// Inverse Z.
    int invZ;
    /// Inverse Z step.
    int invZStep;
};

void OcclusionBuffer::DrawTriangle2D(const Vector3* vertices)
{
    int top, middle, bottom;
    bool middleIsRight;
    
    // Sort vertices in Y-direction
    if (vertices[0].y < vertices[1].y)
    {
        if (vertices[2].y < vertices[0].y)
        {
            top = 2; middle = 0; bottom = 1;
            middleIsRight = true;
        }
        else
        {
            top = 0;
            if (vertices[1].y < vertices[2].y)
            {
                middle = 1; bottom = 2;
                middleIsRight = true;
            }
            else
            {
                middle = 2; bottom = 1;
                middleIsRight = false;
            }
        }
    }
    else
    {
        if (vertices[2].y < vertices[1].y)
        {
            top = 2; middle = 1; bottom = 0;
            middleIsRight = false;
        }
        else
        {
            top = 1;
            if (vertices[0].y < vertices[2].y)
            {
                middle = 0; bottom = 2;
                middleIsRight = false;
            }
            else
            {
                middle = 2; bottom = 0;
                middleIsRight = true;
            }
        }
    }
    
    int topY = (int)vertices[top].y;
    int middleY = (int)vertices[middle].y;
    int bottomY = (int)vertices[bottom].y;
    
    // Check for degenerate triangle
    if (topY == bottomY)
        return;
    
    Gradients gradients(vertices);
    Edge topToMiddle(gradients, vertices[top], vertices[middle], topY);
    Edge topToBottom(gradients, vertices[top], vertices[bottom], topY);
    Edge middleToBottom(gradients, vertices[middle], vertices[bottom], middleY);
    
    // The triangle is clockwise, so if bottom > middle then middle is right
    if (middleIsRight)
    {
        // Top half
        int* row = buffer + topY * width;
        int* endRow = buffer + middleY * width;
        while (row < endRow)
        {
            int invZ = topToBottom.invZ;
            int* dest = row + (topToBottom.x >> 16);
            int* end = row + (topToMiddle.x >> 16);
            while (dest < end)
            {
                if (invZ < *dest)
                    *dest = invZ;
                invZ += gradients.dInvZdXInt;
                ++dest;
            }
            
            topToBottom.x += topToBottom.xStep;
            topToBottom.invZ += topToBottom.invZStep;
            topToMiddle.x += topToMiddle.xStep;
            row += width;
        }
        
        // Bottom half
        row = buffer + middleY * width;
        endRow = buffer + bottomY * width;
        while (row < endRow)
        {
            int invZ = topToBottom.invZ;
            int* dest = row + (topToBottom.x >> 16);
            int* end = row + (middleToBottom.x >> 16);
            while (dest < end)
            {
                if (invZ < *dest)
                    *dest = invZ;
                invZ += gradients.dInvZdXInt;
                ++dest;
            }
            
            topToBottom.x += topToBottom.xStep;
            topToBottom.invZ += topToBottom.invZStep;
            middleToBottom.x += middleToBottom.xStep;
            row += width;
        }
    }
    else
    {
        // Top half
        int* row = buffer + topY * width;
        int* endRow = buffer + middleY * width;
        while (row < endRow)
        {
            int invZ = topToMiddle.invZ;
            int* dest = row + (topToMiddle.x >> 16);
            int* end = row + (topToBottom.x >> 16);
            while (dest < end)
            {
                if (invZ < *dest)
                    *dest = invZ;
                invZ += gradients.dInvZdXInt;
                ++dest;
            }
            
            topToMiddle.x += topToMiddle.xStep;
            topToMiddle.invZ += topToMiddle.invZStep;
            topToBottom.x += topToBottom.xStep;
            row += width;
        }
        
        // Bottom half
        row = buffer + middleY * width;
        endRow = buffer + bottomY * width;
        while (row < endRow)
        {
            int invZ = middleToBottom.invZ;
            int* dest = row + (middleToBottom.x >> 16);
            int* end = row + (topToBottom.x >> 16);
            while (dest < end)
            {
                if (invZ < *dest)
                    *dest = invZ;
                invZ += gradients.dInvZdXInt;
                ++dest;
            }
            
            middleToBottom.x += middleToBottom.xStep;
            middleToBottom.invZ += middleToBottom.invZStep;
            topToBottom.x += topToBottom.xStep;
            row += width;
        }
    }
}
