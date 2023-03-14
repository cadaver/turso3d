// For conditions of distribution and use, see copyright notice in License.txt

#include "../Graphics/Graphics.h"
#include "../Graphics/ShaderProgram.h"
#include "../Graphics/VertexBuffer.h"
#include "../Math/Polyhedron.h"
#include "Camera.h"
#include "DebugRenderer.h"

#include <glew.h>
#include <tracy/Tracy.hpp>

DebugRenderer::DebugRenderer()
{
    RegisterSubsystem(this);
    
    vertexBuffer = new VertexBuffer();
    vertexElements.push_back(VertexElement(ELEM_VECTOR3, SEM_POSITION));
    vertexElements.push_back(VertexElement(ELEM_UBYTE4, SEM_COLOR));
}

DebugRenderer::~DebugRenderer()
{
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
    std::vector<DebugVertex>& dest = depthTest ? vertices : noDepthVertices;
    dest.push_back(DebugVertex(start, color));
    dest.push_back(DebugVertex(end, color));
}

void DebugRenderer::AddBoundingBox(const BoundingBox& box, const Color& color, bool depthTest)
{
    const Vector3& min = box.min;
    const Vector3& max = box.max;

    Vector3 v1(max.x, min.y, min.z);
    Vector3 v2(max.x, max.y, min.z);
    Vector3 v3(min.x, max.y, min.z);
    Vector3 v4(min.x, min.y, max.z);
    Vector3 v5(max.x, min.y, max.z);
    Vector3 v6(min.x, max.y, max.z);

    unsigned uintColor = color.ToUInt();

    AddLine(min, v1, uintColor, depthTest);
    AddLine(v1, v2, uintColor, depthTest);
    AddLine(v2, v3, uintColor, depthTest);
    AddLine(v3, min, uintColor, depthTest);
    AddLine(v4, v5, uintColor, depthTest);
    AddLine(v5, max, uintColor, depthTest);
    AddLine(max, v6, uintColor, depthTest);
    AddLine(v6, v4, uintColor, depthTest);
    AddLine(min, v4, uintColor, depthTest);
    AddLine(v1, v5, uintColor, depthTest);
    AddLine(v2, max, uintColor, depthTest);
    AddLine(v3, v6, uintColor, depthTest);
}

void DebugRenderer::AddBoundingBox(const BoundingBox& box, const Matrix3x4& transform, const Color& color, bool depthTest)
{
    const Vector3& min = box.min;
    const Vector3& max = box.max;

    Vector3 v0(transform * min);
    Vector3 v1(transform * Vector3(max.x, min.y, min.z));
    Vector3 v2(transform * Vector3(max.x, max.y, min.z));
    Vector3 v3(transform * Vector3(min.x, max.y, min.z));
    Vector3 v4(transform * Vector3(min.x, min.y, max.z));
    Vector3 v5(transform * Vector3(max.x, min.y, max.z));
    Vector3 v6(transform * Vector3(min.x, max.y, max.z));
    Vector3 v7(transform * max);

    unsigned uintColor = color.ToUInt();

    AddLine(v0, v1, uintColor, depthTest);
    AddLine(v1, v2, uintColor, depthTest);
    AddLine(v2, v3, uintColor, depthTest);
    AddLine(v3, v0, uintColor, depthTest);
    AddLine(v4, v5, uintColor, depthTest);
    AddLine(v5, v7, uintColor, depthTest);
    AddLine(v7, v6, uintColor, depthTest);
    AddLine(v6, v4, uintColor, depthTest);
    AddLine(v0, v4, uintColor, depthTest);
    AddLine(v1, v5, uintColor, depthTest);
    AddLine(v2, v7, uintColor, depthTest);
    AddLine(v3, v6, uintColor, depthTest);
}

void DebugRenderer::AddFrustum(const Frustum& frustum_, const Color& color, bool depthTest)
{
    unsigned uintColor = color.ToUInt();

    AddLine(frustum_.vertices[0], frustum_.vertices[1], uintColor, depthTest);
    AddLine(frustum_.vertices[1], frustum_.vertices[2], uintColor, depthTest);
    AddLine(frustum_.vertices[2], frustum_.vertices[3], uintColor, depthTest);
    AddLine(frustum_.vertices[3], frustum_.vertices[0], uintColor, depthTest);
    AddLine(frustum_.vertices[4], frustum_.vertices[5], uintColor, depthTest);
    AddLine(frustum_.vertices[5], frustum_.vertices[6], uintColor, depthTest);
    AddLine(frustum_.vertices[6], frustum_.vertices[7], uintColor, depthTest);
    AddLine(frustum_.vertices[7], frustum_.vertices[4], uintColor, depthTest);
    AddLine(frustum_.vertices[0], frustum_.vertices[4], uintColor, depthTest);
    AddLine(frustum_.vertices[1], frustum_.vertices[5], uintColor, depthTest);
    AddLine(frustum_.vertices[2], frustum_.vertices[6], uintColor, depthTest);
    AddLine(frustum_.vertices[3], frustum_.vertices[7], uintColor, depthTest);
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
            Vector3 p1 = sphere.Point(i, j);
            Vector3 p2 = sphere.Point(i + 45.0f, j);
            Vector3 p3 = sphere.Point(i, j + 45.0f);
            Vector3 p4 = sphere.Point(i + 45.0f, j + 45.0f);

            AddLine(p1, p2, uintColor, depthTest);
            AddLine(p3, p4, uintColor, depthTest);
            AddLine(p1, p3, uintColor, depthTest);
            AddLine(p2, p4, uintColor, depthTest);
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

    size_t totalVertices = vertices.size() + noDepthVertices.size();
    if (!totalVertices)
        return;

    if (vertexBuffer->NumVertices() < totalVertices)
        vertexBuffer->Define(USAGE_DYNAMIC, totalVertices, vertexElements);

    if (vertices.size())
        vertexBuffer->SetData(0, vertices.size(), &vertices[0]);
    if (noDepthVertices.size())
        vertexBuffer->SetData(vertices.size(), noDepthVertices.size(), &noDepthVertices[0]);

    Graphics* graphics = Subsystem<Graphics>();
    ShaderProgram* program = graphics->SetProgram("Shaders/DebugLines.glsl");
    graphics->SetUniform(program, "viewProjMatrix", projection * view);
    graphics->SetVertexBuffer(vertexBuffer, program);
    
    if (vertices.size())
    {
        graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_LESS, true, false);
        glDrawArrays(GL_LINES, 0, (GLsizei)vertices.size());
    }

    if (noDepthVertices.size())
    {
        graphics->SetRenderState(BLEND_REPLACE, CULL_NONE, CMP_ALWAYS, true, false);
        glDrawArrays(GL_LINES, (GLint)vertices.size(), (GLsizei)noDepthVertices.size());
    }

    vertices.clear();
    noDepthVertices.clear();
}
