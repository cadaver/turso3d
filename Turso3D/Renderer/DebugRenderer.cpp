// For conditions of distribution and use, see copyright notice in License.txt

#include "../Graphics/Graphics.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/ShaderProgram.h"
#include "../Graphics/VertexBuffer.h"
#include "../Math/Polyhedron.h"
#include "Camera.h"
#include "DebugRenderer.h"

#include <tracy/Tracy.hpp>

DebugRenderer::DebugRenderer()
{
    RegisterSubsystem(this);
    
    vertexBuffer = new VertexBuffer();
    vertexElements.push_back(VertexElement(ELEM_VECTOR3, SEM_POSITION));
    vertexElements.push_back(VertexElement(ELEM_UBYTE4, SEM_COLOR));

    indexBuffer = new IndexBuffer();

    shaderProgram = Subsystem<Graphics>()->CreateProgram("Shaders/DebugLines.glsl");
}

DebugRenderer::~DebugRenderer()
{
    RemoveSubsystem(this);
}

void DebugRenderer::SetView(Camera* camera)
{
    if (!camera)
        return;

    view = camera->ViewMatrix();
    projection = camera->ProjectionMatrix();
    frustum = camera->WorldFrustum();
}

void DebugRenderer::AddLine(const Vector3& start, const Vector3& end, const Color& color, bool depthTest)
{
    AddLine(start, end, color.ToUInt(), depthTest);
}

void DebugRenderer::AddLine(const Vector3& start, const Vector3& end, unsigned color, bool depthTest)
{
    unsigned startVertex = (unsigned)vertices.size();

    vertices.push_back(DebugVertex(start, color));
    vertices.push_back(DebugVertex(end, color));

    std::vector<unsigned>& dest = depthTest ? indices : noDepthIndices;
    dest.push_back(startVertex);
    dest.push_back(startVertex + 1);
}

void DebugRenderer::AddBoundingBox(const BoundingBox& box, const Color& color, bool depthTest)
{
    unsigned startVertex = (unsigned)vertices.size();
    unsigned uintColor = color.ToUInt();

    const Vector3& min = box.min;
    const Vector3& max = box.max;

    vertices.push_back(DebugVertex(min, uintColor));
    vertices.push_back(DebugVertex(Vector3(max.x, min.y, min.z), uintColor));
    vertices.push_back(DebugVertex(Vector3(max.x, max.y, min.z), uintColor));
    vertices.push_back(DebugVertex(Vector3(min.x, max.y, min.z), uintColor));
    vertices.push_back(DebugVertex(Vector3(min.x, min.y, max.z), uintColor));
    vertices.push_back(DebugVertex(Vector3(max.x, min.y, max.z), uintColor));
    vertices.push_back(DebugVertex(Vector3(min.x, max.y, max.z), uintColor));
    vertices.push_back(DebugVertex(max, uintColor));

    std::vector<unsigned>& dest = depthTest ? indices : noDepthIndices;

    dest.push_back(startVertex);
    dest.push_back(startVertex + 1);

    dest.push_back(startVertex + 1);
    dest.push_back(startVertex + 2);

    dest.push_back(startVertex + 2);
    dest.push_back(startVertex + 3);

    dest.push_back(startVertex + 3);
    dest.push_back(startVertex);

    dest.push_back(startVertex + 4);
    dest.push_back(startVertex + 5);

    dest.push_back(startVertex + 5);
    dest.push_back(startVertex + 7);

    dest.push_back(startVertex + 7);
    dest.push_back(startVertex + 6);

    dest.push_back(startVertex + 6);
    dest.push_back(startVertex + 4);

    dest.push_back(startVertex + 0);
    dest.push_back(startVertex + 4);

    dest.push_back(startVertex + 1);
    dest.push_back(startVertex + 5);

    dest.push_back(startVertex + 2);
    dest.push_back(startVertex + 7);

    dest.push_back(startVertex + 3);
    dest.push_back(startVertex + 6);
}

void DebugRenderer::AddBoundingBox(const BoundingBox& box, const Matrix3x4& transform, const Color& color, bool depthTest)
{
    unsigned startVertex = (unsigned)vertices.size();
    unsigned uintColor = color.ToUInt();

    const Vector3& min = box.min;
    const Vector3& max = box.max;

    vertices.push_back(DebugVertex(Vector3(transform * min), uintColor));
    vertices.push_back(DebugVertex(Vector3(transform * Vector3(max.x, min.y, min.z)), uintColor));
    vertices.push_back(DebugVertex(Vector3(transform * Vector3(max.x, max.y, min.z)), uintColor));
    vertices.push_back(DebugVertex(Vector3(transform * Vector3(min.x, max.y, min.z)), uintColor));
    vertices.push_back(DebugVertex(Vector3(transform * Vector3(min.x, min.y, max.z)), uintColor));
    vertices.push_back(DebugVertex(Vector3(transform * Vector3(max.x, min.y, max.z)), uintColor));
    vertices.push_back(DebugVertex(Vector3(transform * Vector3(min.x, max.y, max.z)), uintColor));
    vertices.push_back(DebugVertex(Vector3(transform * max), uintColor));

    std::vector<unsigned>& dest = depthTest ? indices : noDepthIndices;

    dest.push_back(startVertex);
    dest.push_back(startVertex + 1);

    dest.push_back(startVertex + 1);
    dest.push_back(startVertex + 2);

    dest.push_back(startVertex + 2);
    dest.push_back(startVertex + 3);

    dest.push_back(startVertex + 3);
    dest.push_back(startVertex);

    dest.push_back(startVertex + 4);
    dest.push_back(startVertex + 5);

    dest.push_back(startVertex + 5);
    dest.push_back(startVertex + 7);

    dest.push_back(startVertex + 7);
    dest.push_back(startVertex + 6);

    dest.push_back(startVertex + 6);
    dest.push_back(startVertex + 4);

    dest.push_back(startVertex + 0);
    dest.push_back(startVertex + 4);

    dest.push_back(startVertex + 1);
    dest.push_back(startVertex + 5);

    dest.push_back(startVertex + 2);
    dest.push_back(startVertex + 7);

    dest.push_back(startVertex + 3);
    dest.push_back(startVertex + 6);
}

void DebugRenderer::AddFrustum(const Frustum& frustum_, const Color& color, bool depthTest)
{
    unsigned startVertex = (unsigned)vertices.size();
    unsigned uintColor = color.ToUInt();

    vertices.push_back(DebugVertex(frustum_.vertices[0], uintColor));
    vertices.push_back(DebugVertex(frustum_.vertices[1], uintColor));
    vertices.push_back(DebugVertex(frustum_.vertices[2], uintColor));
    vertices.push_back(DebugVertex(frustum_.vertices[3], uintColor));
    vertices.push_back(DebugVertex(frustum_.vertices[4], uintColor));
    vertices.push_back(DebugVertex(frustum_.vertices[5], uintColor));
    vertices.push_back(DebugVertex(frustum_.vertices[6], uintColor));
    vertices.push_back(DebugVertex(frustum_.vertices[7], uintColor));

    std::vector<unsigned>& dest = depthTest ? indices : noDepthIndices;

    dest.push_back(startVertex);
    dest.push_back(startVertex + 1);

    dest.push_back(startVertex + 1);
    dest.push_back(startVertex + 2);

    dest.push_back(startVertex + 2);
    dest.push_back(startVertex + 3);

    dest.push_back(startVertex + 3);
    dest.push_back(startVertex);

    dest.push_back(startVertex + 4);
    dest.push_back(startVertex + 5);

    dest.push_back(startVertex + 5);
    dest.push_back(startVertex + 6);

    dest.push_back(startVertex + 6);
    dest.push_back(startVertex + 7);

    dest.push_back(startVertex + 7);
    dest.push_back(startVertex + 4);

    dest.push_back(startVertex);
    dest.push_back(startVertex + 4);

    dest.push_back(startVertex + 1);
    dest.push_back(startVertex + 5);

    dest.push_back(startVertex + 2);
    dest.push_back(startVertex + 6);

    dest.push_back(startVertex + 3);
    dest.push_back(startVertex + 7);
}

void DebugRenderer::AddPolyhedron(const Polyhedron& poly, const Color& color, bool depthTest)
{
    unsigned uintColor = color.ToUInt();

    for (size_t i = 0; i < poly.faces.size(); ++i)
    {
        const std::vector<Vector3>& face = poly.faces[i];
        if (face.size() >= 3)
        {
            for (size_t j = 0; j < face.size(); ++j)
                AddLine(face[j], face[(j + 1) % face.size()], uintColor, depthTest);
        }
    }
}

void DebugRenderer::AddSphere(const Sphere& sphere, const Color& color, bool depthTest)
{
    unsigned uintColor = color.ToUInt();

    for (float j = 0.0f; j < 180.0f; j += 45.0f)
    {
        for (float i = 0.0f; i < 360.0f; i += 45.0f)
        {
            unsigned startVertex = (unsigned)vertices.size();

            vertices.push_back(DebugVertex(sphere.Point(i, j), uintColor));
            vertices.push_back(DebugVertex(sphere.Point(i + 45.0f, j), uintColor));
            vertices.push_back(DebugVertex(sphere.Point(i, j + 45.0f), uintColor));
            vertices.push_back(DebugVertex(sphere.Point(i + 45.0f, j + 45.0f), uintColor));

            std::vector<unsigned>& dest = depthTest ? indices : noDepthIndices;

            dest.push_back(startVertex);
            dest.push_back(startVertex + 1);

            dest.push_back(startVertex + 2);
            dest.push_back(startVertex + 3);

            dest.push_back(startVertex);
            dest.push_back(startVertex + 2);

            dest.push_back(startVertex + 1);
            dest.push_back(startVertex + 3);
        }
    }
}

void DebugRenderer::AddCylinder(const Vector3& position, float radius, float height, const Color& color, bool depthTest)
{
    Sphere sphere(position, radius);
    Vector3 heightVec(0, height, 0);
    Vector3 offsetXVec(radius, 0, 0);
    Vector3 offsetZVec(0, 0, radius);

    for (float i = 0.0f; i < 360.0f; i += 45.0f)
    {
        Vector3 p1 = sphere.Point(i, 90.0f);
        Vector3 p2 = sphere.Point(i + 45.0f, 90.0f);
        AddLine(p1, p2, color, depthTest);
        AddLine(p1 + heightVec, p2 + heightVec, color, depthTest);
    }
    
    AddLine(position + offsetXVec, position + heightVec + offsetXVec, color, depthTest);
    AddLine(position - offsetXVec, position + heightVec - offsetXVec, color, depthTest);
    AddLine(position + offsetZVec, position + heightVec + offsetZVec, color, depthTest);
    AddLine(position - offsetZVec, position + heightVec - offsetZVec, color, depthTest);
}

void DebugRenderer::Render()
{
    ZoneScoped;

    // Early-out if no geometry to render or shader failed to load
    if (!vertices.size() || !shaderProgram)
        return;

    if (vertexBuffer->NumVertices() < vertices.size())
        vertexBuffer->Define(USAGE_DYNAMIC, vertices.size(), vertexElements);

    if (vertices.size())
        vertexBuffer->SetData(0, vertices.size(), &vertices[0]);

    size_t totalIndices = indices.size() + noDepthIndices.size();
    
    if (indexBuffer->NumIndices() < totalIndices)
        indexBuffer->Define(USAGE_DYNAMIC, totalIndices, sizeof(unsigned));

    if (indices.size())
        indexBuffer->SetData(0, indices.size(), &indices[0]);
    if (noDepthIndices.size())
        indexBuffer->SetData(indices.size(), noDepthIndices.size(), &noDepthIndices[0]);

    Graphics* graphics = Subsystem<Graphics>();
    shaderProgram->Bind();
    graphics->SetUniform(shaderProgram, "viewProjMatrix", projection * view);
    graphics->SetVertexBuffer(vertexBuffer, shaderProgram);
    graphics->SetIndexBuffer(indexBuffer);

    if (indices.size())
    {
        graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_LESS, true, false);
        graphics->DrawIndexed(PT_LINE_LIST, 0, indices.size());
    }

    if (noDepthIndices.size())
    {
        graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
        graphics->DrawIndexed(PT_LINE_LIST, indices.size(), noDepthIndices.size());
    }

    vertices.clear();
    indices.clear();
    noDepthIndices.clear();
}
