//
// Copyright (c) 2008-2017 the Urho3D project.
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

#include "AnimatedSprite2D.h"

#include "AnimationSet2D.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Scene/Scene.h"
#include "Lutefisk3D/Scene/SceneEvents.h"
#include "Sprite2D.h"
#include "SpriterInstance2D.h"

#ifdef LUTEFISK3D_SPINE
#include <spine/spine.h>
#endif

namespace Urho3D
{

extern const char* URHO2D_CATEGORY;
extern const char* blendModeNames[];

const char* loopModeNames[] =
{
    "Default",
    "ForceLooped",
    "ForceClamped",
    nullptr
};

AnimatedSprite2D::AnimatedSprite2D(Context* context) :
    StaticSprite2D(context),
    #ifdef LUTEFISK3D_SPINE
    skeleton_(0),
    animationStateData_(0),
    animationState_(0),
    #endif
    speed_(1.0f),
    loopMode_(LM_DEFAULT)
{
}

AnimatedSprite2D::~AnimatedSprite2D()
{
    Dispose();
}

void AnimatedSprite2D::RegisterObject(Context* context)
{
    context->RegisterFactory<AnimatedSprite2D>(URHO2D_CATEGORY);

    URHO3D_COPY_BASE_ATTRIBUTES(StaticSprite2D);
    URHO3D_REMOVE_ATTRIBUTE("Sprite");
    URHO3D_ACCESSOR_ATTRIBUTE("Speed", GetSpeed, SetSpeed, float, 1.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Entity", GetEntity, SetEntity, QString, QString(), AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Animation Set", GetAnimationSetAttr, SetAnimationSetAttr, ResourceRef, ResourceRef(AnimatedSprite2D::GetTypeStatic()), AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Animation", GetAnimation, SetAnimationAttr, QString, QString(), AM_DEFAULT);
    URHO3D_ENUM_ACCESSOR_ATTRIBUTE("Loop Mode", GetLoopMode, SetLoopMode, LoopMode2D, loopModeNames, LM_DEFAULT, AM_DEFAULT);
}

void AnimatedSprite2D::OnSetEnabled()
{
    StaticSprite2D::OnSetEnabled();
    bool enabled = IsEnabledEffective();

    Scene* scene = GetScene();
    if (scene)
    {
        if (enabled)
            scene->scenePostUpdate.Connect(this,&AnimatedSprite2D::HandleScenePostUpdate);
        else
            scene->scenePostUpdate.Disconnect(this,&AnimatedSprite2D::HandleScenePostUpdate);
    }
}

void AnimatedSprite2D::SetAnimationSet(AnimationSet2D* animationSet)
{
    if (animationSet == animationSet_)
        return;

    Dispose();

    animationSet_ = animationSet;
    if (!animationSet_)
        return;

    SetSprite(animationSet_->GetSprite());

#ifdef LUTEFISK3D_SPINE
    if (animationSet_->GetSkeletonData())
    {
        spSkeletonData* skeletonData = animationSet->GetSkeletonData();

        // Create skeleton
        skeleton_ = spSkeleton_create(skeletonData);
        skeleton_->flipX = flipX_;
        skeleton_->flipY = flipY_;

        if (skeleton_->data->skinsCount > 0)
        {
            // If entity is empty use first skin in spine
            if (entity_.Empty())
                entity_ = skeleton_->data->skins[0]->name;
            spSkeleton_setSkinByName(skeleton_, qPrintable(entity_));
        }

        spSkeleton_updateWorldTransform(skeleton_);
    }
#endif
    if (animationSet_->GetSpriterData())
    {
        spriterInstance_.reset(new Spriter::SpriterInstance(this, animationSet_->GetSpriterData()));

        if (!animationSet_->GetSpriterData()->entities_.empty())
        {
            // If entity is empty use first entity in spriter
            if (entity_.isEmpty())
                entity_ = animationSet_->GetSpriterData()->entities_[0]->name_;
            spriterInstance_->SetEntity(entity_);
        }
    }

    // Clear animation name
    animationName_.clear();
    loopMode_ = LM_DEFAULT;
}

void AnimatedSprite2D::SetEntity(const QString& entity)
{
    if (entity == entity_)
        return;

    entity_ = entity;

#ifdef LUTEFISK3D_SPINE
    if (skeleton_)
        spSkeleton_setSkinByName(skeleton_, qPrintable(entity_));
#endif
    if (spriterInstance_)
        spriterInstance_->SetEntity(entity_);
}

void AnimatedSprite2D::SetAnimation(const QString& name, LoopMode2D loopMode)
{
    animationName_ = name;
    loopMode_ = loopMode;

    if (!animationSet_ || !animationSet_->HasAnimation(animationName_))
        return;

#ifdef LUTEFISK3D_SPINE
    if (skeleton_)
        SetSpineAnimation();
#endif
    if (spriterInstance_)
        SetSpriterAnimation();
}

void AnimatedSprite2D::SetLoopMode(LoopMode2D loopMode)
{
    loopMode_ = loopMode;
}

void AnimatedSprite2D::SetSpeed(float speed)
{
    speed_ = speed;
    MarkNetworkUpdate();
}

AnimationSet2D* AnimatedSprite2D::GetAnimationSet() const
{
    return animationSet_;
}


void AnimatedSprite2D::SetAnimationSetAttr(const ResourceRef& value)
{
    ResourceCache* cache = context_->m_ResourceCache.get();
    SetAnimationSet(cache->GetResource<AnimationSet2D>(value.name_));
}

ResourceRef AnimatedSprite2D::GetAnimationSetAttr() const
{
    return GetResourceRef(animationSet_, AnimationSet2D::GetTypeStatic());
}

void AnimatedSprite2D::OnSceneSet(Scene* scene)
{
    StaticSprite2D::OnSceneSet(scene);

    if (scene)
    {
        if (scene == node_)
            URHO3D_LOGWARNING(GetTypeName() + " should not be created to the root scene node");
        if (IsEnabledEffective())
            scene->scenePostUpdate.Connect(this,&AnimatedSprite2D::HandleScenePostUpdate);
    }
    else {
        assert(GetScene());
        GetScene()->scenePostUpdate.Disconnect(this,&AnimatedSprite2D::HandleScenePostUpdate);
    }
}

void AnimatedSprite2D::SetAnimationAttr(const QString& name)
{
    animationName_ = name;

    SetAnimation(animationName_, loopMode_);
}

void AnimatedSprite2D::UpdateSourceBatches()
{
#ifdef LUTEFISK3D_SPINE
    if (skeleton_ && animationState_)
        UpdateSourceBatchesSpine();
#endif
    if (spriterInstance_ && spriterInstance_->GetAnimation())
        UpdateSourceBatchesSpriter();

    sourceBatchesDirty_ = false;
}

void AnimatedSprite2D::HandleScenePostUpdate(Scene *,float timeStep)
{
    UpdateAnimation(timeStep);
}

void AnimatedSprite2D::UpdateAnimation(float timeStep)
{
#ifdef LUTEFISK3D_SPINE
    if (skeleton_ && animationState_)
        UpdateSpineAnimation(timeStep);
#endif
    if (spriterInstance_ && spriterInstance_->GetAnimation())
        UpdateSpriterAnimation(timeStep);
}

#ifdef LUTEFISK3D_SPINE
void AnimatedSprite2D::SetSpineAnimation()
{
    if (!animationStateData_)
    {
        animationStateData_ = spAnimationStateData_create(animationSet_->GetSkeletonData());
        if (!animationStateData_)
        {
            URHO3D_LOGERROR("Create animation state data failed");
            return;
        }
    }


    if (!animationState_)
    {
        animationState_ = spAnimationState_create(animationStateData_);
        if (!animationState_)
        {
            URHO3D_LOGERROR("Create animation state failed");
            return;
        }
    }

    // Reset slots to setup pose, fix issue #932
    spSkeleton_setSlotsToSetupPose(skeleton_);
    spAnimationState_setAnimationByName(animationState_, 0, qPrintable(animationName_), loopMode_ != LM_FORCE_CLAMPED ? true : false);
    UpdateAnimation(0.0f);
    MarkNetworkUpdate();
}

void AnimatedSprite2D::UpdateSpineAnimation(float timeStep)
{
    timeStep *= speed_;

    skeleton_->flipX = flipX_;
    skeleton_->flipY = flipY_;

    spSkeleton_update(skeleton_, timeStep);
    spAnimationState_update(animationState_, timeStep);
    spAnimationState_apply(animationState_, skeleton_);
    spSkeleton_updateWorldTransform(skeleton_);

    sourceBatchesDirty_ = true;
    worldBoundingBoxDirty_ = true;
}

void AnimatedSprite2D::UpdateSourceBatchesSpine()
{
    const Matrix3x4& worldTransform = GetNode()->GetWorldTransform();

    SourceBatch2D& sourceBatch = sourceBatches_[0];
    sourceBatches_[0].vertices_.clear();

    const constexpr int SLOT_VERTEX_COUNT_MAX = 1024;
    static float slotVertices[SLOT_VERTEX_COUNT_MAX];

    for (int i = 0; i < skeleton_->slotsCount; ++i)
    {
        spSlot* slot = skeleton_->drawOrder[i];
        spAttachment* attachment = slot->attachment;
        if (!attachment)
            continue;

        unsigned color = Color(color_.r_ * slot->r,
                               color_.g_ * slot->g,
                               color_.b_ * slot->b,
                               color_.a_ * slot->a).ToUInt();

        if (attachment->type == SP_ATTACHMENT_REGION)
        {
            spRegionAttachment* region = (spRegionAttachment*)attachment;
            spRegionAttachment_computeWorldVertices(region, slot->bone, slotVertices);

            Vertex2D vertices[4];
            vertices[0].position_ = worldTransform * Vector3(slotVertices[SP_VERTEX_X1], slotVertices[SP_VERTEX_Y1]);
            vertices[1].position_ = worldTransform * Vector3(slotVertices[SP_VERTEX_X2], slotVertices[SP_VERTEX_Y2]);
            vertices[2].position_ = worldTransform * Vector3(slotVertices[SP_VERTEX_X3], slotVertices[SP_VERTEX_Y3]);
            vertices[3].position_ = worldTransform * Vector3(slotVertices[SP_VERTEX_X4], slotVertices[SP_VERTEX_Y4]);

            vertices[0].color_ = color;
            vertices[1].color_ = color;
            vertices[2].color_ = color;
            vertices[3].color_ = color;

            vertices[0].uv_ = Vector2(region->uvs[SP_VERTEX_X1], region->uvs[SP_VERTEX_Y1]);
            vertices[1].uv_ = Vector2(region->uvs[SP_VERTEX_X2], region->uvs[SP_VERTEX_Y2]);
            vertices[2].uv_ = Vector2(region->uvs[SP_VERTEX_X3], region->uvs[SP_VERTEX_Y3]);
            vertices[3].uv_ = Vector2(region->uvs[SP_VERTEX_X4], region->uvs[SP_VERTEX_Y4]);

            sourceBatches_[0].vertices_.Push(vertices[0]);
            sourceBatches_[0].vertices_.Push(vertices[1]);
            sourceBatches_[0].vertices_.Push(vertices[2]);
            sourceBatches_[0].vertices_.Push(vertices[3]);
        }
        else if (attachment->type == SP_ATTACHMENT_MESH)
        {
            spMeshAttachment* mesh = (spMeshAttachment*)attachment;
            if (mesh->verticesCount > SLOT_VERTEX_COUNT_MAX)
                continue;

            spMeshAttachment_computeWorldVertices(mesh, slot, slotVertices);

            Vertex2D vertex;
            vertex.color_ = color;
            for (int j = 0; j < mesh->trianglesCount; ++j)
            {
                int index = mesh->triangles[j] << 1;
                vertex.position_ = worldTransform * Vector3(slotVertices[index], slotVertices[index + 1]);
                vertex.uv_ = Vector2(mesh->uvs[index], mesh->uvs[index + 1]);

                sourceBatches_[0].vertices_.Push(vertex);
                // Add padding vertex
                if (j % 3 == 2)
                    sourceBatches_[0].vertices_.Push(vertex);
            }
        }

        else if (attachment->type == SP_ATTACHMENT_SKINNED_MESH)
        {
            spSkinnedMeshAttachment* skinnedMesh = (spSkinnedMeshAttachment*)attachment;
            if (skinnedMesh->uvsCount > SLOT_VERTEX_COUNT_MAX)
                continue;

            spSkinnedMeshAttachment_computeWorldVertices(skinnedMesh, slot, slotVertices);

            Vertex2D vertex;
            vertex.color_ = color;
            for (int j = 0; j < skinnedMesh->trianglesCount; ++j)
            {
                int index = skinnedMesh->triangles[j] << 1;
                vertex.position_ = worldTransform * Vector3(slotVertices[index], slotVertices[index + 1]);
                vertex.uv_ = Vector2(skinnedMesh->uvs[index], skinnedMesh->uvs[index + 1]);

                sourceBatches_[0].vertices_.push_back(vertex);
                // Add padding vertex
                if (j % 3 == 2)
                    sourceBatches_[0].vertices_.push_back(vertex);
            }
        }
    }
}
#endif

void AnimatedSprite2D::SetSpriterAnimation()
{
    if (!spriterInstance_)
        spriterInstance_.reset(new Spriter::SpriterInstance(this,animationSet_->GetSpriterData()));

    // Use entity is empty first entity
    if (entity_.isEmpty())
        entity_ = animationSet_->GetSpriterData()->entities_[0]->name_;

    if (!spriterInstance_->SetEntity(qPrintable(entity_)))
    {
        URHO3D_LOGERROR("Set entity failed");
        return;
    }

    if (!spriterInstance_->SetAnimation(qPrintable(animationName_), (Spriter::LoopMode)loopMode_))
    {
        URHO3D_LOGERROR("Set animation failed");
        return;
    }

    UpdateAnimation(0.0f);
    MarkNetworkUpdate();
}

void AnimatedSprite2D::UpdateSpriterAnimation(float timeStep)
{
    spriterInstance_->Update(timeStep * speed_);
    sourceBatchesDirty_ = true;
    worldBoundingBoxDirty_ = true;
}

void AnimatedSprite2D::UpdateSourceBatchesSpriter()
{
    const Matrix3x4& nodeWorldTransform = GetNode()->GetWorldTransform();

    std::vector<Vertex2D>& vertices = sourceBatch_.front().vertices_;
    vertices.clear();

    Rect drawRect;
    Rect textureRect;
    unsigned color = color_.ToUInt();

    Vertex2D vertex0;
    Vertex2D vertex1;
    Vertex2D vertex2;
    Vertex2D vertex3;

    const std::vector<Spriter::SpatialTimelineKey*>& timelineKeys = spriterInstance_->GetTimelineKeys();
    for (auto key : timelineKeys)
    {
        if (key->GetObjectType() != Spriter::SPRITE)
            continue;

        Spriter::SpriteTimelineKey* timelineKey = (Spriter::SpriteTimelineKey*)key;

        Spriter::SpatialInfo& info = timelineKey->info_;
        Vector3 position(info.x_, info.y_, 0.0f);
        if (flipX_)
            position.x_ = -position.x_;
        if (flipY_)
            position.y_ = -position.y_;

        float angle = info.angle_;
        if (flipX_ != flipY_)
            angle = -angle;

        Matrix3x4 localTransform(position * PIXEL_SIZE,
                                 Quaternion(angle),
                                 Vector3(info.scaleX_, info.scaleY_, 1.0f));

        Matrix3x4 worldTransform = nodeWorldTransform * localTransform;
        Sprite2D* sprite = animationSet_->GetSpriterFileSprite(timelineKey->folderId_, timelineKey->fileId_);
        if (!sprite)
            return;

        if (timelineKey->useDefaultPivot_)
            sprite->GetDrawRectangle(drawRect, flipX_, flipY_);
        else
            sprite->GetDrawRectangle(drawRect, Vector2(timelineKey->pivotX_, timelineKey->pivotY_), flipX_, flipY_);

        if (!sprite->GetTextureRectangle(textureRect, flipX_, flipY_))
            return;

        vertex0.position_ = worldTransform * Vector3(drawRect.min_.x_, drawRect.min_.y_, 0.0f);
        vertex1.position_ = worldTransform * Vector3(drawRect.min_.x_, drawRect.max_.y_, 0.0f);
        vertex2.position_ = worldTransform * Vector3(drawRect.max_.x_, drawRect.max_.y_, 0.0f);
        vertex3.position_ = worldTransform * Vector3(drawRect.max_.x_, drawRect.min_.y_, 0.0f);

        vertex0.uv_ = textureRect.min_;
        vertex1.uv_ = Vector2(textureRect.min_.x_, textureRect.max_.y_);
        vertex2.uv_ = textureRect.max_;
        vertex3.uv_ = Vector2(textureRect.max_.x_, textureRect.min_.y_);

        vertex0.color_ = vertex1.color_ = vertex2.color_ = vertex3.color_ = color;

        vertices.push_back(vertex0);
        vertices.push_back(vertex1);
        vertices.push_back(vertex2);
        vertices.push_back(vertex3);
    }
}

void AnimatedSprite2D::Dispose()
{
#ifdef LUTEFISK3D_SPINE
    if (animationState_)
    {
        spAnimationState_dispose(animationState_);
        animationState_ = 0;
    }
    if (animationStateData_)
    {
        spAnimationStateData_dispose(animationStateData_);
        animationStateData_ = 0;
    }

    if (skeleton_)
    {
        spSkeleton_dispose(skeleton_);
        skeleton_ = 0;
    }
#endif
    spriterInstance_.reset();
}

}