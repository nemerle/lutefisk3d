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

#include "Cursor.h"

#include "../Core/Context.h"
#include "../Input/Input.h"
#include "../Input/InputEvents.h"
#include "../IO/Log.h"
#include "../Container/Ptr.h"
#include "../Resource/ResourceCache.h"
#include "../Graphics/Texture2D.h"
#include "../UI/UI.h"

#include "SDL2/SDL_mouse.h"

namespace Urho3D
{

static const char* shapeNames[] =
{
    "Normal",
    "IBeam",
    "Cross",
    "ResizeVertical",
    "ResizeDiagonalTopRight",
    "ResizeHorizontal",
    "ResizeDiagonalTopLeft",
    "ResizeAll",
    "AcceptDrop",
    "RejectDrop",
    "Busy",
    "BusyArrow"
};

/// OS cursor shape lookup table matching cursor shape enumeration
#if !defined(ANDROID) && !defined(IOS)
static const int osCursorLookup[CS_MAX_SHAPES] =
{
    SDL_SystemCursor::SDL_SYSTEM_CURSOR_ARROW,    // CS_NORMAL
    SDL_SYSTEM_CURSOR_IBEAM,     // CS_IBEAM
    SDL_SYSTEM_CURSOR_CROSSHAIR, // CS_CROSS
    SDL_SYSTEM_CURSOR_SIZENS,   // CS_RESIZEVERTICAL
    SDL_SYSTEM_CURSOR_SIZENESW, // CS_RESIZEDIAGONAL_TOPRIGHT
    SDL_SYSTEM_CURSOR_SIZEWE,   // CS_RESIZEHORIZONTAL
    SDL_SYSTEM_CURSOR_SIZENWSE, // CS_RESIZEDIAGONAL_TOPLEFT
    SDL_SYSTEM_CURSOR_SIZEALL,   // CS_RESIZE_ALL
    SDL_SYSTEM_CURSOR_HAND,     // CS_ACCEPTDROP
    SDL_SYSTEM_CURSOR_NO,       // CS_REJECTDROP
    SDL_SYSTEM_CURSOR_WAIT,   // CS_BUSY
    SDL_SYSTEM_CURSOR_WAITARROW // CS_BUSY_ARROW
};
#endif

extern const char* UI_CATEGORY;

Cursor::Cursor(Context* context) :
    BorderImage(context),
    shape_(shapeNames[CS_NORMAL]),
    useSystemShapes_(false),
    osShapeDirty_(false)
{
    // Define the defaults for system cursor usage.
    for (unsigned i = 0; i < CS_MAX_SHAPES; i++)
        shapeInfos_[shapeNames[i]] = CursorShapeInfo(i);

    // Subscribe to OS mouse cursor visibility changes to be able to reapply the cursor shape
    SubscribeToEvent(E_MOUSEVISIBLECHANGED, HANDLER(Cursor, HandleMouseVisibleChanged));
}

Cursor::~Cursor()
{
    for (auto iter=shapeInfos_.begin(); iter != shapeInfos_.end(); ++iter)
    {
        CursorShapeInfo &info(MAP_VALUE(iter));

        if (info.osCursor_)
        {
            SDL_FreeCursor(MAP_VALUE(iter).osCursor_);
            info.osCursor_ = nullptr;
        }
    }
}

void Cursor::RegisterObject(Context* context)
{
    context->RegisterFactory<Cursor>(UI_CATEGORY);

    COPY_BASE_ATTRIBUTES(BorderImage);
    UPDATE_ATTRIBUTE_DEFAULT_VALUE("Priority", M_MAX_INT);
    ACCESSOR_ATTRIBUTE("Use System Shapes", GetUseSystemShapes, SetUseSystemShapes, bool, false, AM_FILE);
    MIXED_ACCESSOR_ATTRIBUTE("Shapes", GetShapesAttr, SetShapesAttr, VariantVector, Variant::emptyVariantVector, AM_FILE);
}

void Cursor::GetBatches(std::vector<UIBatch>& batches, std::vector<float>& vertexData, const IntRect& currentScissor)
{
    unsigned initialSize = vertexData.size();
    const IntVector2& offset = shapeInfos_[shape_].hotSpot_;
    Vector2 floatOffset(-(float)offset.x_, -(float)offset.y_);

    BorderImage::GetBatches(batches, vertexData, currentScissor);
    for (unsigned i = initialSize; i < vertexData.size(); i += 6)
    {
        vertexData[i] += floatOffset.x_;
        vertexData[i + 1] += floatOffset.y_;
    }
}

void Cursor::DefineShape(CursorShape shape, Image* image, const IntRect& imageRect, const IntVector2& hotSpot)
{
    if (shape < CS_NORMAL || shape >= CS_MAX_SHAPES)
    {
        LOGERROR("Shape index out of bounds, can not define cursor shape");
        return;
    }

    DefineShape(shapeNames[shape], image, imageRect, hotSpot);
}

void Cursor::DefineShape(const QString& shape, Image* image, const IntRect& imageRect, const IntVector2& hotSpot)
{
    if (!image)
        return;

    ResourceCache* cache = GetSubsystem<ResourceCache>();

    if (!shapeInfos_.contains(shape))
        shapeInfos_[shape] = CursorShapeInfo();

    CursorShapeInfo& info = shapeInfos_[shape];

    // Prefer to get the texture with same name from cache to prevent creating several copies of the texture
    info.texture_ = cache->GetResource<Texture2D>(image->GetName(), false);
    if (!info.texture_)
    {
        Texture2D* texture = new Texture2D(context_);
        texture->SetData(SharedPtr<Image>(image));
        info.texture_ = texture;
    }

    info.image_ = image;
    info.imageRect_ = imageRect;
    info.hotSpot_ = hotSpot;

    // Remove existing SDL cursor
    if (info.osCursor_)
    {
        SDL_FreeCursor(info.osCursor_);
        info.osCursor_ = nullptr;
    }

    // Reset current shape if it was edited
    if (shape_ == shape)
    {
        shape_ = QString::null;
        SetShape(shape);
    }
}


void Cursor::SetShape(const QString& shape)
{
    if (shape == QString::null || shape.isEmpty() || shape_ == shape || !shapeInfos_.contains(shape))
        return;

    shape_ = shape;

    CursorShapeInfo& info = shapeInfos_[shape_];
    texture_ = info.texture_;
    imageRect_ = info.imageRect_;
    SetSize(info.imageRect_.Size());

    // To avoid flicker, the UI subsystem will apply the OS shape once per frame. Exception: if we are using the
    // busy shape, set it immediately as we may block before that
    osShapeDirty_ = true;
    if (shape_ == shapeNames[CS_BUSY])
        ApplyOSCursorShape();
}

void Cursor::SetShape(CursorShape shape)
{
    if (shape < CS_NORMAL || shape >= CS_MAX_SHAPES || shape_ == shapeNames[shape])
        return;

    SetShape(shapeNames[shape]);
}

void Cursor::SetUseSystemShapes(bool enable)
{
    if (enable != useSystemShapes_)
    {
        useSystemShapes_ = enable;
        // Reapply current shape
        osShapeDirty_ = true;
    }
}

void Cursor::SetShapesAttr(const VariantVector& value)
{

    if (!value.size())
        return;

    VariantVector::const_iterator iter = value.cbegin();
    for (; iter != value.end(); iter++)
    {
        VariantVector shapeVector = iter->GetVariantVector();
        if (shapeVector.size() >= 4)
        {
            QString shape = shapeVector[0].GetString();
            ResourceRef ref = shapeVector[1].GetResourceRef();
            IntRect imageRect = shapeVector[2].GetIntRect();
            IntVector2 hotSpot = shapeVector[3].GetIntVector2();

            DefineShape(shape, GetSubsystem<ResourceCache>()->GetResource<Image>(ref.name_), imageRect, hotSpot);
        }
    }
}

VariantVector Cursor::GetShapesAttr() const
{
    VariantVector ret;

    for (auto & iter_pair : shapeInfos_)
    {
        auto & elem(iter_pair.second);
        if (elem.imageRect_ != IntRect::ZERO)
        {
            // Could use a map but this simplifies the UI xml.
            VariantVector shape;
            shape.push_back(ELEMENT_KEY(iter_pair));
            shape.push_back(GetResourceRef(elem.texture_, Texture2D::GetTypeStatic()));
            shape.push_back(elem.imageRect_);
            shape.push_back(elem.hotSpot_);
            ret.push_back(shape);
        }
    }

    return ret;
}

void Cursor::ApplyOSCursorShape()
{
    // Mobile platforms do not support applying OS cursor shapes: comment out to avoid log error messages
#if !defined(ANDROID) && !defined(IOS)
    if (!osShapeDirty_ || !GetSubsystem<Input>()->IsMouseVisible() || GetSubsystem<UI>()->GetCursor() != this)
        return;

    CursorShapeInfo& info = shapeInfos_[shape_];

    // Remove existing SDL cursor if is not a system shape while we should be using those, or vice versa
    if (info.osCursor_ && info.systemDefined_ != useSystemShapes_)
    {
        SDL_FreeCursor(info.osCursor_);
        info.osCursor_ = nullptr;
    }

    // Create SDL cursor now if necessary
    if (!info.osCursor_)
    {
        // Create a system default shape
        if (useSystemShapes_ && info.systemCursor_ >= 0 && info.systemCursor_ < CS_MAX_SHAPES)
        {
            info.osCursor_ = SDL_CreateSystemCursor((SDL_SystemCursor)osCursorLookup[info.systemCursor_]);
            info.systemDefined_ = true;
            if (!info.osCursor_)
                LOGERROR("Could not create system cursor");
        }
        // Create from image
        else if (info.image_)
        {
            SDL_Surface* surface = info.image_->GetSDLSurface(info.imageRect_);

            if (surface)
            {
                info.osCursor_ = SDL_CreateColorCursor(surface, info.hotSpot_.x_, info.hotSpot_.y_);
                info.systemDefined_ = false;
                if (!info.osCursor_)
                    LOGERROR("Could not create cursor from image " + info.image_->GetName());
                SDL_FreeSurface(surface);
            }
        }
    }

    if (info.osCursor_)
        SDL_SetCursor(info.osCursor_);

    osShapeDirty_ = false;
#endif
}

void Cursor::HandleMouseVisibleChanged(StringHash eventType, VariantMap& eventData)
{
    ApplyOSCursorShape();
}

}