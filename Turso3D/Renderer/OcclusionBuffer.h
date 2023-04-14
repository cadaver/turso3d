// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Frustum.h"
#include "../Thread/WorkQueue.h"

class BoundingBox;
class Camera;
class IntRect;
struct Edge;
struct GenerateTrianglesTask;
struct RasterizeTrianglesTask;

static const int OCCLUSION_MIN_SIZE = 8;
static const int OCCLUSION_BUFFER_SLICES = 8;
static const float OCCLUSION_X_SCALE = 65536.0f;

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

/// Occlusion mip buffer depth range.
struct DepthValue
{
    /// Minimum value.
    float min;
    /// Maximum value.
    float max;
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
    float* Buffer() const { return buffer; }
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
    /// Perform transform of model vertex from clip to viewport space.
    inline Vector3 ViewportTransform(const Vector4& vertex) const;
    /// Rasterize between two edges. Clip to slice.
    inline void RasterizeSpans(const Edge& leftEdge, const Edge& rightEdge, int topY, int bottomY, float dInvZdX, int sliceStartY, int sliceEndY, int& leftX, float& leftInvZ, int& rightX);

    /// Cached work queue subsystem.
    WorkQueue* workQueue;
    /// Highest level depth buffer.
    float* buffer;
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
    std::vector<AutoArrayPtr<DepthValue> > mipBuffers;
    /// Highest level buffer with safety padding.
    AutoArrayPtr<float> fullBuffer;
    /// %Task for threaded depth hierarchy building.
    AutoPtr<MemberFunctionTask<OcclusionBuffer> > depthHierarchyTask;
    /// Tasks for generating triangle batches, one for each batch.
    std::vector<AutoPtr<GenerateTrianglesTask> > generateTrianglesTasks;
    /// Tasks for depth buffer clearing and triangle rasterization.
    AutoPtr<RasterizeTrianglesTask> rasterizeTrianglesTasks[OCCLUSION_BUFFER_SLICES];
};
