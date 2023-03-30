// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "GeometryNode.h"

class Model;

/// %Occluder drawable.
class OccluderDrawable : public GeometryDrawable
{
    friend class Occluder;

public:
    /// Recalculate the world space bounding box.
    void OnWorldBoundingBoxUpdate() const override;
    /// Add debug geometry to be rendered.
    void OnRenderDebug(DebugRenderer* debug) override;

protected:
    /// Current model resource.
    SharedPtr<Model> model;
};

/// %Scene node that is software rendered for occlusion culling. Similar to Occluder.
class Occluder : public OctreeNodeBase
{
    OBJECT(Occluder);

public:
    /// Construct.
    Occluder();
    /// Destruct.
    ~Occluder();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Set the model resource.
    void SetModel(Model* model);
    /// Set max distance for rendering. 0 is unlimited.
    void SetMaxDistance(float distance);

    /// Return the model resource.
    Model* GetModel() const;
    /// Return max distance for rendering, or 0 for unlimited.
    float MaxDistance() const { return drawable->MaxDistance(); }

protected:
    /// Search for an octree from the scene root and add self to it.
    void OnSceneSet(Scene* newScene, Scene* oldScene) override;
    /// Handle the transform matrix changing. Reinsert the occluder drawable.
    void OnTransformChanged() override;
    /// Handle the bounding box changing. Only queue octree reinsertion, does not dirty the node hierarchy.
    void OnBoundingBoxChanged();
    /// Handle the enabled status changing.
    void OnEnabledChanged(bool newEnabled) override;
    /// Remove from the current octree.
    void RemoveFromOctree();

    /// Set model attribute. Used in serialization.
    void SetModelAttr(const ResourceRef& value);
    /// Return model attribute. Used in serialization.
    ResourceRef ModelAttr() const;
};
