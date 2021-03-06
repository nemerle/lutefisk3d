//
// Copyright (c) 2008-2016 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "Lutefisk3D/Scene/Component.h"
#include "Lutefisk3D/Scene/SceneEvents.h"

namespace Urho3D
{
enum eSmoothingModes : unsigned
{
    SMOOTH_NONE     = 0, //!< No ongoing smoothing.
    SMOOTH_POSITION = 1, //!< Ongoing position smoothing.
    SMOOTH_ROTATION = 2, //!< Ongoing rotation smoothing.
};
/// Transform smoothing component for network updates.
class LUTEFISK3D_EXPORT SmoothedTransform : public Component, public SmoothedTransformSignals
{
    URHO3D_OBJECT(SmoothedTransform,Component)

public:
    /// Construct.
    SmoothedTransform(Context* context);
    /// Destruct.
    ~SmoothedTransform() = default;
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Update smoothing.
    void Update(float constant, float squaredSnapThreshold);
    /// Set target position in parent space.
    void SetTargetPosition(const Vector3& position);
    /// Set target rotation in parent space.
    void SetTargetRotation(const Quaternion& rotation);
    /// Set target position in world space.
    void SetTargetWorldPosition(const Vector3& position);
    /// Set target rotation in world space.
    void SetTargetWorldRotation(const Quaternion& rotation);

    /// Return target position in parent space.
    const Vector3& GetTargetPosition() const { return targetPosition_; }
    /// Return target rotation in parent space.
    const Quaternion& GetTargetRotation() const { return targetRotation_; }
    /// Return target position in world space.
    Vector3 GetTargetWorldPosition() const;
    /// Return target rotation in world space.
    Quaternion GetTargetWorldRotation() const;
    /// Return whether smoothing is in progress.
    bool IsInProgress() const { return smoothingMask_ != 0; }

protected:
    /// Handle scene node being assigned at creation.
    virtual void OnNodeSet(Node* node) override;

private:
    /// Target position.
    Vector3 targetPosition_;
    /// Target rotation.
    Quaternion targetRotation_;
    /// Active smoothing operations bitmask.
    unsigned char smoothingMask_;
    /// Subscribed to smoothing update event flag.
    bool subscribed_;
};

}
