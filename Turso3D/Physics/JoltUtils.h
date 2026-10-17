// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/BoundingBox.h"
#include "../Math/Quaternion.h"

#include <Jolt/Jolt.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

/// Jolt object layers.
namespace PhysicsLayers
{
    static const JPH::ObjectLayer NON_MOVING = 0;
    static const JPH::ObjectLayer MOVING = 1;
    static const JPH::uint NUM_LAYERS = 2;
}

/// Jolt broadphase layers.
namespace PhysicsBroadPhaseLayers
{
    static const JPH::BroadPhaseLayer NON_MOVING(0);
    static const JPH::BroadPhaseLayer MOVING(1);
    static const JPH::uint NUM_LAYERS = 2;
}

/// Convert a vector to Jolt.
inline JPH::Vec3 ToJolt(const Vector3& v) { return JPH::Vec3(v.x, v.y, v.z); }
/// Convert a position vector to Jolt.
inline JPH::RVec3 ToJoltR(const Vector3& v) { return JPH::RVec3(v.x, v.y, v.z); }
/// Convert a quaternion to Jolt.
inline JPH::Quat ToJolt(const Quaternion& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
/// Convert a vector from Jolt.
inline Vector3 ToVector3(JPH::Vec3Arg v) { return Vector3(v.GetX(), v.GetY(), v.GetZ()); }
#ifdef JPH_DOUBLE_PRECISION
/// Convert a position vector from Jolt.
inline Vector3 ToVector3(JPH::DVec3Arg v) { return Vector3((float)v.GetX(), (float)v.GetY(), (float)v.GetZ()); }
#endif
/// Convert a quaternion from Jolt.
inline Quaternion ToQuaternion(JPH::QuatArg q) { return Quaternion(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }
/// Convert a bounding box from Jolt.
inline BoundingBox ToBoundingBox(const JPH::AABox& box) { return BoundingBox(ToVector3(box.mMin), ToVector3(box.mMax)); }
