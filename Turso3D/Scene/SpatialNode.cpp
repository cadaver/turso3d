// For conditions of distribution and use, see copyright notice in License.txt

#include "SpatialNode.h"

SpatialNode::SpatialNode() :
    worldTransform(Matrix3x4::IDENTITY)
{
    impl->position = Vector3::ZERO;
    impl->rotation = Quaternion::IDENTITY;
    impl->scale = Vector3::ONE;
    SetFlag(NF_SPATIAL, true);

}

void SpatialNode::RegisterObject()
{
    RegisterFactory<SpatialNode>();
    RegisterDerivedType<SpatialNode, Node>();
    CopyBaseAttributes<SpatialNode, Node>();
    RegisterRefAttribute("position", &SpatialNode::Position, &SpatialNode::SetPosition, Vector3::ZERO);
    RegisterRefAttribute("rotation", &SpatialNode::Rotation, &SpatialNode::SetRotation, Quaternion::IDENTITY);
    RegisterRefAttribute("scale", &SpatialNode::Scale, &SpatialNode::SetScale, Vector3::ONE);
    RegisterAttribute("static", &SpatialNode::Static, &SpatialNode::SetStatic, false);
}

void SpatialNode::SetPosition(const Vector3& newPosition)
{
    impl->position = newPosition;
    OnTransformChanged();
}

void SpatialNode::SetRotation(const Quaternion& newRotation)
{
    impl->rotation = newRotation;
    OnTransformChanged();
}

void SpatialNode::SetDirection(const Vector3& newDirection)
{
    impl->rotation = Quaternion(Vector3::FORWARD, newDirection);
    OnTransformChanged();
}

void SpatialNode::SetScale(const Vector3& newScale)
{
    impl->scale = newScale;
    // Make sure scale components never go to exactly zero, to prevent problems with decomposing the world matrix
    if (impl->scale.x == 0.0f)
        impl->scale.x = M_EPSILON;
    if (impl->scale.y == 0.0f)
        impl->scale.y = M_EPSILON;
    if (impl->scale.z == 0.0f)
        impl->scale.z = M_EPSILON;

    OnTransformChanged();
}

void SpatialNode::SetScale(float newScale)
{
    SetScale(Vector3(newScale, newScale, newScale));
}

void SpatialNode::SetTransform(const Vector3& newPosition, const Quaternion& newRotation)
{
    impl->position = newPosition;
    impl->rotation = newRotation;
    OnTransformChanged();
}

void SpatialNode::SetTransform(const Vector3& newPosition, const Quaternion& newRotation, const Vector3& newScale)
{
    impl->position = newPosition;
    impl->rotation = newRotation;
    impl->scale = newScale;
    OnTransformChanged();
}

void SpatialNode::SetTransform(const Vector3& newPosition, const Quaternion& newRotation, float newScale)
{
    SetTransform(newPosition, newRotation, Vector3(newScale, newScale, newScale));
}

void SpatialNode::SetWorldPosition(const Vector3& newPosition)
{
    SpatialNode* parentNode = SpatialParent();
    SetPosition(parentNode ? parentNode->WorldTransform().Inverse() * newPosition : newPosition);
}

void SpatialNode::SetWorldRotation(const Quaternion& newRotation)
{
    SpatialNode* parentNode = SpatialParent();
    SetRotation(parentNode ? parentNode->WorldRotation().Inverse() * newRotation : newRotation);
}

void SpatialNode::SetWorldDirection(const Vector3& newDirection)
{
    SpatialNode* parentNode = SpatialParent();
    SetDirection(parentNode ? parentNode->WorldRotation().Inverse() * newDirection : newDirection);
}

void SpatialNode::SetWorldScale(const Vector3& newScale)
{
    SpatialNode* parentNode = SpatialParent();
    SetScale(parentNode ? newScale / parentNode->WorldScale() : newScale);
}

void SpatialNode::SetWorldScale(float newScale)
{
    SetWorldScale(Vector3(newScale, newScale, newScale));
}

void SpatialNode::SetWorldTransform(const Vector3& newPosition, const Quaternion& newRotation)
{
    SpatialNode* parentNode = SpatialParent();
    if (parentNode)
    {
        Vector3 localPosition = parentNode->WorldTransform().Inverse() * newPosition;
        Quaternion localRotation = parentNode->WorldRotation().Inverse() * newRotation;
        SetTransform(localPosition, localRotation);
    }
    else
        SetTransform(newPosition, newRotation);
}

void SpatialNode::SetWorldTransform(const Vector3& newPosition, const Quaternion& newRotation, const Vector3& newScale)
{
    SpatialNode* parentNode = SpatialParent();
    if (parentNode)
    {
        Vector3 localPosition = parentNode->WorldTransform().Inverse() * newPosition;
        Quaternion localRotation = parentNode->WorldRotation().Inverse() * newRotation;
        Vector3 localScale = newScale / parentNode->WorldScale();
        SetTransform(localPosition, localRotation, localScale);
    }
    else
        SetTransform(newPosition, newRotation);
}

void SpatialNode::SetWorldTransform(const Vector3& newPosition, const Quaternion& newRotation, float newScale)
{
    SetWorldTransform(newPosition, newRotation, Vector3(newScale, newScale, newScale));
}

void SpatialNode::SetStatic(bool enable)
{
    if (enable != Static())
    {
        SetFlag(NF_STATIC, enable);
        // Handle possible octree reinsertion
        OnTransformChanged();
    }
}

void SpatialNode::Translate(const Vector3& delta, TransformSpace space)
{
    switch (space)
    {
    case TS_LOCAL:
        // Note: local space translation disregards local scale for scale-independent movement speed
        impl->position += impl->rotation * delta;
        break;

    case TS_PARENT:
        impl->position += delta;
        break;

    case TS_WORLD:
        if (!TestFlag(NF_SPATIAL_PARENT))
            impl->position += delta;
        else
            impl->position += SpatialParent()->WorldTransform().Inverse() * Vector4(delta, 0.0f);
        break;
    }

    OnTransformChanged();
}

void SpatialNode::Rotate(const Quaternion& delta, TransformSpace space)
{
    switch (space)
    {
    case TS_LOCAL:
        impl->rotation = (impl->rotation * delta);
        break;

    case TS_PARENT:
        impl->rotation = (delta * impl->rotation);
        break;

    case TS_WORLD:
        if (!TestFlag(NF_SPATIAL_PARENT))
        {
            impl->rotation = (delta * impl->rotation);
        }
        else
        {
            Quaternion worldRotation = WorldRotation();
            impl->rotation = impl->rotation * worldRotation.Inverse() * delta * worldRotation;
        }
        break;
    }

    impl->rotation.Normalize();
    OnTransformChanged();
}

void SpatialNode::RotateAround(const Vector3& point, const Quaternion& delta, TransformSpace space)
{
    SpatialNode* parentNode = SpatialParent();
    Vector3 parentSpacePoint;
    Quaternion oldRotation = impl->rotation;

    switch (space)
    {
    case TS_LOCAL:
        parentSpacePoint = Transform() * point;
        impl->rotation = (impl->rotation * delta);
        break;

    case TS_PARENT:
        parentSpacePoint = point;
        impl->rotation = (delta * impl->rotation);
        break;

    case TS_WORLD:
        if (!parentNode)
        {
            parentSpacePoint = point;
            impl->rotation = (delta * impl->rotation);
        }
        else
        {
            parentSpacePoint = parentNode->WorldTransform().Inverse() * point;
            Quaternion worldRotation = WorldRotation();
            impl->rotation = impl->rotation * worldRotation.Inverse() * delta * worldRotation;
        }
        break;
    }

    Vector3 oldRelativePos = oldRotation.Inverse() * (impl->position - parentSpacePoint);
    impl->rotation.Normalize();
    impl->position = impl->rotation * oldRelativePos + parentSpacePoint;
    OnTransformChanged();
}

void SpatialNode::Yaw(float angle, TransformSpace space)
{
    Rotate(Quaternion(angle, Vector3::UP), space);
}

void SpatialNode::Pitch(float angle, TransformSpace space)
{
    Rotate(Quaternion(angle, Vector3::RIGHT), space);
}

void SpatialNode::Roll(float angle, TransformSpace space)
{
    Rotate(Quaternion(angle, Vector3::FORWARD), space);
}

bool SpatialNode::LookAt(const Vector3& target, const Vector3& up, TransformSpace space)
{
    SpatialNode* parentNode = SpatialParent();
    Vector3 worldSpaceTarget;

    switch (space)
    {
    case TS_LOCAL:
        worldSpaceTarget = WorldTransform() * target;
        break;

    case TS_PARENT:
        worldSpaceTarget = !parentNode ? target : parentNode->WorldTransform() * target;
        break;

    case TS_WORLD:
        worldSpaceTarget = target;
        break;
    }

    Vector3 lookDir = worldSpaceTarget - WorldPosition();
    // Check if target is very close, in that case can not reliably calculate lookat direction
    if (lookDir.Equals(Vector3::ZERO))
        return false;
    Quaternion newRotation;
    // Do nothing if setting look rotation failed
    if (!newRotation.FromLookRotation(lookDir, up))
        return false;

    SetWorldRotation(newRotation);
    return true;
}

void SpatialNode::ApplyScale(float delta)
{
    ApplyScale(Vector3(delta, delta, delta));
}

void SpatialNode::ApplyScale(const Vector3& delta)
{
    impl->scale *= delta;
    OnTransformChanged();
}

void SpatialNode::OnParentSet(Node* newParent, Node*)
{
    SetFlag(NF_SPATIAL_PARENT, dynamic_cast<SpatialNode*>(newParent) != 0);
    OnTransformChanged();
}

void SpatialNode::OnTransformChanged()
{
    SpatialNode* cur = this;

    for (;;)
    {
        // Precondition:
        // a) whenever a node is marked dirty, all its children are marked dirty as well.
        // b) whenever a node is cleared from being dirty, all its parents must have been cleared as well.
        // Therefore if we are recursing here to mark this node dirty, and it already was, then all children of this node must also be already dirty,
        // and we don't need to reflag them again.
        if (cur->TestFlag(NF_WORLD_TRANSFORM_DIRTY))
            return;
        cur->SetFlag(NF_WORLD_TRANSFORM_DIRTY, true);

        // Tail call optimization: Don't recurse to mark the first child dirty, but instead process it in the context of the current function. 
        // If there are more than one child, then recurse to the excess children.
        auto it = cur->impl->children.begin();
        if (it != cur->impl->children.end())
        {
            Node* next = *it;
            for (++it; it != cur->impl->children.end(); ++it)
            {
                Node* child = *it;
                if (child->TestFlag(NF_SPATIAL))
                    static_cast<SpatialNode*>(child)->OnTransformChanged();
            }
            if (next->TestFlag(NF_SPATIAL))
                cur = static_cast<SpatialNode*>(next);
            else
                return;
        }
        else
            return;
    }
}

void SpatialNode::UpdateWorldTransform() const
{
    if (TestFlag(NF_SPATIAL_PARENT))
        worldTransform = static_cast<SpatialNode*>(Parent())->WorldTransform() * Matrix3x4(impl->position, impl->rotation, impl->scale);
    else
        worldTransform = Matrix3x4(impl->position, impl->rotation, impl->scale);
    SetFlag(NF_WORLD_TRANSFORM_DIRTY, false);
}
