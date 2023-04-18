#include "../IO/Log.h"
#include "../Math/IntRect.h"
#include "../Thread/WorkQueue.h"
#include "Camera.h"
#include "OcclusionBuffer.h"

#include <cstring>
#include <tracy/Tracy.hpp>

// Rasterizer code based on Chris Hecker's Perspective Texture Mapping series in the Game Developer magazine
// Also available online at http://chrishecker.com/Miscellaneous_Technical_Articles

static const unsigned CLIPMASK_X_POS = 0x1;
static const unsigned CLIPMASK_X_NEG = 0x2;
static const unsigned CLIPMASK_Y_POS = 0x4;
static const unsigned CLIPMASK_Y_NEG = 0x8;
static const unsigned CLIPMASK_Z_POS = 0x10;
static const unsigned CLIPMASK_Z_NEG = 0x20;

static inline Vector4 ModelTransform(const Matrix4& transform, const Vector3& vertex)
{
    return Vector4(
        transform.m00 * vertex.x + transform.m01 * vertex.y + transform.m02 * vertex.z + transform.m03,
        transform.m10 * vertex.x + transform.m11 * vertex.y + transform.m12 * vertex.z + transform.m13,
        transform.m20 * vertex.x + transform.m21 * vertex.y + transform.m22 * vertex.z + transform.m23,
        transform.m30 * vertex.x + transform.m31 * vertex.y + transform.m32 * vertex.z + transform.m33
    );
}

static inline Vector4 ClipEdge(const Vector4& v0, const Vector4& v1, float d0, float d1)
{
    float t = d0 / (d0 - d1);
    return v0 + t * (v1 - v0);
}

static inline bool CheckFacing(const Vector3& v0, const Vector3& v1, const Vector3& v2)
{
    float aX = v0.x - v1.x;
    float aY = v0.y - v1.y;
    float bX = v2.x - v1.x;
    float bY = v2.y - v1.y;
    return (aX * bY - aY * bX) <= 0.0f;
}

/// Depth gradients of a software rasterized triangle.
struct Gradients
{
    /// Calculate from 3 vertices.
    void Calculate(const Vector3* vertices)
    {
        float invdX = 1.0f / (((vertices[1].x - vertices[2].x) * (vertices[0].y - vertices[2].y)) - ((vertices[0].x - vertices[2].x) * (vertices[1].y - vertices[2].y)));
        float invdY = -invdX;

        dInvZdX = invdX * (((vertices[1].z - vertices[2].z) * (vertices[0].y - vertices[2].y)) - ((vertices[0].z - vertices[2].z) * (vertices[1].y - vertices[2].y)));
        dInvZdY = invdY * (((vertices[1].z - vertices[2].z) * (vertices[0].x - vertices[2].x)) - ((vertices[0].z - vertices[2].z) * (vertices[1].x - vertices[2].x)));
    }

    /// Horizontal gradient.
    float dInvZdX;
    /// Vertical gradient.
    float dInvZdY;
};

/// %Edge of a software rasterized triangle.
struct Edge
{
    /// Calculate from gradients and top & bottom vertices.
    void Calculate(const Gradients& gradients, const Vector3& top, const Vector3& bottom)
    {
        topY = (int)top.y;
        bottomY = (int)bottom.y;
        float slope = (bottom.x - top.x) / (bottom.y - top.y);
        float yPreStep = (float)(topY + 1) - top.y;
        float xPreStep = slope * yPreStep;

        x = (int)((xPreStep + top.x) * OCCLUSION_X_SCALE + 0.5f);
        xStep = (int)(slope * OCCLUSION_X_SCALE + 0.5f);
        invZ = top.z + xPreStep * gradients.dInvZdX + yPreStep * gradients.dInvZdY;
        invZStep = slope * gradients.dInvZdX + gradients.dInvZdY;
    }

    /// X coordinate at the top.
    int x;
    /// Y top coordinate.
    int topY;
    /// Y bottom coordinate.
    int bottomY;
    /// X coordinate step.
    int xStep;
    /// Inverse Z.
    float invZ;
    /// Inverse Z step.
    float invZStep;
};

/// Stored triangle with all edges calculated for rasterization.
struct GradientTriangle
{
    /// Calculate from vertices.
    void Calculate(Vector3* vertices)
    {
        int top, middle, bottom;
        Gradients gradients;

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

        gradients.Calculate(vertices);
        topToMiddle.Calculate(gradients, vertices[top], vertices[middle]);
        topToBottom.Calculate(gradients, vertices[top], vertices[bottom]);
        middleToBottom.Calculate(gradients, vertices[middle], vertices[bottom]);
        dInvZdX = gradients.dInvZdX;
    }

    /// Top to middle edge.
    Edge topToMiddle;
    /// Middle to bottom edge.
    Edge middleToBottom;
    /// Top to bottom edge.
    Edge topToBottom;
    /// Horizontal gradient applied per pixel.
    float dInvZdX;
    /// Whether middle vertex is on the right.
    bool middleIsRight;
};

/// %Task for generating clipped triangles out of an occluder draw batch.
struct GenerateTrianglesTask : public MemberFunctionTask<OcclusionBuffer>
{
    /// Construct.
    GenerateTrianglesTask(OcclusionBuffer* object_, MemberWorkFunctionPtr function_) :
        MemberFunctionTask<OcclusionBuffer>(object_, function_)
    {
    }

    /// Draw batch.
    TriangleDrawBatch batch;
    /// Generated triangles.
    std::vector<GradientTriangle> triangles;
    /// Triangle indices per depth buffer slice.
    std::vector<unsigned> triangleIndices[OCCLUSION_BUFFER_SLICES];
};

/// %Task for clearing a slice of the depth buffer and then rasterizing triangles assigned to it.
struct RasterizeTrianglesTask : public MemberFunctionTask<OcclusionBuffer>
{
    /// Construct.
    RasterizeTrianglesTask(OcclusionBuffer* object_, MemberWorkFunctionPtr function_) :
        MemberFunctionTask<OcclusionBuffer>(object_, function_)
    {
    }

    /// Vertical slice index.
    size_t sliceIdx;
    /// Start Y-position.
    int startY;
    /// End Y-position.
    int endY;
};

OcclusionBuffer::OcclusionBuffer() :
    buffer(nullptr),
    width(0),
    height(0),
    numTriangleBatches(0),
    numReadyMipBuffers(0)
{
    workQueue = Subsystem<WorkQueue>();

    depthHierarchyTask = new MemberFunctionTask<OcclusionBuffer>(this, &OcclusionBuffer::BuildDepthHierarchyWork);

    // If not threaded, do not divide rendering into slices
    size_t numSlices = workQueue->NumThreads() > 1 ? OCCLUSION_BUFFER_SLICES : 1;
    for (size_t i = 0; i < numSlices; ++i)
    {
        rasterizeTrianglesTasks[i] = new RasterizeTrianglesTask(this, &OcclusionBuffer::RasterizeTrianglesWork);
        rasterizeTrianglesTasks[i]->sliceIdx = i;
    }

    numPendingRasterizeTasks.store(0);
}

OcclusionBuffer::~OcclusionBuffer()
{
}

bool OcclusionBuffer::SetSize(int newWidth, int newHeight)
{
    ZoneScoped;

    // Force the height to a multiple of 16 to make sure each worker thread slice is same size, and can build its part of the depth hierarchy
    newWidth += 8;
    newWidth &= 0x7ffffff0;
    
    if (newWidth == width && newHeight == height)
        return true;
    
    if (newWidth <= 0 || newHeight <= 0)
        return false;
    
    if (!IsPowerOfTwo(width))
    {
        LOGERROR("Occlusion buffer width is not a power of two");
        return false;
    }
    
    width = newWidth;
    height = newHeight;

    // Define slices for worker threads if actually threaded
    if (workQueue->NumThreads() > 1)
    {
        sliceHeight = newHeight / OCCLUSION_BUFFER_SLICES;
        activeSlices = OCCLUSION_BUFFER_SLICES;

        for (int i = 0; i < OCCLUSION_BUFFER_SLICES; ++i)
        {
            rasterizeTrianglesTasks[i]->startY = i * sliceHeight;
            rasterizeTrianglesTasks[i]->endY = (i + 1) * sliceHeight;
        }
    }
    else
    {
        sliceHeight = height;
        activeSlices = 1;
        rasterizeTrianglesTasks[0]->startY = 0;
        rasterizeTrianglesTasks[0]->endY = height;
    }

    // Allocate extra for safety in case of off-by-one 3D clipping
    fullBuffer = new float[width * (height + 4)];
    buffer = fullBuffer.Get() + 2 * width;
    mipBuffers.clear();
    
    // Build buffers for mip levels
    for (;;)
    {
        newWidth = (newWidth + 1) / 2;
        newHeight = (newHeight + 1) / 2;
        
        mipBuffers.push_back(AutoArrayPtr<DepthValue>(new DepthValue[newWidth * newHeight]));
        
        if (newWidth <= OCCLUSION_MIN_SIZE && newHeight <= OCCLUSION_MIN_SIZE)
            break;
    }
    
    LOGDEBUGF("Set occlusion buffer size %dx%d with %d mip levels", width, height, (int)mipBuffers.size());
    
    // Add half pixel offset due to 3D frustum culling
    scaleX = 0.5f * width;
    scaleY = -0.5f * height;
    offsetX = 0.5f * width + 0.5f;
    offsetY = 0.5f * height + 0.5f;

    return true;
}

void OcclusionBuffer::SetView(Camera* camera)
{
    if (camera)
        viewProj = camera->ProjectionMatrix(false) * camera->ViewMatrix();
}

void OcclusionBuffer::Reset()
{
    // Make sure to complete previous work before resetting to avoid out of sync state
    Complete();

    numTriangleBatches = 0;
    numReadyMipBuffers = 0;
    numPendingGenerateTasks.store(0);
    numPendingRasterizeTasks.store(0);
}

void OcclusionBuffer::AddTriangles(const Matrix3x4& worldTransform, const void* vertexData, size_t vertexSize, size_t vertexStart, size_t vertexCount)
{
    if (generateTrianglesTasks.size() <= numTriangleBatches)
        generateTrianglesTasks.push_back(new GenerateTrianglesTask(this, &OcclusionBuffer::GenerateTrianglesWork));

    GenerateTrianglesTask* task = generateTrianglesTasks[numTriangleBatches];
    TriangleDrawBatch& batch = task->batch;

    batch.worldTransform = worldTransform;
    batch.vertexData = ((const unsigned char*)vertexData) + vertexStart * vertexSize;
    batch.vertexSize = vertexSize;
    batch.indexData = nullptr;
    batch.drawCount = vertexCount;

    task->triangles.clear();
    for (int i = 0; i < activeSlices; ++i)
        task->triangleIndices[i].clear();

    ++numTriangleBatches;
}

void OcclusionBuffer::AddTriangles(const Matrix3x4& worldTransform, const void* vertexData, size_t vertexSize, const void* indexData, size_t indexSize, size_t indexStart, size_t indexCount)
{
    if (generateTrianglesTasks.size() <= numTriangleBatches)
        generateTrianglesTasks.push_back(new GenerateTrianglesTask(this, &OcclusionBuffer::GenerateTrianglesWork));

    GenerateTrianglesTask* task = generateTrianglesTasks[numTriangleBatches];
    TriangleDrawBatch& batch = task->batch;

    batch.worldTransform = worldTransform;
    batch.vertexData = ((const unsigned char*)vertexData);
    batch.vertexSize = vertexSize;
    batch.indexData = ((const unsigned char*)indexData) + indexSize * indexStart;
    batch.indexSize = indexSize;
    batch.drawCount = indexCount;

    task->triangles.clear();
    for (int i = 0; i < activeSlices; ++i)
        task->triangleIndices[i].clear();

    ++numTriangleBatches;
}

void OcclusionBuffer::DrawTriangles()
{
    // Avoid beginning the work twice
    if (!buffer || !IsCompleted())
        return;

    if (numTriangleBatches)
    {
        numPendingGenerateTasks.store((int)numTriangleBatches);
        numPendingRasterizeTasks.store(1); // Have non-zero counter at this point for correct completion check. It will be loaded with slice count once triangles are ready
        workQueue->QueueTasks(numTriangleBatches, reinterpret_cast<Task**>(&generateTrianglesTasks[0]));
    }
}

void OcclusionBuffer::Complete()
{
    while (numPendingRasterizeTasks.load() > 0)
        workQueue->TryComplete();
}

bool OcclusionBuffer::IsCompleted() const
{
    return !numPendingRasterizeTasks.load();
}

bool OcclusionBuffer::IsVisible(const BoundingBox& worldSpaceBox) const
{
    if (!buffer || !numTriangleBatches)
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
    
    // Correct rasterization offset and expand slightly to prevent false negatives
    IntRect rect(
        (int)minX - 1, (int)minY - 1,
        (int)maxX, (int)maxY
    );
    
    // Subtract a minor depth bias to prevent e.g. self-occlusion further away
    minZ -= M_EPSILON;

    // Clipping of rect
    if (rect.left < 0)
        rect.left = 0;
    if (rect.top < 0)
        rect.top = 0;
    if (rect.right >= width)
        rect.right = width - 1;
    if (rect.bottom >= height)
        rect.bottom = height - 1;
    
    // Start from lowest available mip level and check if a conclusive result can be found
    for (int i = (int)numReadyMipBuffers - 1; i >= 0; --i)
    {
        int shift = i + 1;
        int mipWidth = width >> shift;
        int left = rect.left >> shift;
        int right = rect.right >> shift;
        
        DepthValue* mipBuffer = mipBuffers[i];
        DepthValue* row = mipBuffer + (rect.top >> shift) * mipWidth;
        DepthValue* endRow = mipBuffer + (rect.bottom >> shift) * mipWidth;
        bool allOccluded = true;
        
        while (row <= endRow)
        {
            DepthValue* src = row + left;
            DepthValue* end = row + right;
            while (src <= end)
            {
                if (minZ <= src->min)
                    return true;
                if (minZ <= src->max)
                    allOccluded = false;
                ++src;
            }
            row += mipWidth;
        }
        
        if (allOccluded)
            return false;
    }
    
    // If no conclusive result, finally check the pixel-level data
    float* row = buffer + rect.top * width;
    float* endRow = buffer + rect.bottom * width;
    while (row <= endRow)
    {
        float* src = row + rect.left;
        float* end = row + rect.right;
        while (src <= end)
        {
            if (minZ <= *src)
                return true;
            ++src;
        }
        row += width;
    }
    
    return false;
}

void OcclusionBuffer::AddTriangle(GenerateTrianglesTask* task, Vector4* vertices)
{
    unsigned clipMask;
    unsigned andClipMask;
    unsigned vertexClipMask = 0;

    if (vertices[0].x > vertices[0].w) vertexClipMask |= CLIPMASK_X_POS;
    if (vertices[0].x < -vertices[0].w) vertexClipMask |= CLIPMASK_X_NEG;
    if (vertices[0].y > vertices[0].w) vertexClipMask |= CLIPMASK_Y_POS;
    if (vertices[0].y < -vertices[0].w) vertexClipMask |= CLIPMASK_Y_NEG;
    if (vertices[0].z > vertices[0].w) vertexClipMask |= CLIPMASK_Z_POS;
    if (vertices[0].z < 0.0f) vertexClipMask |= CLIPMASK_Z_NEG;

    clipMask = vertexClipMask;
    andClipMask = vertexClipMask;
    vertexClipMask = 0;

    if (vertices[1].x > vertices[1].w) vertexClipMask |= CLIPMASK_X_POS;
    if (vertices[1].x < -vertices[1].w) vertexClipMask |= CLIPMASK_X_NEG;
    if (vertices[1].y > vertices[1].w) vertexClipMask |= CLIPMASK_Y_POS;
    if (vertices[1].y < -vertices[1].w) vertexClipMask |= CLIPMASK_Y_NEG;
    if (vertices[1].z > vertices[1].w) vertexClipMask |= CLIPMASK_Z_POS;
    if (vertices[1].z < 0.0f) vertexClipMask |= CLIPMASK_Z_NEG;

    clipMask |= vertexClipMask;
    andClipMask &= vertexClipMask;
    vertexClipMask = 0;

    if (vertices[2].x > vertices[2].w) vertexClipMask |= CLIPMASK_X_POS;
    if (vertices[2].x < -vertices[2].w) vertexClipMask |= CLIPMASK_X_NEG;
    if (vertices[2].y > vertices[2].w) vertexClipMask |= CLIPMASK_Y_POS;
    if (vertices[2].y < -vertices[2].w) vertexClipMask |= CLIPMASK_Y_NEG;
    if (vertices[2].z > vertices[2].w) vertexClipMask |= CLIPMASK_Z_POS;
    if (vertices[2].z < 0.0f) vertexClipMask |= CLIPMASK_Z_NEG;

    clipMask |= vertexClipMask;
    andClipMask &= vertexClipMask;

    // If triangle is fully behind any clip plane, can reject quickly
    if (andClipMask)
        return;
    
    // Check if triangle is fully inside
    if (!clipMask)
    {
        unsigned idx = (unsigned)task->triangles.size();
        task->triangles.resize(idx + 1);
        GradientTriangle& triangle = task->triangles.back();

        Vector3 viewportVertices[3];
        viewportVertices[0] = ViewportTransform(vertices[0]);
        viewportVertices[1] = ViewportTransform(vertices[1]);
        viewportVertices[2] = ViewportTransform(vertices[2]);

        // Check facing before calculating gradients or storing
        if (!CheckFacing(viewportVertices[0], viewportVertices[1], viewportVertices[2]))
        {
            task->triangles.resize(idx);
            return;
        }

        triangle.Calculate(viewportVertices);

        int minY = triangle.topToBottom.topY;
        int maxY = triangle.topToBottom.bottomY;

        // Add to needed slices
        for (int j = 0; j < activeSlices; ++j)
        {
            int sliceStartY = j * sliceHeight;
            int sliceEndY = sliceStartY + sliceHeight;
            if (minY < sliceEndY && maxY > sliceStartY)
                task->triangleIndices[j].push_back(idx);
        }
    }
    else
    {
        bool clipTriangles[64];

        // Initial triangle
        clipTriangles[0] = true;
        size_t numClipTriangles = 1;
        
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
        
        // Add each accepted triangle
        for (size_t i = 0; i < numClipTriangles; ++i)
        {
            if (clipTriangles[i])
            {
                unsigned idx = (unsigned)task->triangles.size();
                task->triangles.resize(idx + 1);
                GradientTriangle& triangle = task->triangles.back();

                Vector3 viewportVertices[3];
                viewportVertices[0] = ViewportTransform(vertices[i * 3]);
                viewportVertices[1] = ViewportTransform(vertices[i * 3 + 1]);
                viewportVertices[2] = ViewportTransform(vertices[i * 3 + 2]);

                // Check facing before calculating gradients or storing
                if (!CheckFacing(viewportVertices[0], viewportVertices[1], viewportVertices[2]))
                {
                    task->triangles.resize(idx);
                    return;
                }

                triangle.Calculate(viewportVertices);

                int minY = triangle.topToBottom.topY;
                int maxY = triangle.topToBottom.bottomY;

                // Add to needed slices
                for (int j = 0; j < activeSlices; ++j)
                {
                    int sliceStartY = j * sliceHeight;
                    int sliceEndY = sliceStartY + sliceHeight;
                    if (minY < sliceEndY && maxY > sliceStartY)
                        task->triangleIndices[j].push_back(idx);
                }
            }
        }
    }
}

void OcclusionBuffer::ClipVertices(const Vector4& plane, Vector4* vertices, bool* clipTriangles, size_t& numClipTriangles)
{
    size_t trianglesNow = numClipTriangles;
    
    for (size_t i = 0; i < trianglesNow; ++i)
    {
        if (clipTriangles[i])
        {
            size_t index = i * 3;
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
                size_t newIdx = numClipTriangles * 3;
                clipTriangles[numClipTriangles] = true;
                ++numClipTriangles;
                
                vertices[newIdx] = ClipEdge(vertices[index], vertices[index + 2], d0, d2);
                vertices[newIdx + 1] = vertices[index] = ClipEdge(vertices[index], vertices[index + 1], d0, d1);
                vertices[newIdx + 2] = vertices[index + 2];
            }
            else if (d1 < 0.0f)
            {
                size_t newIdx = numClipTriangles * 3;
                clipTriangles[numClipTriangles] = true;
                ++numClipTriangles;
                
                vertices[newIdx + 1] = ClipEdge(vertices[index + 1], vertices[index], d1, d0);
                vertices[newIdx + 2] = vertices[index + 1] = ClipEdge(vertices[index + 1], vertices[index + 2], d1, d2);
                vertices[newIdx] = vertices[index];
            }
            else if (d2 < 0.0f)
            {
                size_t newIdx = numClipTriangles * 3;
                clipTriangles[numClipTriangles] = true;
                ++numClipTriangles;
                
                vertices[newIdx + 2] = ClipEdge(vertices[index + 2], vertices[index + 1], d2, d1);
                vertices[newIdx] = vertices[index + 2] = ClipEdge(vertices[index + 2], vertices[index], d2, d0);
                vertices[newIdx + 1] = vertices[index + 1];
            }
        }
    }
}

/// Perform transform of model vertex from clip to viewport space.
inline Vector3 OcclusionBuffer::ViewportTransform(const Vector4& vertex) const
{
    float invW = 1.0f / vertex.w;
    return Vector3(
        invW * vertex.x * scaleX + offsetX,
        invW * vertex.y * scaleY + offsetY,
        invW * vertex.z
    );
}

inline void OcclusionBuffer::RasterizeSpans(const Edge& leftEdge, const Edge& rightEdge, int topY, int bottomY, float dInvZdX, int sliceStartY, int sliceEndY, int& leftX, float& leftInvZ, int& rightX)
{
    // If past the bottom or degenerate, no operation
    if (topY >= sliceEndY || topY == bottomY)
        return;

    // If completely above the slice, just advance both edges
    if (bottomY <= sliceStartY)
    {
        int clip = bottomY - topY;
        leftX += leftEdge.xStep * clip;
        leftInvZ += leftEdge.invZStep * clip;
        rightX += rightEdge.xStep * clip;
        return;
    }

    // Clip top and advance edges
    if (topY < sliceStartY)
    {
        int clip = sliceStartY - topY;
        topY += clip;
        leftX += leftEdge.xStep * clip;
        leftInvZ += leftEdge.invZStep * clip;
        rightX += rightEdge.xStep * clip;
    }

    // Clip bottom
    if (bottomY > sliceEndY)
        bottomY = sliceEndY;

    // Rasterize the part of edges inside slice
    float* row = buffer + topY * width;
    float* endRow = buffer + bottomY * width;
    while (row < endRow)
    {
        float invZ = leftInvZ;
        float* dest = row + (leftX >> 16);
        float* end = row + (rightX >> 16);
        while (dest < end)
        {
            if (invZ < *dest)
                *dest = invZ;
            invZ += dInvZdX;
            ++dest;
        }

        leftX += leftEdge.xStep;
        leftInvZ += leftEdge.invZStep;
        rightX += rightEdge.xStep;
        row += width;
    }
}

void OcclusionBuffer::GenerateTrianglesWork(Task* task, unsigned)
{
    ZoneScoped;

    GenerateTrianglesTask* trianglesTask = static_cast<GenerateTrianglesTask*>(task);
    const TriangleDrawBatch& batch = trianglesTask->batch;
    Matrix4 worldViewProj = viewProj * batch.worldTransform;

    // Theoretical max. amount of vertices if each of the 6 clipping planes doubles the triangle count
    Vector4 vertices[64 * 3];

    if (!batch.indexData)
    {
        // Non-indexed
        unsigned char* srcData = (unsigned char*)batch.vertexData;
        size_t index = 0;

        while (index + 2 < batch.drawCount)
        {
            const Vector3& v0 = *((const Vector3*)(&srcData[index * batch.vertexSize]));
            const Vector3& v1 = *((const Vector3*)(&srcData[(index + 1) * batch.vertexSize]));
            const Vector3& v2 = *((const Vector3*)(&srcData[(index + 2) * batch.vertexSize]));

            vertices[0] = ModelTransform(worldViewProj, v0);
            vertices[1] = ModelTransform(worldViewProj, v1);
            vertices[2] = ModelTransform(worldViewProj, v2);
            AddTriangle(trianglesTask, vertices);

            index += 3;
        }
    }
    else
    {
        // 16-bit indices
        if (batch.indexSize == sizeof(unsigned short))
        {
            unsigned char* srcData = (unsigned char*)batch.vertexData;
            const unsigned short* indices = (const unsigned short*)batch.indexData;
            const unsigned short* indicesEnd = indices + batch.drawCount;

            while (indices < indicesEnd)
            {
                const Vector3& v0 = *((const Vector3*)(&srcData[indices[0] * batch.vertexSize]));
                const Vector3& v1 = *((const Vector3*)(&srcData[indices[1] * batch.vertexSize]));
                const Vector3& v2 = *((const Vector3*)(&srcData[indices[2] * batch.vertexSize]));

                vertices[0] = ModelTransform(worldViewProj, v0);
                vertices[1] = ModelTransform(worldViewProj, v1);
                vertices[2] = ModelTransform(worldViewProj, v2);
                AddTriangle(trianglesTask, vertices);

                indices += 3;
            }
        }
        else
        {
            unsigned char* srcData = (unsigned char*)batch.vertexData;
            const unsigned* indices = (const unsigned*)batch.indexData;
            const unsigned* indicesEnd = indices + batch.drawCount;

            while (indices < indicesEnd)
            {
                const Vector3& v0 = *((const Vector3*)(&srcData[indices[0] * batch.vertexSize]));
                const Vector3& v1 = *((const Vector3*)(&srcData[indices[1] * batch.vertexSize]));
                const Vector3& v2 = *((const Vector3*)(&srcData[indices[2] * batch.vertexSize]));

                vertices[0] = ModelTransform(worldViewProj, v0);
                vertices[1] = ModelTransform(worldViewProj, v1);
                vertices[2] = ModelTransform(worldViewProj, v2);
                AddTriangle(trianglesTask, vertices);

                indices += 3;
            }
        }
    }

    // Start rasterization once triangles for all batches have been generated
    if (numPendingGenerateTasks.fetch_add(-1) == 1)
    {
        numPendingRasterizeTasks.store(activeSlices);
        workQueue->QueueTasks(activeSlices, reinterpret_cast<Task**>(&rasterizeTrianglesTasks[0]));
    }
}

void OcclusionBuffer::RasterizeTrianglesWork(Task* task, unsigned)
{
    ZoneScoped;

    RasterizeTrianglesTask* rasterizeTask = static_cast<RasterizeTrianglesTask*>(task);
    int sliceStartY = rasterizeTask->startY;
    int sliceEndY = rasterizeTask->endY;

    for (int y = sliceStartY; y < sliceEndY; ++y)
    {
        float* start = buffer + width * y;
        float* end = buffer + width * y + width;

        while (start < end)
            *start++ = 1.0f;
    }

    for (size_t i = 0; i < numTriangleBatches; ++i)
    {
        GenerateTrianglesTask* trianglesTask = generateTrianglesTasks[i];
        const std::vector<GradientTriangle>& triangles = trianglesTask->triangles;
        const std::vector<unsigned>& indices = trianglesTask->triangleIndices[rasterizeTask->sliceIdx];

        for (auto it = indices.begin(); it != indices.end(); ++it)
        {
            const GradientTriangle& triangle = triangles[*it];

            if (triangle.middleIsRight)
            {
                int leftX = triangle.topToBottom.x;
                float leftInvZ = triangle.topToBottom.invZ;
                int rightX = triangle.topToMiddle.x;
                RasterizeSpans(triangle.topToBottom, triangle.topToMiddle, triangle.topToMiddle.topY, triangle.topToMiddle.bottomY, triangle.dInvZdX, sliceStartY, sliceEndY, leftX, leftInvZ, rightX);

                rightX = triangle.middleToBottom.x;
                RasterizeSpans(triangle.topToBottom, triangle.middleToBottom, triangle.middleToBottom.topY, triangle.middleToBottom.bottomY, triangle.dInvZdX, sliceStartY, sliceEndY, leftX, leftInvZ, rightX);
            }
            else
            {
                int leftX = triangle.topToMiddle.x;
                float leftInvZ = triangle.topToMiddle.invZ;
                int rightX = triangle.topToBottom.x;
                RasterizeSpans(triangle.topToMiddle, triangle.topToBottom, triangle.topToMiddle.topY, triangle.topToMiddle.bottomY, triangle.dInvZdX, sliceStartY, sliceEndY, leftX, leftInvZ, rightX);

                leftX = triangle.middleToBottom.x;
                leftInvZ = triangle.middleToBottom.invZ;
                RasterizeSpans(triangle.middleToBottom, triangle.topToBottom, triangle.middleToBottom.topY, triangle.middleToBottom.bottomY, triangle.dInvZdX, sliceStartY, sliceEndY, leftX, leftInvZ, rightX);
            }
        }
    }

    // After finishing, build part of first miplevel
    int mipWidth = (width + 1) / 2;

    for (int y = sliceStartY / 2; y < sliceEndY / 2; ++y)
    {
        float* src = buffer + (y * 2) * width;
        DepthValue* dest = mipBuffers[0] + y * mipWidth;
        DepthValue* end = dest + mipWidth;

        if (y * 2 + 1 < height)
        {
            float* src2 = src + width;

            while (dest < end)
            {
                dest->min = Min(Min(src[0], src[1]), Min(src2[0], src2[1]));
                dest->max = Max(Max(src[0], src[1]), Max(src2[0], src2[1]));
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

    // If done, build rest of the depth hierarchy
    if (numPendingRasterizeTasks.fetch_add(-1) == 1)
    {
        numReadyMipBuffers = 1;
        workQueue->QueueTask(depthHierarchyTask);
    }
}

void OcclusionBuffer::BuildDepthHierarchyWork(Task*, unsigned)
{
    ZoneScoped;

    // The first mip level has been built by the rasterize tasks
    int mipWidth = (width + 1) / 2;
    int mipHeight = (height + 1) / 2;

    // Build the rest of the mip levels
    for (size_t i = 1; i < mipBuffers.size(); ++i)
    {
        int prevWidth = mipWidth;
        int prevHeight = mipHeight;
        mipWidth = (mipWidth + 1) / 2;
        mipHeight = (mipHeight + 1) / 2;

        for (int y = 0; y < mipHeight; ++y)
        {
            DepthValue* src = mipBuffers[i - 1] + (y * 2) * prevWidth;
            DepthValue* dest = mipBuffers[i] + y * mipWidth;
            DepthValue* end = dest + mipWidth;

            if (y * 2 + 1 < prevHeight)
            {
                DepthValue* src2 = src + prevWidth;

                while (dest < end)
                {
                    dest->min = Min(Min(src[0].min, src[1].min), Min(src2[0].min, src2[1].min));
                    dest->max = Max(Max(src[0].max, src[1].max), Max(src2[0].max, src2[1].max));
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

        ++numReadyMipBuffers;
    }
}
