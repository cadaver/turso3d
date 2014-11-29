// For conditions of distribution and use, see copyright notice in License.txt

#include "Frustum.h"
#include "Polyhedron.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

Polyhedron::Polyhedron()
{
}

Polyhedron::Polyhedron(const Polyhedron& polyhedron) :
    faces(polyhedron.faces)
{
}

Polyhedron::Polyhedron(const Vector<Vector<Vector3> >& faces) :
    faces(faces)
{
}

Polyhedron::Polyhedron(const BoundingBox& box)
{
    Define(box);
}

Polyhedron::Polyhedron(const Frustum& frustum)
{
    Define(frustum);
}

Polyhedron::~Polyhedron()
{
}

void Polyhedron::Define(const BoundingBox& box)
{
    Vector3 vertices[8];
    vertices[0] = box.min;
    vertices[1] = Vector3(box.max.x, box.min.y, box.min.z);
    vertices[2] = Vector3(box.min.x, box.max.y, box.min.z);
    vertices[3] = Vector3(box.max.x, box.max.y, box.min.z);
    vertices[4] = Vector3(box.min.x, box.min.y, box.max.z);
    vertices[5] = Vector3(box.max.x, box.min.y, box.max.z);
    vertices[6] = Vector3(box.min.x, box.max.y, box.max.z);
    vertices[7] = box.max;
    
    faces.Resize(6);
    SetFace(0, vertices[3], vertices[7], vertices[5], vertices[1]);
    SetFace(1, vertices[6], vertices[2], vertices[0], vertices[4]);
    SetFace(2, vertices[6], vertices[7], vertices[3], vertices[2]);
    SetFace(3, vertices[1], vertices[5], vertices[4], vertices[0]);
    SetFace(4, vertices[7], vertices[6], vertices[4], vertices[5]);
    SetFace(5, vertices[2], vertices[3], vertices[1], vertices[0]);
}

void Polyhedron::Define(const Frustum& frustum)
{
    const Vector3* vertices = frustum.vertices;
    
    faces.Resize(6);
    SetFace(0, vertices[0], vertices[4], vertices[5], vertices[1]);
    SetFace(1, vertices[7], vertices[3], vertices[2], vertices[6]);
    SetFace(2, vertices[7], vertices[4], vertices[0], vertices[3]);
    SetFace(3, vertices[1], vertices[5], vertices[6], vertices[2]);
    SetFace(4, vertices[4], vertices[7], vertices[6], vertices[5]);
    SetFace(5, vertices[3], vertices[0], vertices[1], vertices[2]);
}

void Polyhedron::AddFace(const Vector3& v0, const Vector3& v1, const Vector3& v2)
{
    faces.Resize(faces.Size() + 1);
    Vector<Vector3>& face = faces[faces.Size() - 1];
    face.Resize(3);
    face[0] = v0;
    face[1] = v1;
    face[2] = v2;
}

void Polyhedron::AddFace(const Vector3& v0, const Vector3& v1, const Vector3& v2, const Vector3& v3)
{
    faces.Resize(faces.Size() + 1);
    Vector<Vector3>& face = faces[faces.Size() - 1];
    face.Resize(4);
    face[0] = v0;
    face[1] = v1;
    face[2] = v2;
    face[3] = v3;
}

void Polyhedron::AddFace(const Vector<Vector3>& face)
{
    faces.Push(face);
}

void Polyhedron::Clip(const Plane& plane, Vector<Vector3>& clippedVertices, Vector<Vector3>& outFace)
{
    clippedVertices.Clear();
    
    for (size_t i = 0; i < faces.Size(); ++i)
    {
        Vector<Vector3>& face = faces[i];
        Vector3 lastVertex;
        float lastDistance = 0.0f;
        
        outFace.Clear();
        
        for (size_t j = 0; j < face.Size(); ++j)
        {
            float distance = plane.Distance(face[j]);
            if (distance >= 0.0f)
            {
                if (lastDistance < 0.0f)
                {
                    float t = lastDistance / (lastDistance - distance);
                    Vector3 clippedVertex = lastVertex + t * (face[j] - lastVertex);
                    outFace.Push(clippedVertex);
                    clippedVertices.Push(clippedVertex);
                }
                
                outFace.Push(face[j]);
            }
            else
            {
                if (lastDistance >= 0.0f && j != 0)
                {
                    float t = lastDistance / (lastDistance - distance);
                    Vector3 clippedVertex = lastVertex + t * (face[j] - lastVertex);
                    outFace.Push(clippedVertex);
                    clippedVertices.Push(clippedVertex);
                }
            }
            
            lastVertex = face[j];
            lastDistance = distance;
        }
        
        // Recheck the distances of the last and first vertices and add the final clipped vertex if applicable
        float distance = plane.Distance(face[0]);
        if ((lastDistance < 0.0f && distance >= 0.0f) || (lastDistance >= 0.0f && distance < 0.0f))
        {
            float t = lastDistance / (lastDistance - distance);
            Vector3 clippedVertex = lastVertex + t * (face[0] - lastVertex);
            outFace.Push(clippedVertex);
            clippedVertices.Push(clippedVertex);
        }
        
        // Do not keep faces which are less than triangles
        if (outFace.Size() < 3)
            outFace.Clear();
        
        face = outFace;
    }
    
    // Remove empty faces
    for (size_t i = faces.Size() - 1; i < faces.Size(); --i)
    {
        if (faces[i].IsEmpty())
            faces.Erase(i);
    }
    
    // Create a new face from the clipped vertices. First remove duplicates
    for (size_t i = 0; i < clippedVertices.Size(); ++i)
    {
        for (size_t j = clippedVertices.Size() - 1; j > i; --j)
        {
            if (clippedVertices[j].Equals(clippedVertices[i]))
                clippedVertices.Erase(j);
        }
    }
    
    if (clippedVertices.Size() > 3)
    {
        outFace.Clear();
        
        // Start with the first vertex
        outFace.Push(clippedVertices.Front());
        clippedVertices.Erase(0);
        
        while (!clippedVertices.IsEmpty())
        {
            // Then add the vertex which is closest to the last added
            const Vector3& lastAdded = outFace.Back();
            float bestDistance = M_INFINITY;
            size_t bestIndex = 0;
            
            for (size_t i = 0; i < clippedVertices.Size(); ++i)
            {
                float distance = (clippedVertices[i] - lastAdded).LengthSquared();
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestIndex = i;
                }
            }
            
            outFace.Push(clippedVertices[bestIndex]);
            clippedVertices.Erase(bestIndex);
        }
        
        faces.Push(outFace);
    }
}

void Polyhedron::Clip(const Plane& plane)
{
    Vector<Vector3> clippedVertices;
    Vector<Vector3> outFace;
    
    Clip(plane, clippedVertices, outFace);
}

void Polyhedron::Clip(const Frustum& frustum)
{
    Vector<Vector3> clippedVertices;
    Vector<Vector3> outFace;
    
    for (size_t i = 0; i < NUM_FRUSTUM_PLANES; ++i)
        Clip(frustum.planes[i], clippedVertices, outFace);
}

void Polyhedron::Clip(const BoundingBox& box)
{
    Vector<Vector3> clippedVertices;
    Vector<Vector3> outFace;
    
    Vector3 vertices[8];
    vertices[0] = box.min;
    vertices[1] = Vector3(box.max.x, box.min.y, box.min.z);
    vertices[2] = Vector3(box.min.x, box.max.y, box.min.z);
    vertices[3] = Vector3(box.max.x, box.max.y, box.min.z);
    vertices[4] = Vector3(box.min.x, box.min.y, box.max.z);
    vertices[5] = Vector3(box.max.x, box.min.y, box.max.z);
    vertices[6] = Vector3(box.min.x, box.max.y, box.max.z);
    vertices[7] = box.max;
    
    Clip(Plane(vertices[5], vertices[7], vertices[3]), clippedVertices, outFace);
    Clip(Plane(vertices[0], vertices[2], vertices[6]), clippedVertices, outFace);
    Clip(Plane(vertices[3], vertices[7], vertices[6]), clippedVertices, outFace);
    Clip(Plane(vertices[4], vertices[5], vertices[1]), clippedVertices, outFace);
    Clip(Plane(vertices[4], vertices[6], vertices[7]), clippedVertices, outFace);
    Clip(Plane(vertices[1], vertices[3], vertices[2]), clippedVertices, outFace);
}

void Polyhedron::Clear()
{
    faces.Clear();
}

void Polyhedron::Transform(const Matrix3& transform)
{
    for (size_t i = 0; i < faces.Size(); ++i)
    {
        Vector<Vector3>& face = faces[i];
        for (size_t j = 0; j < face.Size(); ++j)
            face[j] = transform * face[j];
    }
}

void Polyhedron::Transform(const Matrix3x4& transform)
{
    for (size_t i = 0; i < faces.Size(); ++i)
    {
        Vector<Vector3>& face = faces[i];
        for (size_t j = 0; j < face.Size(); ++j)
            face[j] = transform * face[j];
    }
}

Polyhedron Polyhedron::Transformed(const Matrix3& transform) const
{
    Polyhedron ret;
    ret.faces.Resize(faces.Size());
    
    for (size_t i = 0; i < faces.Size(); ++i)
    {
        const Vector<Vector3>& face = faces[i];
        Vector<Vector3>& newFace = ret.faces[i];
        newFace.Resize(face.Size());
        
        for (size_t j = 0; j < face.Size(); ++j)
            newFace[j] = transform * face[j];
    }
    
    return ret;
}

Polyhedron Polyhedron::Transformed(const Matrix3x4& transform) const
{
    Polyhedron ret;
    ret.faces.Resize(faces.Size());
    
    for (size_t i = 0; i < faces.Size(); ++i)
    {
        const Vector<Vector3>& face = faces[i];
        Vector<Vector3>& newFace = ret.faces[i];
        newFace.Resize(face.Size());
        
        for (size_t j = 0; j < face.Size(); ++j)
            newFace[j] = transform * face[j];
    }
    
    return ret;
}

void Polyhedron::SetFace(size_t index, const Vector3& v0, const Vector3& v1, const Vector3& v2)
{
    Vector<Vector3>& face = faces[index];
    face.Resize(3);
    face[0] = v0;
    face[1] = v1;
    face[2] = v2;
}

void Polyhedron::SetFace(size_t index, const Vector3& v0, const Vector3& v1, const Vector3& v2, const Vector3& v3)
{
    Vector<Vector3>& face = faces[index];
    face.Resize(4);
    face[0] = v0;
    face[1] = v1;
    face[2] = v2;
    face[3] = v3;
}

}
