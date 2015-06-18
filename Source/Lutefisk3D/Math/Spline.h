//
// Copyright (c) 2008-2015 the Urho3D project.
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

#include "../Math/Color.h"
#include "../Math/Vector2.h"
#include "../Math/Vector3.h"
#include "../Math/Vector4.h"
#include "../IO/Log.h"
#include <QtCore/QVariant>

namespace Urho3D
{

enum InterpolationMode
{
    BEZIER_CURVE = 0
};

/// Spline class to get a point on it based off the interpolation mode.
template<class T>
class Spline
{
    Q_GADGET
public:
    /// Default constructor.
    Spline() : interpolationMode_(BEZIER_CURVE) { }
    /// Constructor setting InterpolationMode.
    Spline(InterpolationMode mode) : interpolationMode_(mode) { }
    /// Constructor setting Knots and InterpolationMode.
    Spline(const std::vector<T>& knots, InterpolationMode mode = BEZIER_CURVE) :
        interpolationMode_(mode),
        knots_(knots)
    {
    }
    /// Copy constructor.
    Spline(const Spline& rhs) :
        interpolationMode_(rhs.interpolationMode_),
        knots_(rhs.knots_)
    {
    }

    /// Copy operator.
    void operator= (const Spline& rhs)
    {
        knots_ = rhs.knots_;
        interpolationMode_ = rhs.interpolationMode_;
    }
    /// Equality operator.
    bool operator== (const Spline& rhs) const
    {
        return (knots_ == rhs.knots_ && interpolationMode_ == rhs.interpolationMode_);
    }
    /// Non Equality operator.
    bool operator!= (const Spline& rhs) const
    {
        return !(*this == rhs);
    }

    /// Return the ImplementationMode.
    InterpolationMode GetInterpolationMode() const { return interpolationMode_; }
    /// Return the Knots of the Spline.
    const std::vector<T>& GetKnots() const { return knots_; }
    /// Return the Knot at the specific index.
    QVariant GetKnot(unsigned index) const { return knots_[index]; }
    /// Return the T of the point of the Spline at f from 0.f - 1.f.
    QVariant GetPoint(float f) const
    {
        if (knots_.size() < 2)
            return knots_.size() == 1 ? knots_[0] : T();

        if (f > 1.f)
            f = 1.f;
        else if (f < 0.f)
            f = 0.f;

        switch (interpolationMode_)
        {
        case BEZIER_CURVE:
            return BezierInterpolation(knots_, f);

        default:
            LOGERROR("Unsupported interpolation mode");
            return T();
        }
    }
    /// Set the InterpolationMode of the Spline.
    void SetInterpolationMode(InterpolationMode interpolationMode) { interpolationMode_ = interpolationMode; }
    /// Set the Knots of the Spline.
    void SetKnots(const std::vector<QVariant>& knots) { knots_ = knots; }
    /// Set the Knot value of an existing Knot.
    void SetKnot(const T& knot, unsigned index)
    {
        if (index < knots_.size())
        {
            if (knots_.size() > 0 && knots_[0].GetType() == knot.type())
                knots_[index] = knot;
            else if (knots_.empty())
                knots_.push_back(knot);
            else
                LOGERROR(QString("Attempted to set a Spline's Knot value of type %1 where elements are already using %2").arg(knot.GetTypeName()).arg(knots_[0].GetTypeName()));
        }
    }
    /// Add a Knot to the end of the Spline.
    void AddKnot(const T& knot)
    {
        if (!knots_.empty() && knots_[0].type() == knot.type())
            knots_.push_back(knot);
        else if (knots_.empty())
            knots_.push_back(knot);
        else
            LOGERROR(QString("Attempted to add Knot to Spline of type %1 where elements are already using %2").arg(knot.GetTypeName()).arg(knots_[0].GetTypeName()));
    }
    /// Add a Knot to the Spline at a specific index.
    void AddKnot(const T& knot, unsigned index)
    {
        if (index > knots_.size())
            index = knots_.size();

        if (knots_.size() > 0 && knots_[0].type() == knot.type())
            knots_.insert(knots_.begin()+index, knot);
        else if (knots_.empty())
            knots_.push_back(knot);
        else
            LOGERROR(QString("Attempted to add Knot to Spline of type %1 where elements are already using %2").arg(knot.GetTypeName()).arg(knots_[0].GetTypeName()));
    }
    /// Remove the last Knot on the Spline.
    void RemoveKnot() { knots_.pop_back(); }
    /// Remove the Knot at the specific index.
    void RemoveKnot(unsigned index) { knots_.erase(knots_.begin()+index); }
    /// Clear the Spline.
    void Clear() { knots_.clear(); }

private:
    /// Perform Bezier Interpolation on the Spline.
    T BezierInterpolation(const std::vector<T>& knots, float t) const
    {
        if (knots.size() == 2)
        {
            return LinearInterpolation(knots[0], knots[1], t);
        }
        else
        {
            std::vector<T> interpolatedKnots;
            interpolatedKnots.reserve(knots.size()-1);
            for (unsigned i = 1; i < knots.size(); i++)
            {
                interpolatedKnots.push_back(LinearInterpolation(knots[i - 1], knots[i], t));
            }
            return BezierInterpolation(interpolatedKnots, t);
        }
    }

    /// LinearInterpolation between two Variants based on underlying type.
    float LinearInterpolation(float lhs, float rhs, float t) const {
        return Lerp(lhs,rhs, t);
    }
    /// LinearInterpolation between two Variants based on underlying type.
    T LinearInterpolation(const T& lhs, const T& rhs, float t) const {
        return lhs.Lerp(rhs, t);
    }
    /// InterpolationMode.
    InterpolationMode interpolationMode_;
    /// Knots on the Spline.
    std::vector<T> knots_;
};

}
