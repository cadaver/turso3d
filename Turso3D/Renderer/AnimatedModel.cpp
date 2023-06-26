// For conditions of distribution and use, see copyright notice in License.txt

#include "../Graphics/GraphicsDefs.h"
#include "../Graphics/UniformBuffer.h"
#include "../IO/Log.h"
#include "../Math/Ray.h"
#include "../Resource/ResourceCache.h"
#include "AnimatedModel.h"
#include "Animation.h"
#include "AnimationState.h"
#include "DebugRenderer.h"
#include "Model.h"
#include "Octree.h"

#include <algorithm>
#include <tracy/Tracy.hpp>

static Allocator<AnimatedModelDrawable> drawableAllocator;

Bone::Bone() :
    drawable(nullptr),
    animationEnabled(true),
    numChildBones(0)
{
}

Bone::~Bone()
{
}

void Bone::RegisterObject()
{
    RegisterFactory<Bone>();
    CopyBaseAttributes<Bone, SpatialNode>();
    RegisterDerivedType<Bone, SpatialNode>();
    RegisterAttribute("animationEnabled", &Bone::AnimationEnabled, &Bone::SetAnimationEnabled);
}

void Bone::SetDrawable(AnimatedModelDrawable* drawable_)
{
    drawable = drawable_;
}

void Bone::SetAnimationEnabled(bool enable)
{
    animationEnabled = enable;
}

void Bone::CountChildBones()
{
    numChildBones = 0;

    for (auto it = children.begin(); it != children.end(); ++it)
    {
        if ((*it)->Type() == Bone::TypeStatic())
            ++numChildBones;
    }
}

void Bone::OnTransformChanged()
{
    SpatialNode::OnTransformChanged();

    // Avoid duplicate dirtying calls if model's skinning is already dirty. Do not signal changes either during animation update,
    // as the model will set the hierarchy dirty when finished. This is also used to optimize when only the model node moves.
    if (drawable && !(drawable->AnimatedModelFlags() & (AMF_IN_ANIMATION_UPDATE | AMF_SKINNING_DIRTY)))
        drawable->OnBoneTransformChanged();
}

static inline bool CompareAnimationStates(const SharedPtr<AnimationState>& lhs, const SharedPtr<AnimationState>& rhs)
{
    return lhs->BlendLayer() < rhs->BlendLayer();
}

AnimatedModelDrawable::AnimatedModelDrawable() :
    animatedModelFlags(0),
    numBones(0),
    octree(nullptr),
    rootBone(nullptr)
{
    SetFlag(DF_SKINNED_GEOMETRY | DF_OCTREE_UPDATE_CALL, true);
}

void AnimatedModelDrawable::OnWorldBoundingBoxUpdate() const
{
    if (model && numBones)
    {
        // Recalculate bounding box from bones only if they moved individually
        if (animatedModelFlags & AMF_BONE_BOUNDING_BOX_DIRTY)
        {
            const std::vector<ModelBone>& modelBones = model->Bones();

            // Use a temporary bounding box for calculations in case many threads call this simultaneously
            BoundingBox tempBox;

            for (size_t i = 0; i < numBones; ++i)
            {
                if (modelBones[i].active)
                    tempBox.Merge(modelBones[i].boundingBox.Transformed(bones[i]->WorldTransform()));
            }

            worldBoundingBox = tempBox;
            boneBoundingBox = tempBox.Transformed(WorldTransform().Inverse());
            animatedModelFlags &= ~AMF_BONE_BOUNDING_BOX_DIRTY;
        }
        else
            worldBoundingBox = boneBoundingBox.Transformed(WorldTransform());
    }
    else
        Drawable::OnWorldBoundingBoxUpdate();
}

void AnimatedModelDrawable::OnOctreeUpdate(unsigned short frameNumber)
{
    if (TestFlag(DF_UPDATE_INVISIBLE) || WasInView(frameNumber))
    {
        if (animatedModelFlags & AMF_ANIMATION_DIRTY)
            UpdateAnimation();

        if (animatedModelFlags & AMF_SKINNING_DIRTY)
            UpdateSkinning();
    }
}

bool AnimatedModelDrawable::OnPrepareRender(unsigned short frameNumber, Camera* camera)
{
    if (!StaticModelDrawable::OnPrepareRender(frameNumber, camera))
        return false;

    // Update animation here too if just came into view and animation / skinning is still dirty
    if (animatedModelFlags & AMF_ANIMATION_DIRTY)
        UpdateAnimation();

    if (animatedModelFlags & AMF_SKINNING_DIRTY)
        UpdateSkinning();

    return true;
}

void AnimatedModelDrawable::OnRender(ShaderProgram*, size_t)
{
    if (!skinMatrixBuffer || !numBones)
        return;

    if (animatedModelFlags & AMF_SKINNING_BUFFER_DIRTY)
    {
        skinMatrixBuffer->SetData(0, numBones * sizeof(Matrix3x4), skinMatrices);
        animatedModelFlags &= ~AMF_SKINNING_BUFFER_DIRTY;
    }

    skinMatrixBuffer->Bind(UB_OBJECTDATA);
}

void AnimatedModelDrawable::OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance_)
{
    if (ray.HitDistance(WorldBoundingBox()) < maxDistance_ && model)
    {
        RaycastResult res;
        res.distance = M_INFINITY;

        // Perform raycast against each bone in its local space
        const std::vector<ModelBone>& modelBones = model->Bones();

        for (size_t i = 0; i < numBones; ++i)
        {
            if (!modelBones[i].active)
                continue;

            const Matrix3x4& transform = bones[i]->WorldTransform();
            Ray localRay = ray.Transformed(transform.Inverse());
            float localDistance = localRay.HitDistance(modelBones[i].boundingBox);

            if (localDistance < M_INFINITY)
            {
                // If has a hit, transform it back to world space
                Vector3 hitPosition = transform * (localRay.origin + localDistance * localRay.direction);
                float hitDistance = (hitPosition - ray.origin).Length();

                if (hitDistance < maxDistance_ && hitDistance < res.distance)
                {
                    res.position = hitPosition;
                    /// \todo Hit normal not calculated correctly
                    res.normal = -ray.direction;
                    res.distance = hitDistance;
                    res.drawable = this;
                    res.subObject = i;
                }
            }
        }

        if (res.distance < maxDistance_)
            dest.push_back(res);
    }
}

void AnimatedModelDrawable::OnRenderDebug(DebugRenderer* debug)
{
    debug->AddBoundingBox(WorldBoundingBox(), Color::GREEN, false);

    for (size_t i = 0; i < numBones; ++i)
    {
        // Skip the root bone, as it has no sensible connection point
        Bone* bone = bones[i];
        if (bone != rootBone)
            debug->AddLine(bone->WorldPosition(), bone->SpatialParent()->WorldPosition(), Color::WHITE, false);
    }
}

void AnimatedModelDrawable::CreateBones()
{
    ZoneScoped;

    if (!model)
    {
        skinMatrixBuffer.Reset();
        RemoveBones();
        return;
    }

    const std::vector<ModelBone>& modelBones = model->Bones();
    if (numBones != modelBones.size())
        RemoveBones();

    numBones = (unsigned short)modelBones.size();

    bones = new Bone*[numBones];
    skinMatrices = new Matrix3x4[numBones];

    for (size_t i = 0; i < modelBones.size(); ++i)
    {
        const ModelBone& modelBone = modelBones[i];

        // Try to find existing bone from scene hierarchy, if not found create new
        bones[i] = owner->FindChild<Bone>(modelBone.nameHash, true);

        if (!bones[i])
        {
            bones[i] = Object::Create<Bone>();
            bones[i]->SetName(modelBone.name);
            bones[i]->SetTransform(modelBone.initialPosition, modelBone.initialRotation, modelBone.initialScale);
        }

        bones[i]->SetDrawable(this);
    }

    // Loop through bones again to set the correct parents
    for (size_t i = 0; i < modelBones.size(); ++i)
    {
        const ModelBone& desc = modelBones[i];
        if (desc.parentIndex == i)
        {
            bones[i]->SetParent(owner);
            rootBone = bones[i];
        }
        else
            bones[i]->SetParent(bones[desc.parentIndex]);
    }

    // Count child bones now for optimized transform dirtying
    for (size_t i = 0; i < modelBones.size(); ++i)
        bones[i]->CountChildBones();

    if (!skinMatrixBuffer)
        skinMatrixBuffer = new UniformBuffer();
    skinMatrixBuffer->Define(USAGE_DYNAMIC, numBones * sizeof(Matrix3x4));

    // Set initial bone bounding box recalculation and skinning dirty. Also calculate a valid bone bounding box immediately to ensure models can enter the view without updating animation first
    OnBoneTransformChanged();
    OnWorldBoundingBoxUpdate();
}

void AnimatedModelDrawable::UpdateAnimation()
{
    ZoneScoped;

    if (animatedModelFlags & AMF_ANIMATION_ORDER_DIRTY)
        std::sort(animationStates.begin(), animationStates.end(), CompareAnimationStates);

    animatedModelFlags |= AMF_IN_ANIMATION_UPDATE | AMF_BONE_BOUNDING_BOX_DIRTY;

    // Reset bones to initial pose, then apply animations
    const std::vector<ModelBone>& modelBones = model->Bones();

    for (size_t i = 0; i < numBones; ++i)
    {
        Bone* bone = bones[i];
        const ModelBone& modelBone = modelBones[i];
        if (bone->AnimationEnabled())
            bone->SetTransformSilent(modelBone.initialPosition, modelBone.initialRotation, modelBone.initialScale);
    }

    for (auto it = animationStates.begin(); it != animationStates.end(); ++it)
    {
        AnimationState* state = *it;
        if (state->Enabled())
            state->Apply();
    }

    // Dirty the bone hierarchy now. This will also dirty and queue reinsertion for attached models
    SetBoneTransformsDirty();

    animatedModelFlags &= ~(AMF_ANIMATION_ORDER_DIRTY | AMF_ANIMATION_DIRTY | AMF_IN_ANIMATION_UPDATE);

    // Update bounding box already here to take advantage of threaded update, and also to update bone world transforms for skinning
    OnWorldBoundingBoxUpdate();

    // If updating only when visible, queue octree reinsertion for next frame. This also ensures shadowmap rendering happens correctly
    // Else just dirty the skinning
    if (!TestFlag(DF_UPDATE_INVISIBLE))
    {
        if (octree && octant && !TestFlag(DF_OCTREE_REINSERT_QUEUED))
            octree->QueueUpdate(this);
    }

    animatedModelFlags |= AMF_SKINNING_DIRTY;
}

void AnimatedModelDrawable::UpdateSkinning()
{
    ZoneScoped;

    const std::vector<ModelBone>& modelBones = model->Bones();

    for (size_t i = 0; i < numBones; ++i)
        skinMatrices[i] = bones[i]->WorldTransform() * modelBones[i].offsetMatrix;

    animatedModelFlags &= ~AMF_SKINNING_DIRTY;
    animatedModelFlags |= AMF_SKINNING_BUFFER_DIRTY;
}

void AnimatedModelDrawable::SetBoneTransformsDirty()
{
    for (size_t i = 0; i < numBones; ++i)
    {
        // If bone has only other bones as children, just set its world transform dirty without going through the hierarchy. The whole hierarchy will be eventually updated
        if (bones[i]->NumChildren() == bones[i]->NumChildBones())
            bones[i]->SetFlag(NF_WORLD_TRANSFORM_DIRTY, true);
        else
            bones[i]->OnTransformChanged();
    }
}

void AnimatedModelDrawable::RemoveBones()
{
    if (!numBones)
        return;

    // Do not signal transform changes back to the model during deletion
    for (size_t i = 0; i < numBones; ++i)
        bones[i]->SetDrawable(nullptr);

    if (rootBone)
    {
        rootBone->RemoveSelf();
        rootBone = nullptr;
    }

    bones.Reset();
    skinMatrices.Reset();
    skinMatrixBuffer.Reset();
    numBones = 0;
}

AnimatedModel::AnimatedModel()
{
    drawable = drawableAllocator.Allocate();
    drawable->SetOwner(this);
}

AnimatedModel::~AnimatedModel()
{
    static_cast<AnimatedModelDrawable*>(drawable)->RemoveBones();
    RemoveFromOctree();
    drawableAllocator.Free(static_cast<AnimatedModelDrawable*>(drawable));
    drawable = nullptr;
}

void AnimatedModel::RegisterObject()
{
    RegisterFactory<AnimatedModel>();
    CopyBaseAttributes<AnimatedModel, OctreeNode>();
    RegisterDerivedType<AnimatedModel, StaticModel>();
    RegisterMixedRefAttribute("model", &AnimatedModel::ModelAttr, &AnimatedModel::SetModelAttr, ResourceRef(Model::TypeStatic()));
    CopyBaseAttribute<AnimatedModel, StaticModel>("materials");
    CopyBaseAttribute<AnimatedModel, StaticModel>("lodBias");
    RegisterMixedRefAttribute("animationStates", &AnimatedModel::AnimationStatesAttr, &AnimatedModel::SetAnimationStatesAttr);
}

void AnimatedModel::SetModel(Model* model_)
{
    ZoneScoped;

    StaticModel::SetModel(model_);
    static_cast<AnimatedModelDrawable*>(drawable)->CreateBones();
}

AnimationState* AnimatedModel::AddAnimationState(Animation* animation)
{
    AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

    if (!animation || !modelDrawable->numBones)
        return nullptr;

    // Check for not adding twice
    AnimationState* existing = FindAnimationState(animation);
    if (existing)
        return existing;

    SharedPtr<AnimationState> newState(new AnimationState(modelDrawable, animation));
    modelDrawable->animationStates.push_back(newState);
    modelDrawable->OnAnimationOrderChanged();

    return newState;
}

void AnimatedModel::RemoveAnimationState(Animation* animation)
{
    if (animation)
        RemoveAnimationState(animation->NameHash());
}

void AnimatedModel::RemoveAnimationState(const std::string& animationName)
{
    RemoveAnimationState(StringHash(animationName));
}

void AnimatedModel::RemoveAnimationState(const char* animationName)
{
    RemoveAnimationState(StringHash(animationName));
}

void AnimatedModel::RemoveAnimationState(StringHash animationNameHash)
{
    AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

    for (auto it = modelDrawable->animationStates.begin(); it != modelDrawable->animationStates.end(); ++it)
    {
        AnimationState* state = *it;
        Animation* animation = state->GetAnimation();
        // Check both the animation and the resource name
        if (animation->NameHash() == animationNameHash || animation->AnimationNameHash() == animationNameHash)
        {
            modelDrawable->animationStates.erase(it);
            modelDrawable->OnAnimationChanged();
            return;
        }
    }
}

void AnimatedModel::RemoveAnimationState(AnimationState* state)
{
    AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

    for (auto it = modelDrawable->animationStates.begin(); it != modelDrawable->animationStates.end(); ++it)
    {
        if (*it == state)
        {
            modelDrawable->animationStates.erase(it);
            modelDrawable->OnAnimationChanged();
            return;
        }
    }
}

void AnimatedModel::RemoveAnimationState(size_t index)
{
    AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

    if (index < modelDrawable->animationStates.size())
    {
        modelDrawable->animationStates.erase(modelDrawable->animationStates.begin() + index);
        modelDrawable->OnAnimationChanged();
    }
}

void AnimatedModel::RemoveAllAnimationStates()
{
    AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

    if (modelDrawable->animationStates.size())
    {
        modelDrawable->animationStates.clear();
        modelDrawable->OnAnimationChanged();
    }
}

AnimationState* AnimatedModel::FindAnimationState(Animation* animation) const
{
    AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

    for (auto it = modelDrawable->animationStates.begin(); it != modelDrawable->animationStates.end(); ++it)
    {
        if ((*it)->GetAnimation() == animation)
            return *it;
    }

    return nullptr;
}

AnimationState* AnimatedModel::FindAnimationState(const std::string& animationName) const
{
    return GetAnimationState(StringHash(animationName));
}

AnimationState* AnimatedModel::FindAnimationState(const char* animationName) const
{
    return GetAnimationState(StringHash(animationName));
}

AnimationState* AnimatedModel::FindAnimationState(StringHash animationNameHash) const
{
    AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

    for (auto it = modelDrawable->animationStates.begin(); it != modelDrawable->animationStates.end(); ++it)
    {
        Animation* animation = (*it)->GetAnimation();
        // Check both the animation and the resource name
        if (animation->NameHash() == animationNameHash || animation->AnimationNameHash() == animationNameHash)
            return *it;
    }

    return nullptr;
}

AnimationState* AnimatedModel::GetAnimationState(size_t index) const
{
    AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

    return index < modelDrawable->animationStates.size() ? modelDrawable->animationStates[index] : nullptr;
}

void AnimatedModel::OnSceneSet(Scene* newScene, Scene* oldScene)
{
    OctreeNode::OnSceneSet(newScene, oldScene);

    // Set octree also directly to the drawable so it can queue itself
    static_cast<AnimatedModelDrawable*>(drawable)->octree = octree;
}

void AnimatedModel::OnTransformChanged()
{
    AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);

    // To improve performance set skinning dirty now, so the bone nodes will not redundantly signal transform changes back
    modelDrawable->animatedModelFlags |= AMF_SKINNING_DIRTY;

    // If have other children than the root bone, dirty the hierarchy normally. Otherwise optimize
    if (children.size() > 1)
        SpatialNode::OnTransformChanged();
    else
    {
        modelDrawable->SetBoneTransformsDirty();
        modelDrawable->SetFlag(DF_WORLD_TRANSFORM_DIRTY, true);
        SetFlag(NF_WORLD_TRANSFORM_DIRTY, true);
    }

    modelDrawable->SetFlag(DF_BOUNDING_BOX_DIRTY, true);
    if (octree && modelDrawable->octant && !modelDrawable->TestFlag(DF_OCTREE_REINSERT_QUEUED))
        octree->QueueUpdate(modelDrawable);
}

void AnimatedModel::SetModelAttr(const ResourceRef& value)
{
    ResourceCache* cache = Subsystem<ResourceCache>();
    SetModel(cache->LoadResource<Model>(value.name));
}

ResourceRef AnimatedModel::ModelAttr() const
{
    return ResourceRef(Model::TypeStatic(), ResourceName(GetModel()));
}

void AnimatedModel::SetAnimationStatesAttr(const JSONValue& value)
{
    for (size_t i = 0; i < value.Size(); ++i)
    {
        const JSONValue& state = value[i];
        if (state.Size() < 6)
            continue;

        Animation* animation = Subsystem<ResourceCache>()->LoadResource<Animation>(state[0].GetString());
        if (!animation)
            continue;

        AnimationState* animState = AddAnimationState(animation);
        if (!animState)
            continue;

        animState->SetStartBone(FindChild<Bone>(state[1].GetString(), true));
        animState->SetLooped(state[2].GetBool());
        animState->SetWeight((float)state[3].GetNumber());
        animState->SetTime((float)state[4].GetNumber());
        animState->SetBlendLayer((unsigned char)(int)state[5].GetNumber());
    }
}

JSONValue AnimatedModel::AnimationStatesAttr() const
{
    AnimatedModelDrawable* modelDrawable = static_cast<AnimatedModelDrawable*>(drawable);
    JSONValue states;

    /// \todo Per-bone weights not serialized
    for (auto it = modelDrawable->animationStates.begin(); it != modelDrawable->animationStates.end(); ++it)
    {
        AnimationState* animState = *it;
        JSONValue state;
        state.Push(animState->GetAnimation() ? animState->GetAnimation()->Name() : "");
        state.Push(animState->StartBone() ? animState->StartBone()->Name() : "");
        state.Push(animState->Looped());
        state.Push(animState->Weight());
        state.Push(animState->Time());
        state.Push((int)animState->BlendLayer());
        states.Push(state);
    }

    return states;
}
