// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Graphics/GraphicsDefs.h"
#include "../Math/Frustum.h"
#include "../Object/Ptr.h"
#include "../Thread/WorkQueue.h"

class BoundingBox;
class Camera;
class IndexBuffer;
class IntRect;
class VertexBuffer;
struct Edge;
struct Gradients;

/// Occlusion hierarchy depth range.
struct DepthValue
{
    /// Minimum value.
    int min;
    /// Maximum value.
    int max;
};

static const int OCCLUSION_MIN_SIZE = 8;
static const size_t OCCLUSION_DEFAULT_MAX_TRIANGLES = 8192;
static const float OCCLUSION_X_SCALE = 65536.0f;
static const float OCCLUSION_Z_SCALE = 1073741824.0f;

/// Software rasterizer for occlusion.
class OcclusionBuffer : public RefCounted
{
public:
    /// Construct.
    OcclusionBuffer();
    /// Destruct.
    ~OcclusionBuffer();
    
    /// Set occlusion buffer size.
    bool SetSize(int newWidth, int newHeight);
    /// Set camera view to render from.
    void SetView(Camera* camera);
    /// Set maximum triangles to render.
    void SetMaxTriangles(unsigned triangles);
    /// Only reset number of triangles but do not clear.
    void Reset();
    /// Clear the buffer and reset number of triangles.
    void Clear();
    /// Draw a triangle mesh to the buffer using non-indexed geometry.
    bool Draw(const Matrix3x4& model, const void* vertexData, size_t vertexSize, size_t vertexStart, size_t vertexCount);
    /// Draw a triangle mesh to the buffer using indexed geometry.
    bool Draw(const Matrix3x4& model, const void* vertexData, size_t vertexSize, const void* indexData, size_t indexSize, size_t indexStart, size_t indexCount);
    /// Build reduced size mip levels. Optionally queue as a task for threaded execution. Occlusion testing can proceed on higher mip levels until all levels built.
    void BuildDepthHierarchy(bool threaded);
    
    /// Return highest level depth values.
    int* Buffer() const { return buffer; }
    /// Return view transform matrix.
    const Matrix3x4& ViewMatrix() const { return view; }
    /// Return projection matrix.
    const Matrix4& ProjectionMatrix() const { return projection; }
    /// Return buffer width.
    int Width() const { return width; }
    /// Return buffer height.
    int Height() const { return height; }
    /// Return number of rendered triangles.
    size_t NumTriangles() const { return numTriangles; }
    /// Return maximum number of triangles.
    size_t MaxTriangles() const { return maxTriangles; }
    /// Test a bounding box for visibility. For best performance, build depth hierarchy first.
    bool IsVisible(const BoundingBox& worldSpaceBox) const;
    
private:
    Vector4 ModelTransform(const Matrix4& transform, const Vector3& vertex) const
    {
        return Vector4(
            transform.m00 * vertex.x + transform.m01 * vertex.y + transform.m02 * vertex.z + transform.m03,
            transform.m10 * vertex.x + transform.m11 * vertex.y + transform.m12 * vertex.z + transform.m13,
            transform.m20 * vertex.x + transform.m21 * vertex.y + transform.m22 * vertex.z + transform.m23,
            transform.m30 * vertex.x + transform.m31 * vertex.y + transform.m32 * vertex.z + transform.m33
        );
    }

    Vector3 ViewportTransform(const Vector4& vertex) const
    {
        float invW = 1.0f / vertex.w;
        return Vector3(
            invW * vertex.x * scaleX + offsetX,
            invW * vertex.y * scaleY + offsetY,
            invW * vertex.z * OCCLUSION_Z_SCALE
        );
    }

    Vector4 ClipEdge(const Vector4& v0, const Vector4& v1, float d0, float d1) const
    {
        float t = d0 / (d0 - d1);
        return v0 + t * (v1 - v0);
    }

    bool CheckFacing(const Vector3& v0, const Vector3& v1, const Vector3& v2) const
    {
        float aX = v0.x - v1.x;
        float aY = v0.y - v1.y;
        float bX = v2.x - v1.x;
        float bY = v2.y - v1.y;
        float signedArea = aX * bY - aY * bX;

        return signedArea < 0.0f;
    }

    /// Calculate viewport transform.
    void CalculateViewport();
    /// Draw a triangle.
    void DrawTriangle(Vector4* vertices);
    /// Clip vertices against a plane.
    void ClipVertices(const Vector4& plane, Vector4* vertices, bool* clipTriangles, size_t& numClipTriangles);
    /// Draw a clipped triangle.
    void DrawTriangle2D(const Vector3* vertices);
    /// Work function to build depth hierarchies.
    void BuildDepthHierarchyWork(Task* task, unsigned threadIndex);

    /// Highest level depth buffer.
    int* buffer;
    /// Buffer width.
    int width;
    /// Buffer height.
    int height;
    /// Number of rendered triangles.
    size_t numTriangles;
    /// Maximum number of triangles.
    size_t maxTriangles;
    /// View transform matrix.
    Matrix3x4 view;
    /// Projection matrix.
    Matrix4 projection;
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
    /// Number of ready reduced size buffers.
    volatile size_t numReadyMipBuffers;
    /// Reduced size depth buffers.
    std::vector<SharedArrayPtr<DepthValue> > mipBuffers;
    /// Highest level buffer with safety padding.
    AutoArrayPtr<int> fullBuffer;
    /// Task for threaded depth hierarchy building.
    AutoPtr<MemberFunctionTask<OcclusionBuffer> > depthHierarchyTask;
};
