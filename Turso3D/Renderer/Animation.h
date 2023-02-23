// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Quaternion.h"
#include "../Resource/Resource.h"

static const unsigned char CHANNEL_POSITION = 1;
static const unsigned char CHANNEL_ROTATION = 2;
static const unsigned char CHANNEL_SCALE = 4;

/// Skeletal animation keyframe.
struct AnimationKeyFrame
{
    /// Construct.
    AnimationKeyFrame() :
        time(0.0f),
        scale(Vector3::ONE)
    {
    }

    /// Keyframe time.
    float time;
    /// Bone position.
    Vector3 position;
    /// Bone rotation.
    Quaternion rotation;
    /// Bone scale.
    Vector3 scale;
};

/// Skeletal animation track, stores keyframes of a single bone.
struct AnimationTrack
{
    /// Adjust keyframe index by time.
    void FindKeyFrameIndex(float time, size_t& index) const;

    /// Bone or scene node name.
    std::string name;
    /// Name hash.
    StringHash nameHash;
    /// Bitmask of included data (position, rotation, scale.)
    unsigned char channelMask;
    /// Keyframes.
    std::vector<AnimationKeyFrame> keyFrames;
};

/// Skeletal animation resource.
class Animation : public Resource
{
    OBJECT(Animation);

public:
    /// Construct.
    Animation();
    /// Destruct.
    ~Animation();

    /// Register object factory.
    static void RegisterObject();

    /// Load animation from a stream. Return true on success.
    bool BeginLoad(Stream& source) override;
    
    /// Set animation name.
    void SetAnimationName(const std::string& name);
    /// Set animation length.
    void SetLength(float length);
    /// Create and return a track by name. If track by same name already exists, returns the existing.
    AnimationTrack* CreateTrack(const std::string& name);
    /// Remove a track by name. This is unsafe if the animation is currently used in playback.
    void RemoveTrack(const std::string& name);
    /// Remove all tracks. This is unsafe if the animation is currently used in playback.
    void RemoveAllTracks();

    /// Return animation name.
    const std::string& AnimationName() const { return animationName; }
    /// Return animation name hash.
    StringHash AnimationNameHash() const { return animationNameHash; }
    /// Return animation length.
    float Length() const { return length; }
    /// Return all animation tracks.
    const std::map<StringHash, AnimationTrack>& Tracks() const { return tracks; }
    /// Return number of animation tracks.
    size_t NumTracks() const { return tracks.size(); }
    /// Return animation track by index.
    AnimationTrack* Track(size_t index) const;
    /// Return animation track by name.
    AnimationTrack* FindTrack(const std::string& name) const;
    /// Return animation track by name hash.
    AnimationTrack* FindTrack(StringHash nameHash) const;

private:
    /// Animation name.
    std::string animationName;
    /// Animation name hash.
    StringHash animationNameHash;
    /// Animation length.
    float length;
    /// Animation tracks.
    std::map<StringHash, AnimationTrack> tracks;
};
