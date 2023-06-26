// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/JSONValue.h"
#include "Octree.h"
#include "StaticModel.h"

class AnimatedModel;
class AnimatedModelDrawable;
class Animation;
class AnimationState;
class UniformBuffer;

static const unsigned char AMF_ANIMATION_ORDER_DIRTY = 0x1;
static const unsigned char AMF_ANIMATION_DIRTY = 0x2;
static const unsigned char AMF_SKINNING_DIRTY = 0x4;
static const unsigned char AMF_SKINNING_BUFFER_DIRTY = 0x8;
static const unsigned char AMF_BONE_BOUNDING_BOX_DIRTY = 0x10;
static const unsigned char AMF_IN_ANIMATION_UPDATE = 0x20;

/// %Bone scene node for AnimatedModel skinning.
class Bone : public SpatialNode
{
    friend class AnimatedModelDrawable;

    OBJECT(Bone);

public:
    /// Construct.
    Bone();
    /// Destruct.
    ~Bone();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Set the animated model drawable associated with. When the bone moves, the model's skinning is dirtied.
    void SetDrawable(AnimatedModelDrawable* drawable);
    /// Set animation enabled. Default is enabled, when disabled the bone can be programmatically controlled.
    void SetAnimationEnabled(bool enable);
    /// Count number of child bones. Called by AnimatedModel once the skeleton has been fully created.
    void CountChildBones();

    /// Set bone parent space transform without dirtying the hierarchy.
    void SetTransformSilent(const Vector3& position_, const Quaternion& rotation_, const Vector3& scale_)
    {
        position = position_;
        rotation = rotation_;
        scale = scale_;
    }

    /// Return the animated model drawable.
    AnimatedModelDrawable* GetDrawable() const { return drawable; }
    /// Return whether animation is enabled.
    bool AnimationEnabled() const { return animationEnabled; }
    /// Return amount of child bones. This is used to check whether bone has attached objects and its dirtying cannot be handled in an optimized way.
    size_t NumChildBones() const { return numChildBones; }

protected:
    /// Handle the transform matrix changing.
    void OnTransformChanged() override;

private:
    /// Animated model drawable associated with.
    AnimatedModelDrawable* drawable;
    /// Animation enabled flag.
    bool animationEnabled;
    /// Amount of child bones.
    size_t numChildBones;
};

/// Animated model drawable.
class AnimatedModelDrawable : public StaticModelDrawable
{
    friend class AnimatedModel;

public:
    /// Construct.
    AnimatedModelDrawable();

    /// Recalculate the world space bounding box.
    void OnWorldBoundingBoxUpdate() const override;
    /// Do animation processing before octree reinsertion, if should update without regard to visibility. Called by Octree in worker threads. Must be opted-in by setting NF_OCTREE_UPDATE_CALL flag.
    void OnOctreeUpdate(unsigned short frameNumber) override;
    /// Prepare object for rendering. Reset framenumber and calculate distance from camera, check for LOD level changes, and update animation / skinning if necessary. Called by Renderer in worker threads. Return false if should not render.
    bool OnPrepareRender(unsigned short frameNumber, Camera* camera) override;
    /// Update GPU resources and set uniforms for rendering. Called by Renderer when geometry type is not static.
    void OnRender(ShaderProgram* program, size_t geomIndex) override;
    /// Perform ray test on self and add possible hit to the result vector.
    void OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance) override;
    /// Add debug geometry to be rendered.
    void OnRenderDebug(DebugRenderer* debug) override;

    /// Set bounding box and skinning dirty and queue octree reinsertion when any of the bones move.
    void OnBoneTransformChanged()
    {
        SetFlag(DF_BOUNDING_BOX_DIRTY, true);
        if (octree && octant && !TestFlag(DF_OCTREE_REINSERT_QUEUED))
            octree->QueueUpdate(this);

        animatedModelFlags |= AMF_SKINNING_DIRTY | AMF_BONE_BOUNDING_BOX_DIRTY;
    }

    /// Set animation order dirty when animation state changes layer order and queue octree reinsertion. Note: bounding box will only be dirtied once animation actually updates.
    void OnAnimationOrderChanged()
    {
        if (octree && octant && !TestFlag(DF_OCTREE_REINSERT_QUEUED))
            octree->QueueUpdate(this);

        animatedModelFlags |= AMF_ANIMATION_DIRTY | AMF_ANIMATION_ORDER_DIRTY;
    }

    /// Set animation dirty when animation state changes time position or weight and queue octree reinsertion. Note: bounding box will only be dirtied once animation actually updates.
    void OnAnimationChanged()
    {
        if (octree && octant && !TestFlag(DF_OCTREE_REINSERT_QUEUED))
            octree->QueueUpdate(this);

        animatedModelFlags |= AMF_ANIMATION_DIRTY;
    }

    /// Mark bone transforms dirty. Do in an optimized manner if bone has no attached objects.
    void SetBoneTransformsDirty();
    /// Apply animation states and recalculate bounding box.
    void UpdateAnimation();
    /// Update skin matrices for rendering.
    void UpdateSkinning();
    /// Create bone scene nodes based on the model. If compatible bones already exist in the scene hierarchy, they are taken into use instead of creating new.
    void CreateBones();
    /// Remove existing bones.
    void RemoveBones();
    
    /// Return the root bone.
    Bone* RootBone() const { return rootBone; }
    /// Return number of bones.
    size_t NumBones() const { return numBones; }
    /// Return all bone scene nodes.
    const AutoArrayPtr<Bone*>& Bones() const { return bones; }
    /// Return all animation states.
    const std::vector<SharedPtr<AnimationState> >& AnimationStates() const { return animationStates; }
    /// Return the internal dirty status flags.
    unsigned char AnimatedModelFlags() { return animatedModelFlags; }

protected:
    /// Combined bounding box of the bones in model space, used for quick updates when only the node moves without animation
    mutable BoundingBox boneBoundingBox;
    /// Internal dirty status flags.
    mutable unsigned char animatedModelFlags;
    /// Number of bones.
    unsigned short numBones;
    /// %Octree.
    Octree* octree;
    /// Root bone.
    Bone* rootBone;
    /// Bone scene nodes.
    AutoArrayPtr<Bone*> bones;
    /// Skinning matrices.
    AutoArrayPtr<Matrix3x4> skinMatrices;
    /// Skinning uniform buffer.
    AutoPtr<UniformBuffer> skinMatrixBuffer;
    /// Animation states.
    std::vector<SharedPtr<AnimationState> > animationStates;
};

/// %Scene node that renders a skeletally animated (skinned) model.
class AnimatedModel : public StaticModel
{
    OBJECT(AnimatedModel);

public:
    /// Construct.
    AnimatedModel();
    /// Destruct.
    ~AnimatedModel();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Set the model resource and create / acquire bone scene nodes.
    void SetModel(Model* model);
    /// Add an animation and return the created animation state.
    AnimationState* AddAnimationState(Animation* animation);
    /// Remove an animation by animation pointer.
    void RemoveAnimationState(Animation* animation);
    /// Remove an animation by animation name.
    void RemoveAnimationState(const std::string& animationName);
    /// Remove an animation by animation name.
    void RemoveAnimationState(const char* animationName);
    /// Remove an animation by animation name hash.
    void RemoveAnimationState(StringHash animationNameHash);
    /// Remove an animation by AnimationState pointer.
    void RemoveAnimationState(AnimationState* state);
    /// Remove an animation by state index.
    void RemoveAnimationState(size_t index);
    /// Remove all animations.
    void RemoveAllAnimationStates();

    /// Return the root bone.
    Bone* RootBone() const { return static_cast<AnimatedModelDrawable*>(drawable)->RootBone(); }
    /// Return number of bones.
    size_t NumBones() const { return static_cast<AnimatedModelDrawable*>(drawable)->NumBones(); }
    /// Return all bone scene nodes.
    const AutoArrayPtr<Bone*>& Bones() const { return static_cast<AnimatedModelDrawable*>(drawable)->Bones(); }
    /// Return all animation states.
    const std::vector<SharedPtr<AnimationState> >& AnimationStates() const { return static_cast<AnimatedModelDrawable*>(drawable)->AnimationStates(); }
    /// Return number of animation states.
    size_t NumAnimationStates() const { return static_cast<AnimatedModelDrawable*>(drawable)->animationStates.size(); }
    /// Return animation state by index.
    AnimationState* GetAnimationState(size_t index) const;
    /// Return animation state by animation pointer.
    AnimationState* FindAnimationState(Animation* animation) const;
    /// Return animation state by animation name.
    AnimationState* FindAnimationState(const std::string& animationName) const;
    /// Return animation state by animation name.
    AnimationState* FindAnimationState(const char* animationName) const;
    /// Return animation state by animation name hash.
    AnimationState* FindAnimationState(StringHash animationNameHash) const;

protected:
    /// Search for an octree from the scene root and add self to it.
    void OnSceneSet(Scene* newScene, Scene* oldScene) override;
    /// Handle the transform matrix changing. Queue octree reinsertion and skinning update for the drawable.
    void OnTransformChanged() override;

private:
    /// Set model attribute. Used in serialization.
    void SetModelAttr(const ResourceRef& value);
    /// Return model attribute. Used in serialization.
    ResourceRef ModelAttr() const;
    /// Set animation states attribute. Used in serialization.
    void SetAnimationStatesAttr(const JSONValue& value);
    /// Return animation states attribute. Used in serialization.
    JSONValue AnimationStatesAttr() const;
};
