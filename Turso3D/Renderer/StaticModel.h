// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "GeometryNode.h"

class Model;

/// %Scene node that renders an unanimated model, which can have LOD levels.
class StaticModel : public GeometryNode
{
    OBJECT(StaticModel);

public:
    /// Construct.
    StaticModel();
    /// Destruct.
    ~StaticModel();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Prepare object for rendering. Reset framenumber and calculate distance from camera, and check for LOD level changes. Called by Renderer in worker threads. Return false if should not render.
    bool OnPrepareRender(unsigned short frameNumber, Camera* camera) override;
    /// Perform ray test on self and add possible hit to the result vector.
    void OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance) override;

    /// Set the model resource.
    void SetModel(Model* model);
    /// Set LOD bias. Values higher than 1 use higher quality LOD (acts if distance is smaller.)
    void SetLodBias(float bias);

    /// Return the model resource.
    Model* GetModel() const;
    /// Return LOD bias.
    float LodBias() const { return lodBias; }

protected:
    /// Recalculate the world space bounding box.
    void OnWorldBoundingBoxUpdate() const override;
    /// Set model attribute. Used in serialization.
    void SetModelAttr(const ResourceRef& value);
    /// Return model attribute. Used in serialization.
    ResourceRef ModelAttr() const;

    /// Current model resource.
    SharedPtr<Model> model;
    /// LOD bias value.
    float lodBias;
};
