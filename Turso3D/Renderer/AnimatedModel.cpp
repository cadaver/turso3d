// For conditions of distribution and use, see copyright notice in License.txt

#include "../Graphics/GraphicsDefs.h"
#include "../Graphics/UniformBuffer.h"
#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Time/Profiler.h"
#include "AnimatedModel.h"
#include "Animation.h"
#include "AnimationState.h"
#include "Model.h"

#include <glew.h>
#include <algorithm>

Bone::Bone() :
    model(nullptr),
    animationEnabled(true)
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

void Bone::SetAnimatedModel(AnimatedModel* model_)
{
    model = model_;
}

void Bone::SetAnimationEnabled(bool enable)
{
    animationEnabled = enable;
}

void Bone::OnTransformChanged()
{
    SpatialNode::OnTransformChanged();

    // Avoid duplicate dirtying calls if model's skinning is already dirty. During animation update, do not signal changes either, as the model will set the hierarchy dirty when finished.
    if (model && !(model->AnimatedModelFlags() & (AMF_IN_ANIMATION_UPDATE | AMF_SKINNING_DIRTY)))
        model->OnBoneTransformChanged();
}

inline bool CompareAnimationStates(const SharedPtr<AnimationState>& lhs, const SharedPtr<AnimationState>& rhs)
{
    return lhs->BlendLayer() < rhs->BlendLayer();
}

AnimatedModel::AnimatedModel() :
    updateInvisible(false),
    animatedModelFlags(0),
    numBones(0),
    rootBone(nullptr)
{
    SetFlag(NF_SKINNED_GEOMETRY, true);
    SetFlag(NF_OCTREE_UPDATE_CALL, true);
}

AnimatedModel::~AnimatedModel()
{
    RemoveBones();
}

void AnimatedModel::RegisterObject()
{
    RegisterFactory<AnimatedModel>();
    CopyBaseAttributes<AnimatedModel, OctreeNode>();
    RegisterDerivedType<AnimatedModel, StaticModel>();
    RegisterMixedRefAttribute("model", &AnimatedModel::ModelAttr, &AnimatedModel::SetModelAttr, ResourceRef(Model::TypeStatic()));
    CopyBaseAttribute<AnimatedModel, StaticModel>("materials");
    CopyBaseAttribute<AnimatedModel, StaticModel>("lodBias");
    RegisterAttribute("updateInvisible", &AnimatedModel::UpdateInvisible, &AnimatedModel::SetUpdateInvisible, false);
    RegisterMixedRefAttribute("animationStates", &AnimatedModel::AnimationStatesAttr, &AnimatedModel::SetAnimationStatesAttr);
}

void AnimatedModel::OnOctreeUpdate(unsigned short frameNumber)
{
    if (animatedModelFlags & AMF_ANIMATION_DIRTY)
    {
        if (updateInvisible || WasInView(frameNumber))
            UpdateAnimation();
    }

    if (animatedModelFlags & AMF_SKINNING_DIRTY)
        UpdateSkinning();
}

bool AnimatedModel::OnPrepareRender(unsigned short frameNumber, Camera* camera)
{
    if (!StaticModel::OnPrepareRender(frameNumber, camera))
        return false;

    // Update animation here too if just came into view and animation / skinning is still dirty
    if (animatedModelFlags & AMF_ANIMATION_DIRTY)
        UpdateAnimation();

    if (animatedModelFlags & AMF_SKINNING_DIRTY)
        UpdateSkinning();

    return true;
}

void AnimatedModel::OnRender(size_t /*geomIndex*/, ShaderProgram* /*program*/)
{
    if (!skinMatrixBuffer || !numBones)
        return;

    if (animatedModelFlags & AMF_SKINNING_BUFFER_DIRTY)
    {
        skinMatrixBuffer->SetData(0, numBones * sizeof(Matrix3x4), &skinMatrices[0]);
        animatedModelFlags &= ~AMF_SKINNING_BUFFER_DIRTY;
    }

    skinMatrixBuffer->Bind(UB_SKINMATRICES);
}

void AnimatedModel::SetModel(Model* model_, bool createBones)
{
    StaticModel::SetModel(model_);
    if (createBones)
        CreateBones();
}

void AnimatedModel::SetUpdateInvisible(bool enable)
{
    updateInvisible = enable;
}

AnimationState* AnimatedModel::AddAnimationState(Animation* animation)
{
    if (!animation || !numBones)
        return nullptr;

    // Check for not adding twice
    AnimationState* existing = FindAnimationState(animation);
    if (existing)
        return existing;

    SharedPtr<AnimationState> newState(new AnimationState(this, animation));
    animationStates.push_back(newState);
    OnAnimationOrderChanged();

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

void AnimatedModel::RemoveAnimationState(StringHash animationNameHash)
{
    for (auto it = animationStates.begin(); it != animationStates.end(); ++it)
    {
        AnimationState* state = *it;
        Animation* animation = state->GetAnimation();
        // Check both the animation and the resource name
        if (animation->NameHash() == animationNameHash || animation->AnimationNameHash() == animationNameHash)
        {
            animationStates.erase(it);
            OnAnimationChanged();
            return;
        }
    }
}

void AnimatedModel::RemoveAnimationState(AnimationState* state)
{
    for (auto it = animationStates.begin(); it != animationStates.end(); ++it)
    {
        if (*it == state)
        {
            animationStates.erase(it);
            OnAnimationChanged();
            return;
        }
    }
}

void AnimatedModel::RemoveAnimationState(size_t index)
{
    if (index < animationStates.size())
    {
        animationStates.erase(animationStates.begin() + index);
        OnAnimationChanged();
    }
}

void AnimatedModel::RemoveAllAnimationStates()
{
    if (animationStates.size())
    {
        animationStates.clear();
        OnAnimationChanged();
    }
}

AnimationState* AnimatedModel::FindAnimationState(Animation* animation) const
{
    for (auto it = animationStates.begin(); it != animationStates.end(); ++it)
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

AnimationState* AnimatedModel::FindAnimationState(StringHash animationNameHash) const
{
    for (auto it = animationStates.begin(); it != animationStates.end(); ++it)
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
    return index < animationStates.size() ? animationStates[index].Get() : nullptr;
}

void AnimatedModel::OnTransformChanged()
{
    SpatialNode::OnTransformChanged();

    SetFlag(NF_BOUNDING_BOX_DIRTY, true);
    if (octree && (Flags() & (NF_OCTREE_REINSERT_QUEUED | NF_ENABLED)) == NF_ENABLED)
        octree->QueueUpdate(this);

    animatedModelFlags |= AMF_SKINNING_DIRTY;
}

void AnimatedModel::OnWorldBoundingBoxUpdate() const
{
    if (model && numBones)
    {
        const std::vector<ModelBone>& modelBones = model->Bones();

        worldBoundingBox = modelBones[0].boundingBox.Transformed(bones[0]->WorldTransform());
        for (size_t i = 1; i < numBones; ++i)
            worldBoundingBox.Merge(modelBones[i].boundingBox.Transformed(bones[i]->WorldTransform()));

        SetFlag(NF_BOUNDING_BOX_DIRTY, false);
    }
    else
        OctreeNode::OnWorldBoundingBoxUpdate();
}

void AnimatedModel::OnLoadFinished()
{
    Node::OnLoadFinished();
    CreateBones();

    animationStates.clear();

    const JSONArray& states = animationStatesAttr.GetArray();

    for (auto it = states.begin(); it != states.end(); ++it)
    {
        const JSONValue& state = *it;
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

    animationStatesAttr.SetNull();
}

void AnimatedModel::CreateBones()
{
    PROFILE(CreateAnimatedModelBones);

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

        Bone* existingBone = FindChild<Bone>(modelBone.nameHash, true);
        if (existingBone)
        {
            bones[i] = existingBone;
        }
        else
        {
            bones[i] = new Bone();
            bones[i]->SetName(modelBone.name);
            bones[i]->SetTransform(modelBone.initialPosition, modelBone.initialRotation, modelBone.initialScale);
        }

        bones[i]->SetAnimatedModel(this);
    }

    // Loop through bones again to set the correct parents
    for (size_t i = 0; i < modelBones.size(); ++i)
    {
        const ModelBone& desc = modelBones[i];
        if (desc.parentIndex == i)
        {
            bones[i]->SetParent(this);
            rootBone = bones[i];
        }
        else
            bones[i]->SetParent(bones[desc.parentIndex]);
    }

    if (!skinMatrixBuffer)
        skinMatrixBuffer = new UniformBuffer();
    skinMatrixBuffer->Define(USAGE_DYNAMIC, numBones * sizeof(Matrix3x4));

    // Set initial bounding box recalculation and skinning dirty
    OnBoneTransformChanged();
}

void AnimatedModel::UpdateAnimation()
{
    if (animatedModelFlags & AMF_ANIMATION_ORDER_DIRTY)
        std::sort(animationStates.begin(), animationStates.end(), CompareAnimationStates);
    
    animatedModelFlags |= AMF_IN_ANIMATION_UPDATE;

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
    rootBone->OnTransformChanged();

    animatedModelFlags &= ~(AMF_ANIMATION_ORDER_DIRTY | AMF_ANIMATION_DIRTY | AMF_IN_ANIMATION_UPDATE);

    // Update bounding box already here to take advantage of threaded update, and also to update bone world transforms for skinning
    OnWorldBoundingBoxUpdate();

    // If updating only when visible, queue octree reinsertion for next frame. This also ensures shadowmap rendering happens correctly
    // Else just dirty the skinning
    if (!updateInvisible)
    {
        if (octree && (Flags() & (NF_OCTREE_REINSERT_QUEUED | NF_ENABLED)) == NF_ENABLED)
            octree->QueueUpdate(this);
    }
    
    animatedModelFlags |= AMF_SKINNING_DIRTY;
}

void AnimatedModel::UpdateSkinning()
{
    const std::vector<ModelBone>& modelBones = model->Bones();

    for (size_t i = 0; i < numBones; ++i)
        skinMatrices[i] = bones[i]->WorldTransform() * modelBones[i].offsetMatrix;

    animatedModelFlags &= ~AMF_SKINNING_DIRTY;
    animatedModelFlags |= AMF_SKINNING_BUFFER_DIRTY;
}

void AnimatedModel::RemoveBones()
{
    if (!numBones)
        return;

    // Do not signal transform changes back to the model during deletion
    for (size_t i = 0; i < numBones; ++i)
        bones[i]->SetAnimatedModel(nullptr);
    
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

void AnimatedModel::SetModelAttr(const ResourceRef& value)
{
    ResourceCache* cache = Subsystem<ResourceCache>();
    // Do not set bones now, wait for load finishing instead
    SetModel(cache->LoadResource<Model>(value.name), false);
}

ResourceRef AnimatedModel::ModelAttr() const
{
    return ResourceRef(Model::TypeStatic(), ResourceName(model.Get()));
}

void AnimatedModel::SetAnimationStatesAttr(const JSONValue& value)
{
    animationStatesAttr = value;
}

JSONValue AnimatedModel::AnimationStatesAttr() const
{
    JSONValue states;
    states.SetEmptyArray();

    /// \todo Per-bone weights not serialized
    for (auto it = animationStates.begin(); it != animationStates.end(); ++it)
    {
        AnimationState* state = *it;
        JSONValue stateValue;
        stateValue.Push(state->GetAnimation() ? state->GetAnimation()->Name() : "");
        stateValue.Push(state->StartBone() ? state->StartBone()->Name() : "");
        stateValue.Push(state->Looped());
        stateValue.Push(state->Weight());
        stateValue.Push(state->Time());
        stateValue.Push((int)state->BlendLayer());
        states.Push(stateValue);
    }

    return states;
}
