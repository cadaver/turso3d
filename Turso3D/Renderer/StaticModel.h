// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "GeometryNode.h"

/// %Scene node that renders an unanimated model.
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

    /// Prepare object for rendering. Reset framenumber and light list and calculate distance from camera, and check for LOD level changes. Called by Renderer. Return false if should not render.
    bool OnPrepareRender(unsigned short frameNumber, Camera* camera) override;

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

private:
    /// Set model attribute. Used in serialization.
    void SetModelAttr(const ResourceRef& model);
    /// Return model attribute. Used in serialization.
    ResourceRef ModelAttr() const;

    /// LOD bias value.
    float lodBias;
    /// Current model resource.
    SharedPtr<Model> model;
};
