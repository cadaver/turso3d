// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Graphics/GraphicsDefs.h"
#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Object/Object.h"

class BoundingBox;
class Camera;
class IndexBuffer;
class Polyhedron;
class Matrix3x4;
class ShaderProgram;
class Sphere;
class VertexBuffer;

/// Debug rendering vertex.
struct DebugVertex
{
    /// Construct with values.
    DebugVertex(const Vector3& position_, unsigned color_) :
        position(position_),
        color(color_)
    {
    }

    /// World-space position.
    Vector3 position;
    /// 32-bit RGBA vertex color.
    unsigned color;
};

/// Debug line geometry rendering subsystem.
class DebugRenderer : public Object
{
    OBJECT(DebugRenderer);

public:
    /// Construct. Register subsystem. Graphics subsystem must have been initialized.
    DebugRenderer();
    /// Destruct.
    ~DebugRenderer();

    /// Set the camera viewpoint. Call before rendering, or before adding geometry if you want to use culling.
    void SetView(Camera* camera);
    /// Add a line.
    void AddLine(const Vector3& start, const Vector3& end, const Color& color, bool depthTest = true);
    /// Add a line with color already converted to unsigned.
    void AddLine(const Vector3& start, const Vector3& end, unsigned color, bool depthTest = true);
    /// Add a bounding box.
    void AddBoundingBox(const BoundingBox& box, const Color& color, bool depthTest = true);
    /// Add a bounding box with transform.
    void AddBoundingBox(const BoundingBox& box, const Matrix3x4& transform, const Color& color, bool depthTest = true);
    /// Add a frustum.
    void AddFrustum(const Frustum& frustum, const Color& color, bool depthTest = true);
    /// Add a polyhedron.
    void AddPolyhedron(const Polyhedron& poly, const Color& color, bool depthTest = true);
    /// Add a sphere.
    void AddSphere(const Sphere& sphere, const Color& color, bool depthTest = true);
    /// Add a cylinder.
    void AddCylinder(const Vector3& position, float radius, float height, const Color& color, bool depthTest = true);
    /// Update vertex buffer and render all debug lines to the currently set framebuffer and viewport. Then clear the lines for the next frame.
    void Render();

    /// Check whether a bounding box is inside the view frustum.
    bool IsInside(const BoundingBox& box) const { return frustum.IsInsideFast(box) == INSIDE; }

private:
    /// Debug geometry vertices.
    std::vector<DebugVertex> vertices;
    /// Indices rendered with depth test.
    std::vector<unsigned> indices;
    /// Indices rendered without depth test.
    std::vector<unsigned> noDepthIndices;
    /// View transform.
    Matrix3x4 view;
    /// Projection transform.
    Matrix4 projection;
    /// View frustum.
    Frustum frustum;
    /// Vertex buffer for the debug geometry.
    AutoPtr<VertexBuffer> vertexBuffer;
    /// Index buffer for the debug geometry.
    AutoPtr<IndexBuffer> indexBuffer;
    /// Vertex elements for the debug vertices.
    std::vector<VertexElement> vertexElements;
    /// Cached shader program.
    SharedPtr<ShaderProgram> shaderProgram;
};
