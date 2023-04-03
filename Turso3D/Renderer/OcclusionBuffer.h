// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Frustum.h"
#include "../Thread/WorkQueue.h"

class BoundingBox;
class Camera;
class IntRect;
struct GenerateTrianglesTask;
struct RasterizeTrianglesTask;

static const int OCCLUSION_MIN_SIZE = 8;
static const int OCCLUSION_BUFFER_SLICES = 8;
static const float OCCLUSION_X_SCALE = 65536.0f;
static const float OCCLUSION_Z_SCALE = 1073741824.0f;

// Rasterizer code based on Chris Hecker's Perspective Texture Mapping series in the Game Developer magazine
// Also available online at http://chrishecker.com/Miscellaneous_Technical_Articles

/// Software rasterized triangle batch.
struct TriangleDrawBatch
{
    /// Transform matrix.
    Matrix3x4 worldTransform;
    /// Vertex data.
    const void* vertexData;
    /// Vertex size.
    size_t vertexSize;
    /// Index data, optional.
    const void* indexData;
    /// Index size.
    size_t indexSize;
    /// Draw range count.
    size_t drawCount;
};

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
        dInvZdXInt = dInvZdX >= 0.0f ? (int)(dInvZdX + 0.5f) : (int)(dInvZdX - 0.5f);
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
    /// Calculate from gradients and top & bottom vertices.
    void Calculate(const Gradients& gradients, const Vector3& top, const Vector3& bottom)
    {
        int intTopY = (int)top.y;
        float slope = (bottom.x - top.x) / (bottom.y - top.y);
        float yPreStep = (float)(intTopY + 1) - top.y;
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

/// Stored triangle with gradients and edges calculated.
struct GradientTriangle
{
    /// Sort vertices, then calculate gradients and edges.
    void Calculate()
    {
        if (vertices[0].y < vertices[1].y)
        {
            if (vertices[2].y < vertices[0].y)
            {
                std::swap(vertices[1], vertices[2]);
                std::swap(vertices[0], vertices[1]);
                middleIsRight = true;
            }
            else
            {
                if (vertices[1].y < vertices[2].y)
                {
                    middleIsRight = true;
                }
                else
                {
                    std::swap(vertices[1], vertices[2]);
                    middleIsRight = false;
                }
            }
        }
        else
        {
            if (vertices[2].y < vertices[1].y)
            {
                std::swap(vertices[0], vertices[2]);
                middleIsRight = false;
            }
            else
            {
                if (vertices[0].y < vertices[2].y)
                {
                    std::swap(vertices[0], vertices[1]);
                    middleIsRight = false;
                }
                else
                {
                    std::swap(vertices[0], vertices[1]);
                    std::swap(vertices[1], vertices[2]);
                    middleIsRight = true;
                }
            }
        }

        gradients.Calculate(vertices);
        topToMiddle.Calculate(gradients, vertices[0], vertices[1]);
        middleToBottom.Calculate(gradients, vertices[1], vertices[2]);
        topToBottom.Calculate(gradients, vertices[0], vertices[2]);
    }

    /// Triangle vertices.
    Vector3 vertices[3];
    /// Gradients calculated from the vertices.
    Gradients gradients;
    /// Top to middle edge.
    Edge topToMiddle;
    /// Middle to bottom edge.
    Edge middleToBottom;
    /// Top to bottom edge.
    Edge topToBottom;
    /// Whether middle vertex is on the right.
    bool middleIsRight;
};

/// Occlusion mip buffer depth range.
struct DepthValue
{
    /// Minimum value.
    int min;
    /// Maximum value.
    int max;
};

/// Software rasterizer for occlusion.
class OcclusionBuffer : public Object
{
    OBJECT(OcclusionBuffer);

public:
    /// Construct.
    OcclusionBuffer();
    /// Destruct.
    ~OcclusionBuffer();
    
    /// Set occlusion buffer size.
    bool SetSize(int newWidth, int newHeight);
    /// Set camera view to render from.
    void SetView(Camera* camera);
    /// Reset number of triangles. Call before use each frame.
    void Reset();
    /// Add a non-indexed triangle mesh to be rasterized.
    void AddTriangles(const Matrix3x4& worldTransform, const void* vertexData, size_t vertexSize, size_t vertexStart, size_t vertexCount);
    /// Add an indexed triangle mesh to be rasterized.
    void AddTriangles(const Matrix3x4& worldTransform, const void* vertexData, size_t vertexSize, const void* indexData, size_t indexSize, size_t indexStart, size_t indexCount);
    /// Begin rasterizing triangles and then build the depth hierarchy, using worker threads.
    void DrawTriangles();
    /// Wait until rasterization is complete.
    void Complete();
    
    /// Return highest level depth values.
    int* Buffer() const { return buffer; }
    /// Return current buffer dimensions.
    IntVector2 Size() const { return IntVector2(width, height); }
    /// Return buffer width.
    int Width() const { return width; }
    /// Return buffer height.
    int Height() const { return height; }
    /// Return the last set view-projection matrix.
    const Matrix4& ViewProjectionMatrix() const { return viewProj; }
    /// Test if rasterization is complete. Visibility tests cannot be performed before.
    bool IsCompleted() const;
    /// Test a bounding box for visibility. For best performance, build depth hierarchy first.
    bool IsVisible(const BoundingBox& worldSpaceBox) const;
    
private:
    /// Perform transform of model vertex to clip space.
    Vector4 ModelTransform(const Matrix4& transform, const Vector3& vertex) const
    {
        return Vector4(
            transform.m00 * vertex.x + transform.m01 * vertex.y + transform.m02 * vertex.z + transform.m03,
            transform.m10 * vertex.x + transform.m11 * vertex.y + transform.m12 * vertex.z + transform.m13,
            transform.m20 * vertex.x + transform.m21 * vertex.y + transform.m22 * vertex.z + transform.m23,
            transform.m30 * vertex.x + transform.m31 * vertex.y + transform.m32 * vertex.z + transform.m33
        );
    }

    /// Perform transform of model vertex from clip to viewport space.
    Vector3 ViewportTransform(const Vector4& vertex) const
    {
        float invW = 1.0f / vertex.w;
        return Vector3(
            invW * vertex.x * scaleX + offsetX,
            invW * vertex.y * scaleY + offsetY,
            invW * vertex.z * OCCLUSION_Z_SCALE
        );
    }

    /// Clip a clip space edge.
    Vector4 ClipEdge(const Vector4& v0, const Vector4& v1, float d0, float d1) const
    {
        float t = d0 / (d0 - d1);
        return v0 + t * (v1 - v0);
    }

    /// Check triangle facing after viewport transform. Must be clockwise to render.
    bool CheckFacing(const Vector3& v0, const Vector3& v1, const Vector3& v2) const
    {
        float aX = v0.x - v1.x;
        float aY = v0.y - v1.y;
        float bX = v2.x - v1.x;
        float bY = v2.y - v1.y;
        return (aX * bY - aY * bX) < 0.0f;
    }

    /// Rasterize between two edges. Clip to slice.
    void RasterizeSpans(Edge& left, Edge& right, int topY, int bottomY, int pixelInvZStep, int sliceStartY, int sliceEndY)
    {
        // If past the bottom or degenerate, no operation
        if (topY >= sliceEndY || topY == bottomY)
            return;

        // If completely above the slice, just advance both edges
        if (bottomY <= sliceStartY)
        {
            int clip = bottomY - topY;
            left.x += left.xStep * clip;
            left.invZ += left.invZStep * clip;
            right.x += right.xStep * clip;
            return;
        }

        // Clip top and advance edges
        if (topY < sliceStartY)
        {
            int clip = sliceStartY - topY;
            topY += clip;
            left.x += left.xStep * clip;
            left.invZ += left.invZStep * clip;
            right.x += right.xStep * clip;
        }

        // Clip bottom
        if (bottomY > sliceEndY)
            bottomY = sliceEndY;

        // Rasterize the part of edges inside slice
        int* row = buffer + topY * width;
        int* endRow = buffer + bottomY * width;
        while (row < endRow)
        {
            int invZ = left.invZ;
            int* dest = row + (left.x >> 16);
            int* end = row + (right.x >> 16);
            while (dest < end)
            {
                if (invZ < *dest)
                    *dest = invZ;
                invZ += pixelInvZStep;
                ++dest;
            }

            left.x += left.xStep;
            left.invZ += left.invZStep;
            right.x += right.xStep;
            row += width;
        }
    }

    /// Clip and add a triangle into per-slice triangle lists.
    void AddTriangle(GenerateTrianglesTask* task, Vector4* vertices);
    /// Clip vertices against a plane.
    void ClipVertices(const Vector4& plane, Vector4* vertices, bool* clipTriangles, size_t& numClipTriangles);
    /// Work function to generate clipped triangles.
    void GenerateTrianglesWork(Task* task, unsigned threadIndex);
    /// Work function to clear a depth buffer slice and rasterize triangles.
    void RasterizeTrianglesWork(Task* task, unsigned threadIndex);
    /// Work function to build the reduced size depth buffers.
    void BuildDepthHierarchyWork(Task* task, unsigned threadIndex);

    /// Cached work queue subsystem.
    WorkQueue* workQueue;
    /// Highest level depth buffer.
    int* buffer;
    /// Buffer width.
    int width;
    /// Buffer height.
    int height;
    /// Per-thread slice size.
    int sliceHeight;
    /// How many slices are actually needed, may be less due to rounding.
    int activeSlices;
    /// Combined view and projection matrix.
    Matrix4 viewProj;
    /// X scaling for viewport transform.
    float scaleX;
    /// Y scaling for viewport transform.
    float scaleY;
    /// X offset for viewport transform.
    float offsetX;
    /// Y offset for viewport transform.
    float offsetY;
    /// Number of active triangle batches.
    size_t numTriangleBatches;
    /// Number of ready reduced size buffers.
    volatile size_t numReadyMipBuffers;
    /// Number of pending generate triangles tasks. When all done, the triangles can be rasterized.
    std::atomic<int> numPendingGenerateTasks;
    /// Number of pending rasterize triangles tasks. When all done, tests can begin and depth hierarchy can be generated to speed up tests.
    std::atomic<int> numPendingRasterizeTasks;
    /// Reduced size depth buffers.
    std::vector<SharedArrayPtr<DepthValue> > mipBuffers;
    /// Highest level buffer with safety padding.
    AutoArrayPtr<int> fullBuffer;
    /// %Task for threaded depth hierarchy building.
    AutoPtr<MemberFunctionTask<OcclusionBuffer> > depthHierarchyTask;
    /// Tasks for generating triangle batches, one for each batch.
    std::vector<AutoPtr<GenerateTrianglesTask> > generateTrianglesTasks;
    /// Tasks for depth buffer clearing and triangle rasterization.
    AutoPtr<RasterizeTrianglesTask> rasterizeTrianglesTasks[OCCLUSION_BUFFER_SLICES];
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