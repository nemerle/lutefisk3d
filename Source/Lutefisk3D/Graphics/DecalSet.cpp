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

#include "DecalSet.h"

#include "AnimatedModel.h"
#include "Batch.h"
#include "Camera.h"
#include "Lutefisk3D/Core/Context.h"
#include "Geometry.h"
#include "Graphics.h"
#include "IndexBuffer.h"
#include "Lutefisk3D/IO/Log.h"
#include "Material.h"
#include "Lutefisk3D/IO/MemoryBuffer.h"
#include "Lutefisk3D/Scene/Node.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Scene/Scene.h"
#include "Lutefisk3D/Scene/SceneEvents.h"
#include "Tangent.h"
#include "Lutefisk3D/IO/VectorBuffer.h"
#include "VertexBuffer.h"

namespace Urho3D
{

extern const char* GEOMETRY_CATEGORY;

static const unsigned MIN_VERTICES = 4;
static const unsigned MIN_INDICES = 6;
static const unsigned MAX_VERTICES = 65536;
static const unsigned DEFAULT_MAX_VERTICES = 512;
static const unsigned DEFAULT_MAX_INDICES = 1024;
static const VertexMaskFlags STATIC_ELEMENT_MASK = MASK_POSITION | MASK_NORMAL | MASK_TEXCOORD1 | MASK_TANGENT;
static const VertexMaskFlags SKINNED_ELEMENT_MASK = MASK_POSITION | MASK_NORMAL | MASK_TEXCOORD1 | MASK_TANGENT | MASK_BLENDWEIGHTS |
    MASK_BLENDINDICES;

static DecalVertex ClipEdge(const DecalVertex& v0, const DecalVertex& v1, float d0, float d1, bool skinned)
{
    DecalVertex ret;
    float t = d0 / (d0 - d1);

    ret.position_ = v0.position_ + t * (v1.position_ - v0.position_);
    ret.normal_ = v0.normal_ + t * (v1.normal_ - v0.normal_);
    if (skinned)
    {
        if (*((unsigned*)v0.blendIndices_) != *((unsigned*)v1.blendIndices_))
        {
            // Blend weights and indices: if indices are different, choose the vertex nearer to the split plane
            const DecalVertex& src = Abs(d0) < Abs(d1) ? v0 : v1;
            for (unsigned i = 0; i < 4; ++i)
            {
                ret.blendWeights_[i] = src.blendWeights_[i];
                ret.blendIndices_[i] = src.blendIndices_[i];
            }
        }
        else
        {
            // If indices are same, can interpolate the weights
            for (unsigned i = 0; i < 4; ++i)
            {
                ret.blendWeights_[i] = v0.blendWeights_[i] + t * (v1.blendWeights_[i] - v0.blendWeights_[i]);
                ret.blendIndices_[i] = v0.blendIndices_[i];
            }
        }
    }

    return ret;
}

static void ClipPolygon(std::vector<DecalVertex>& dest, const std::vector<DecalVertex>& src, const Plane& plane, bool skinned)
{
    unsigned last = 0;
    float lastDistance = 0.0f;
    dest.clear();

    if (src.empty())
        return;

    for (unsigned i = 0; i < src.size(); ++i)
    {
        float distance = plane.Distance(src[i].position_);
        if (distance >= 0.0f)
        {
            if (lastDistance < 0.0f)
                dest.push_back(ClipEdge(src[last], src[i], lastDistance, distance, skinned));

            dest.push_back(src[i]);
        }
        else
        {
            if (lastDistance >= 0.0f && i != 0)
                dest.push_back(ClipEdge(src[last], src[i], lastDistance, distance, skinned));
        }

        last = i;
        lastDistance = distance;
    }

    // Recheck the distances of the last and first vertices and add the final clipped vertex if applicable
    float distance = plane.Distance(src[0].position_);
    if ((lastDistance < 0.0f && distance >= 0.0f) || (lastDistance >= 0.0f && distance < 0.0f))
        dest.push_back(ClipEdge(src[last], src[0], lastDistance, distance, skinned));
}

void Decal::AddVertex(const DecalVertex& vertex)
{
    for (unsigned i = 0; i < vertices_.size(); ++i)
    {
        if (vertex.position_.Equals(vertices_[i].position_) && vertex.normal_.Equals(vertices_[i].normal_))
        {
            indices_.push_back(i);
            return;
        }
    }

    unsigned short newIndex = vertices_.size();
    vertices_.push_back(vertex);
    indices_.push_back(newIndex);
}

void Decal::CalculateBoundingBox()
{
    boundingBox_.Clear();
    for (unsigned i = 0; i < vertices_.size(); ++i)
        boundingBox_.Merge(vertices_[i].position_);
}

DecalSet::DecalSet(Context* context) :
    Drawable(context, DRAWABLE_GEOMETRY),
    geometry_(new Geometry(context)),
    vertexBuffer_(new VertexBuffer(context_)),
    indexBuffer_(new IndexBuffer(context_)),
    numVertices_(0),
    numIndices_(0),
    maxVertices_(DEFAULT_MAX_VERTICES),
    maxIndices_(DEFAULT_MAX_INDICES),
    optimizeBufferSize_(false),
    skinned_(false),
    bufferDirty_(true),
    boundingBoxDirty_(true),
    skinningDirty_(false),
    assignBonesPending_(false),
    subscribed_(false)
{
    geometry_->SetIndexBuffer(indexBuffer_);

    batches_.resize(1);
    batches_[0].geometry_ = geometry_;
    batches_[0].geometryType_ = GEOM_STATIC_NOINSTANCING;
}

DecalSet::~DecalSet()
{
}

void DecalSet::RegisterObject(Context* context)
{
    context->RegisterFactory<DecalSet>(GEOMETRY_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Material", GetMaterialAttr, SetMaterialAttr, ResourceRef, {Material::GetTypeStatic()}, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Max Vertices", GetMaxVertices, SetMaxVertices, unsigned, DEFAULT_MAX_VERTICES, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Max Indices", GetMaxIndices, SetMaxIndices, unsigned, DEFAULT_MAX_INDICES, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Optimize Buffer Size", GetOptimizeBufferSize, SetOptimizeBufferSize, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Can Be Occluded", IsOccludee, SetOccludee, bool, true, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Distance", GetDrawDistance, SetDrawDistance, float, 0.0f, AM_DEFAULT);
    URHO3D_COPY_BASE_ATTRIBUTES(Drawable);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Decals", GetDecalsAttr, SetDecalsAttr, std::vector<unsigned char>, Variant::emptyBuffer, AM_FILE | AM_NOEDIT);
}

void DecalSet::ApplyAttributes()
{
    if (assignBonesPending_)
        AssignBoneNodes();
}

void DecalSet::OnSetEnabled()
{
    Drawable::OnSetEnabled();

    UpdateEventSubscription(true);
}

void DecalSet::ProcessRayQuery(const RayOctreeQuery& query, std::vector<RayQueryResult>& results)
{
    // Do not return raycast hits
}

void DecalSet::UpdateBatches(const FrameInfo& frame)
{
    const BoundingBox& worldBoundingBox = GetWorldBoundingBox();
    const Matrix3x4& worldTransform = node_->GetWorldTransform();
    distance_ = frame.camera_->GetDistance(worldBoundingBox.Center());

    float scale = worldBoundingBox.size().DotProduct(DOT_SCALE);
    lodDistance_ = frame.camera_->GetLodDistance(distance_, scale, lodBias_);

    batches_[0].distance_ = distance_;
    if (!skinned_)
        batches_[0].worldTransform_ = &worldTransform;
}

void DecalSet::UpdateGeometry(const FrameInfo& frame)
{

    if (bufferDirty_ || vertexBuffer_->IsDataLost() || indexBuffer_->IsDataLost())
        UpdateBuffers();

    if (skinningDirty_)
        UpdateSkinning();
}

UpdateGeometryType DecalSet::GetUpdateGeometryType()
{
    if (bufferDirty_ || vertexBuffer_->IsDataLost() || indexBuffer_->IsDataLost())
        return UPDATE_MAIN_THREAD;
    else if (skinningDirty_)
        return UPDATE_WORKER_THREAD;
    else
        return UPDATE_NONE;
}

void DecalSet::SetMaterial(Material* material)
{
    batches_[0].material_ = material;
    MarkNetworkUpdate();
}

void DecalSet::SetMaxVertices(unsigned num)
{
    // Never expand to 32 bit indices
    num = (unsigned)Clamp(num, MIN_VERTICES, MAX_VERTICES);

    if (num != maxVertices_)
    {
        if (!optimizeBufferSize_)
            bufferDirty_ = true;
        maxVertices_ = num;

        while (decals_.size() && numVertices_ > maxVertices_)
            RemoveDecals(1);

        MarkNetworkUpdate();
    }
}

void DecalSet::SetMaxIndices(unsigned num)
{
    if (num < MIN_INDICES)
        num = MIN_INDICES;

    if (num != maxIndices_)
    {
        if (!optimizeBufferSize_)
            bufferDirty_ = true;
        maxIndices_ = num;

        while (decals_.size() && numIndices_ > maxIndices_)
            RemoveDecals(1);
        MarkNetworkUpdate();
    }
}

void DecalSet::SetOptimizeBufferSize(bool enable)
{
    if (enable != optimizeBufferSize_)
    {
        optimizeBufferSize_ = enable;
        bufferDirty_ = true;

        MarkNetworkUpdate();
    }
}

bool DecalSet::AddDecal(Drawable* target, const Vector3& worldPosition, const Quaternion& worldRotation, float size,
    float aspectRatio, float depth, const Vector2& topLeftUV, const Vector2& bottomRightUV, float timeToLive, float normalCutoff,
    unsigned subGeometry)
{
    URHO3D_PROFILE(AddDecal);

    // Do not add decals in headless mode
    if (!node_ || !context_->m_Graphics)
        return false;

    if (!target || !target->GetNode())
    {
        URHO3D_LOGERROR("Null target drawable for decal");
        return false;
    }

    // Check for animated target and switch into skinned/static mode if necessary
    AnimatedModel* animatedModel = dynamic_cast<AnimatedModel*>(target);
    if ((animatedModel && !skinned_) || (!animatedModel && skinned_))
    {
        RemoveAllDecals();
        skinned_ = animatedModel != nullptr;
        bufferDirty_ = true;
    }

    // Center the decal frustum on the world position
    Vector3 adjustedWorldPosition = worldPosition - 0.5f * depth * (worldRotation * Vector3::FORWARD);
    /// \todo target transform is not right if adding a decal to StaticModelGroup
    Matrix3x4 targetTransform = target->GetNode()->GetWorldTransform().Inverse();

    // For an animated model, adjust the decal position back to the bind pose
    // To do this, need to find the bone the decal is colliding with
    if (animatedModel)
    {
        Skeleton& skeleton = animatedModel->GetSkeleton();
        unsigned numBones = skeleton.GetNumBones();
        Bone* bestBone = nullptr;
        float bestSize = 0.0f;

        for (unsigned i = 0; i < numBones; ++i)
        {
            Bone* bone = skeleton.GetBone(i);
            if (!bone->node_ || !bone->collisionMask_)
                continue;

            // Represent the decal as a sphere, try to find the biggest colliding bone
            Sphere decalSphere(bone->node_->GetWorldTransform().Inverse() * worldPosition, 0.5f * size /
                bone->node_->GetWorldScale().Length());

            if (bone->collisionMask_ & BONECOLLISION_BOX)
            {
                float size = bone->boundingBox_.HalfSize().Length();
                if (bone->boundingBox_.IsInside(decalSphere) != OUTSIDE && size > bestSize)
                {
                    bestBone = bone;
                    bestSize = size;
                }
            }
            else if (bone->collisionMask_ & BONECOLLISION_SPHERE)
            {
                Sphere boneSphere(Vector3::ZERO, bone->radius_);
                float size = bone->radius_;
                if (boneSphere.IsInside(decalSphere) && size > bestSize)
                {
                    bestBone = bone;
                    bestSize = size;
                }
            }
        }

        if (bestBone)
            targetTransform = (bestBone->node_->GetWorldTransform() * bestBone->offsetMatrix_).Inverse();
    }

    // Build the decal frustum
    Frustum decalFrustum;
    Matrix3x4 frustumTransform = targetTransform * Matrix3x4(adjustedWorldPosition, worldRotation, 1.0f);
    decalFrustum.DefineOrtho(size, aspectRatio, 1.0, 0.0f, depth, frustumTransform);

    Vector3 decalNormal = (targetTransform * Vector4(worldRotation * Vector3::BACK, 0.0f)).Normalized();

    decals_.emplace_back();
    Decal& newDecal(decals_.back());
    newDecal.timeToLive_ = timeToLive;

    std::vector<std::vector<DecalVertex> > faces;
    std::vector<DecalVertex> tempFace;

    unsigned numBatches = target->GetBatches().size();

    // Use either a specified subgeometry in the target, or all
    if (subGeometry < numBatches)
        GetFaces(faces, target, subGeometry, decalFrustum, decalNormal, normalCutoff);
    else
    {
        for (unsigned i = 0; i < numBatches; ++i)
            GetFaces(faces, target, i, decalFrustum, decalNormal, normalCutoff);
    }

    // Clip the acquired faces against all frustum planes
    for (unsigned i = 0; i < NUM_FRUSTUM_PLANES; ++i)
    {
        for (unsigned j = 0; j < faces.size(); ++j)
        {
            std::vector<DecalVertex>& face = faces[j];
            if (face.empty())
                continue;

            ClipPolygon(tempFace, face, decalFrustum.planes_[i], skinned_);
            face = tempFace;
        }
    }

    // Now triangulate the resulting faces into decal vertices
    for (unsigned i = 0; i < faces.size(); ++i)
    {
        std::vector<DecalVertex>& face = faces[i];
        if (face.size() < 3)
            continue;

        for (unsigned j = 2; j < face.size(); ++j)
        {
            newDecal.AddVertex(face[0]);
            newDecal.AddVertex(face[j - 1]);
            newDecal.AddVertex(face[j]);
        }
    }

    // Check if resulted in no triangles
    if (newDecal.vertices_.empty())
    {
        decals_.pop_back();
        return true;
    }

    if (newDecal.vertices_.size() > maxVertices_)
    {
        URHO3D_LOGWARNING(QString("Can not add decal, vertex count %1 exceeds maximum %2")
                .arg(newDecal.vertices_.size())
                .arg(maxVertices_));
        decals_.pop_back();
        return false;
    }
    if (newDecal.indices_.size() > maxIndices_)
    {
        URHO3D_LOGWARNING(QString("Can not add decal, index count %1 exceeds maximum %2")
                   .arg(newDecal.indices_.size()).arg(maxIndices_));
        decals_.pop_back();
        return false;
    }

    // Calculate UVs
    Matrix4 projection(Matrix4::ZERO);
    projection.m11_ = (1.0f / (size * 0.5f));
    projection.m00_ = projection.m11_ / aspectRatio;
    projection.m22_ = 1.0f / depth;
    projection.m33_ = 1.0f;

    CalculateUVs(newDecal, frustumTransform.Inverse(), projection, topLeftUV, bottomRightUV);

    // Transform vertices to this node's local space and generate tangents
    Matrix3x4 decalTransform = node_->GetWorldTransform().Inverse() * target->GetNode()->GetWorldTransform();
    TransformVertices(newDecal, skinned_ ? Matrix3x4::IDENTITY : decalTransform);
    GenerateTangents(&newDecal.vertices_[0], sizeof(DecalVertex), &newDecal.indices_[0], sizeof(unsigned short), 0,
        newDecal.indices_.size(), offsetof(DecalVertex, normal_), offsetof(DecalVertex, texCoord_), offsetof(DecalVertex,
        tangent_));

    newDecal.CalculateBoundingBox();
    numVertices_ += newDecal.vertices_.size();
    numIndices_ += newDecal.indices_.size();

    // Remove oldest decals if total vertices exceeded
    while (decals_.size() && (numVertices_ > maxVertices_ || numIndices_ > maxIndices_))
        RemoveDecals(1);

    URHO3D_LOGDEBUG("Added decal with " + QString::number(newDecal.vertices_.size()) + " vertices");

    // If new decal is time limited, subscribe to scene post-update
    if (newDecal.timeToLive_ > 0.0f && !subscribed_)
        UpdateEventSubscription(false);

    MarkDecalsDirty();
    return true;
}

void DecalSet::RemoveDecals(unsigned num)
{
    while (num-- && decals_.size())
        RemoveDecal(decals_.begin());
}

void DecalSet::RemoveAllDecals()
{
    if (!decals_.empty())
    {
        decals_.clear();
        numVertices_ = 0;
        numIndices_ = 0;
        MarkDecalsDirty();
    }

    // Remove all bones and skinning matrices and stop listening to the bone nodes
    for (Bone & elem : bones_)
    {
        if (elem.node_)
            elem.node_->RemoveListener(this);
    }

    bones_.clear();
    skinMatrices_.clear();
    UpdateBatch();
}

Material* DecalSet::GetMaterial() const
{
    return batches_[0].material_;
}

void DecalSet::SetMaterialAttr(const ResourceRef& value)
{
    ResourceCache* cache =context_->resourceCache();
    SetMaterial(cache->GetResource<Material>(value.name_));
}

void DecalSet::SetDecalsAttr(const std::vector<unsigned char>& value)
{
    RemoveAllDecals();

    if (value.empty())
        return;

    MemoryBuffer buffer(value);

    skinned_ = buffer.ReadBool();
    unsigned numDecals = buffer.ReadVLE();

    while (numDecals--)
    {
        decals_.emplace_back();
        Decal& newDecal = decals_.back();

        newDecal.timer_ = buffer.ReadFloat();
        newDecal.timeToLive_ = buffer.ReadFloat();
        newDecal.vertices_.resize(buffer.ReadVLE());
        newDecal.indices_.resize(buffer.ReadVLE());

        for (DecalVertex & elem : newDecal.vertices_)
        {
            elem.position_ = buffer.ReadVector3();
            elem.normal_ = buffer.ReadVector3();
            elem.texCoord_ = buffer.ReadVector2();
            elem.tangent_ = buffer.ReadVector4();
            if (skinned_)
            {
                for (unsigned j = 0; j < 4; ++j)
                    elem.blendWeights_[j] = buffer.ReadFloat();
                for (unsigned j = 0; j < 4; ++j)
                    elem.blendIndices_[j] = buffer.ReadUByte();
            }
        }

        for (auto & elem : newDecal.indices_)
            elem = buffer.ReadUShort();

        newDecal.CalculateBoundingBox();
        numVertices_ += newDecal.vertices_.size();
        numIndices_ += newDecal.indices_.size();
    }

    if (skinned_)
    {
        unsigned numBones = buffer.ReadVLE();
        skinMatrices_.resize(numBones);
        bones_.resize(numBones);

        for (unsigned i = 0; i < numBones; ++i)
        {
            Bone& newBone = bones_[i];

            newBone.name_ = buffer.ReadString();
            newBone.collisionMask_ = buffer.ReadUByte();
            if (newBone.collisionMask_ & BONECOLLISION_SPHERE)
                newBone.radius_ = buffer.ReadFloat();
            if (newBone.collisionMask_ & BONECOLLISION_BOX)
                newBone.boundingBox_ = buffer.ReadBoundingBox();
            buffer.Read(&newBone.offsetMatrix_.m00_, sizeof(Matrix3x4));
        }

        assignBonesPending_ = true;
        skinningDirty_ = true;
    }

    UpdateEventSubscription(true);
    UpdateBatch();
    MarkDecalsDirty();
}

ResourceRef DecalSet::GetMaterialAttr() const
{
    return GetResourceRef(batches_[0].material_, Material::GetTypeStatic());
}

std::vector<unsigned char> DecalSet::GetDecalsAttr() const
{
    VectorBuffer ret;

    ret.WriteBool(skinned_);
    ret.WriteVLE(decals_.size());

    for (const Decal & _i : decals_)
    {
        ret.WriteFloat(_i.timer_);
        ret.WriteFloat(_i.timeToLive_);
        ret.WriteVLE(_i.vertices_.size());
        ret.WriteVLE(_i.indices_.size());

        for (const DecalVertex & elem : _i.vertices_)
        {
            ret.WriteVector3(elem.position_);
            ret.WriteVector3(elem.normal_);
            ret.WriteVector2(elem.texCoord_);
            ret.WriteVector4(elem.tangent_);
            if (skinned_)
            {
                for (unsigned k = 0; k < 4; ++k)
                    ret.WriteFloat(elem.blendWeights_[k]);
                for (unsigned k = 0; k < 4; ++k)
                    ret.WriteUByte(elem.blendIndices_[k]);
            }
        }

        for (const auto & elem : _i.indices_)
            ret.WriteUShort(elem);
    }

    if (skinned_)
    {
        ret.WriteVLE(bones_.size());

        for (const Bone & elem : bones_)
        {
            ret.WriteString(elem.name_);
            ret.WriteUByte(elem.collisionMask_);
            if (elem.collisionMask_ & BONECOLLISION_SPHERE)
                ret.WriteFloat(elem.radius_);
            if (elem.collisionMask_ & BONECOLLISION_BOX)
                ret.WriteBoundingBox(elem.boundingBox_);
            ret.Write(elem.offsetMatrix_.Data(), sizeof(Matrix3x4));
        }
    }

    return ret.GetBuffer();
}

void DecalSet::OnMarkedDirty(Node* node)
{
    Drawable::OnMarkedDirty(node);

    if (skinned_)
    {
        // If the scene node or any of the bone nodes move, mark skinning dirty
        skinningDirty_ = true;
    }
}

void DecalSet::OnWorldBoundingBoxUpdate()
{
    if (!skinned_)
    {
        if (boundingBoxDirty_)
            CalculateBoundingBox();

        worldBoundingBox_ = boundingBox_.Transformed(node_->GetWorldTransform());
    }
    else
    {
        // When using skinning, update world bounding box based on the bones
        BoundingBox worldBox;

        for (std::vector<Bone>::const_iterator i = bones_.begin(); i != bones_.end(); ++i)
        {
            Node* boneNode = i->node_;
            if (!boneNode)
                continue;

            // Use hitbox if available. If not, use only half of the sphere radius
            /// \todo The sphere radius should be multiplied with bone scale
            if (i->collisionMask_ & BONECOLLISION_BOX)
                worldBox.Merge(i->boundingBox_.Transformed(boneNode->GetWorldTransform()));
            else if (i->collisionMask_ & BONECOLLISION_SPHERE)
                worldBox.Merge(Sphere(boneNode->GetWorldPosition(), i->radius_ * 0.5f));
        }

        worldBoundingBox_ = worldBox;
    }
}

void DecalSet::GetFaces(std::vector<std::vector<DecalVertex> >& faces, Drawable* target, unsigned batchIndex, const Frustum& frustum,
    const Vector3& decalNormal, float normalCutoff)
{
    // Try to use the most accurate LOD level if possible
    Geometry* geometry = target->GetLodGeometry(batchIndex, 0);
    if (!geometry || geometry->GetPrimitiveType() != TRIANGLE_LIST)
        return;

    const unsigned char* positionData = nullptr;
    const unsigned char* normalData = nullptr;
    const unsigned char* skinningData = nullptr;
    const unsigned char* indexData = nullptr;
    unsigned positionStride = 0;
    unsigned normalStride = 0;
    unsigned skinningStride = 0;
    unsigned indexStride = 0;

    IndexBuffer* ib = geometry->GetIndexBuffer();
    if (ib)
    {
        indexData = ib->GetShadowData();
        indexStride = ib->GetIndexSize();
    }

    // For morphed models positions, normals and skinning may be in different buffers
    for (unsigned i = 0; i < geometry->GetNumVertexBuffers(); ++i)
    {
        VertexBuffer* vb = geometry->GetVertexBuffer(i);
        if (!vb)
            continue;

        unsigned elementMask = vb->GetElementMask();
        unsigned char* data = vb->GetShadowData();
        if (!data)
            continue;

        if (elementMask & MASK_POSITION)
        {
            positionData = data;
            positionStride = vb->GetVertexSize();
        }
        if (elementMask & MASK_NORMAL)
        {
            normalData = data + vb->GetElementOffset(SEM_NORMAL);
            normalStride = vb->GetVertexSize();
        }
        if (elementMask & MASK_BLENDWEIGHTS)
        {
            skinningData = data + vb->GetElementOffset(SEM_BLENDWEIGHTS);
            skinningStride = vb->GetVertexSize();
        }
    }

    // Positions and indices are needed
    if (!positionData)
    {
        // As a fallback, try to get the geometry's raw vertex/index data
        const std::vector<VertexElement>* elements;
        geometry->GetRawData(positionData, positionStride, indexData, indexStride, elements);
        if (!positionData)
        {
            URHO3D_LOGWARNING("Can not add decal, target drawable has no CPU-side geometry data");
            return;
        }
    }

    if (indexData)
    {
        unsigned indexStart = geometry->GetIndexStart();
        unsigned indexCount = geometry->GetIndexCount();

        // 16-bit indices
        if (indexStride == sizeof(unsigned short))
        {
            const unsigned short* indices = ((const unsigned short*)indexData) + indexStart;
            const unsigned short* indicesEnd = indices + indexCount;

            while (indices < indicesEnd)
            {
                GetFace(faces, target, batchIndex, indices[0], indices[1], indices[2], positionData, normalData, skinningData,
                    positionStride, normalStride, skinningStride, frustum, decalNormal, normalCutoff);
                indices += 3;
            }
        }
        else
        // 32-bit indices
        {
            const unsigned* indices = ((const unsigned*)indexData) + indexStart;
            const unsigned* indicesEnd = indices + indexCount;

            while (indices < indicesEnd)
            {
                GetFace(faces, target, batchIndex, indices[0], indices[1], indices[2], positionData, normalData, skinningData,
                    positionStride, normalStride, skinningStride, frustum, decalNormal, normalCutoff);
                indices += 3;
            }
        }
    }
    else
    {
        // Non-indexed geometry
        unsigned indices = geometry->GetVertexStart();
        unsigned indicesEnd = indices + geometry->GetVertexCount();

        while (indices + 2 < indicesEnd)
        {
            GetFace(faces, target, batchIndex, indices, indices + 1, indices + 2, positionData, normalData, skinningData,
                positionStride, normalStride, skinningStride, frustum, decalNormal, normalCutoff);
            indices += 3;
        }
    }
}

void DecalSet::GetFace(std::vector<std::vector<DecalVertex> >& faces, Drawable* target, unsigned batchIndex, unsigned i0, unsigned i1,
    unsigned i2, const unsigned char* positionData, const unsigned char* normalData, const unsigned char* skinningData,
    unsigned positionStride, unsigned normalStride, unsigned skinningStride, const Frustum& frustum, const Vector3& decalNormal,
    float normalCutoff)
{
    bool hasNormals = normalData != nullptr;
    bool hasSkinning = skinned_ && skinningData != nullptr;

    const Vector3& v0 = *((const Vector3*)(&positionData[i0 * positionStride]));
    const Vector3& v1 = *((const Vector3*)(&positionData[i1 * positionStride]));
    const Vector3& v2 = *((const Vector3*)(&positionData[i2 * positionStride]));

    // Calculate unsmoothed face normals if no normal data
    Vector3 faceNormal = Vector3::ZERO;
    if (!hasNormals)
    {
        Vector3 dist1 = v1 - v0;
        Vector3 dist2 = v2 - v0;
        faceNormal = (dist1.CrossProduct(dist2)).Normalized();
    }

    const Vector3& n0 = hasNormals ? *((const Vector3*)(&normalData[i0 * normalStride])) : faceNormal;
    const Vector3& n1 = hasNormals ? *((const Vector3*)(&normalData[i1 * normalStride])) : faceNormal;
    const Vector3& n2 = hasNormals ? *((const Vector3*)(&normalData[i2 * normalStride])) : faceNormal;

    const unsigned char* s0 = hasSkinning ? &skinningData[i0 * skinningStride] : nullptr;
    const unsigned char* s1 = hasSkinning ? &skinningData[i1 * skinningStride] : nullptr;
    const unsigned char* s2 = hasSkinning ? &skinningData[i2 * skinningStride] : nullptr;

    // Check if face is too much away from the decal normal
    if (decalNormal.DotProduct((n0 + n1 + n2) / 3.0f) < normalCutoff)
        return;

    // Check if face is culled completely by any of the planes
    for (const Plane& plane : frustum.planes_)
    {
        if (plane.Distance(v0) < 0.0f && plane.Distance(v1) < 0.0f && plane.Distance(v2) < 0.0f)
            return;
    }

    faces.resize(faces.size() + 1);
    std::vector<DecalVertex>& face = faces.back();
    if (!hasSkinning)
    {
        face.reserve(3);
        face.push_back(DecalVertex(v0, n0));
        face.push_back(DecalVertex(v1, n1));
        face.push_back(DecalVertex(v2, n2));
    }
    else
    {
        const float* bw0 = (const float*)s0;
        const float* bw1 = (const float*)s1;
        const float* bw2 = (const float*)s2;
        const unsigned char* bi0 = s0 + sizeof(float) * 4;
        const unsigned char* bi1 = s1 + sizeof(float) * 4;
        const unsigned char* bi2 = s2 + sizeof(float) * 4;
        unsigned char nbi0[4];
        unsigned char nbi1[4];
        unsigned char nbi2[4];

        // Make sure all bones are found and that there is room in the skinning matrices
        if (!GetBones(target, batchIndex, bw0, bi0, nbi0) || !GetBones(target, batchIndex, bw1, bi1, nbi1) ||
            !GetBones(target, batchIndex, bw2, bi2, nbi2))
            return;

        face.reserve(3);
        face.push_back(DecalVertex(v0, n0, bw0, nbi0));
        face.push_back(DecalVertex(v1, n1, bw1, nbi1));
        face.push_back(DecalVertex(v2, n2, bw2, nbi2));
    }
}

bool DecalSet::GetBones(Drawable* target, unsigned batchIndex, const float* blendWeights, const unsigned char* blendIndices,
    unsigned char* newBlendIndices)
{
    AnimatedModel* animatedModel = dynamic_cast<AnimatedModel*>(target);
    if (!animatedModel)
        return false;

    // Check whether target is using global or per-geometry skinning
    const std::vector<std::vector<Matrix3x4> >& geometrySkinMatrices = animatedModel->GetGeometrySkinMatrices();
    const std::vector<std::vector<unsigned> >& geometryBoneMappings = animatedModel->GetGeometryBoneMappings();

    for (unsigned i = 0; i < 4; ++i)
    {
        if (blendWeights[i] <= 0.0f) {
            newBlendIndices[i] = 0;
            continue;
        }

        Bone* bone = nullptr;
        if (geometrySkinMatrices.empty())
            bone = animatedModel->GetSkeleton().GetBone(blendIndices[i]);
        else if (blendIndices[i] < geometryBoneMappings[batchIndex].size())
            bone = animatedModel->GetSkeleton().GetBone(geometryBoneMappings[batchIndex][blendIndices[i]]);

        if (!bone)
        {
            URHO3D_LOGWARNING("Out of range bone index for skinned decal");
            return false;
        }

        bool found = false;
        unsigned index;

        for (index = 0; index < bones_.size(); ++index)
        {
            if (bones_[index].node_ == bone->node_)
            {
                // Check also that the offset matrix matches, in case we for example have a separate attachment AnimatedModel
                // with a different bind pose
                if (bones_[index].offsetMatrix_.Equals(bone->offsetMatrix_))
                {
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            if (bones_.size() >= Graphics::GetMaxBones())
            {
                URHO3D_LOGWARNING("Maximum skinned decal bone count reached");
                return false;
            }

            // Copy the bone from the model to the decal
            index = bones_.size();
            bones_.resize(bones_.size() + 1);
            bones_[index] = *bone;
            skinMatrices_.resize(skinMatrices_.size() + 1);
            skinningDirty_ = true;

            // Start listening to bone transform changes to update skinning
            bone->node_->AddListener(this);
        }

        newBlendIndices[i] = (unsigned char)index;
    }

    // Update amount of shader data in the decal batch
    UpdateBatch();
    return true;
}

void DecalSet::CalculateUVs(Decal& decal, const Matrix3x4& view, const Matrix4& projection, const Vector2& topLeftUV,
    const Vector2& bottomRightUV)
{
    Matrix4 viewProj = projection * view;

    for (DecalVertex & elem : decal.vertices_)
    {
        Vector3 projected = viewProj * elem.position_;
        elem.texCoord_ = Vector2(
            Lerp(topLeftUV.x_, bottomRightUV.x_, projected.x_ * 0.5f + 0.5f),
            Lerp(bottomRightUV.y_, topLeftUV.y_, projected.y_ * 0.5f + 0.5f)
        );
    }
}

void DecalSet::TransformVertices(Decal& decal, const Matrix3x4& transform)
{
    for (DecalVertex & elem : decal.vertices_)
    {
        elem.position_ = transform * elem.position_;
        elem.normal_ = (transform * Vector4(elem.normal_, 0.0f)).Normalized();
    }
}

std::deque<Decal>::iterator DecalSet::RemoveDecal(std::deque<Decal>::iterator i)
{
    numVertices_ -= i->vertices_.size();
    numIndices_ -= i->indices_.size();
    MarkDecalsDirty();
    return decals_.erase(i);
}

void DecalSet::MarkDecalsDirty()
{
    if (!boundingBoxDirty_)
    {
        boundingBoxDirty_ = true;
        OnMarkedDirty(node_);
    }
    bufferDirty_ = true;
}

void DecalSet::CalculateBoundingBox()
{
    boundingBox_.Clear();
    for (std::deque<Decal>::const_iterator i = decals_.begin(); i != decals_.end(); ++i)
        boundingBox_.Merge(i->boundingBox_);

    boundingBoxDirty_ = false;
}


void DecalSet::UpdateBuffers()
{
    VertexMaskFlags newElementMask = skinned_ ? SKINNED_ELEMENT_MASK : STATIC_ELEMENT_MASK;
    unsigned newVBSize = optimizeBufferSize_ ? numVertices_ : maxVertices_;
    unsigned newIBSize = optimizeBufferSize_ ? numIndices_ : maxIndices_;

    if (vertexBuffer_->GetElementMask() != newElementMask || vertexBuffer_->GetVertexCount() != newVBSize)
        vertexBuffer_->SetSize(newVBSize, newElementMask);
    if (indexBuffer_->GetIndexCount() != newIBSize)
        indexBuffer_->SetSize(newIBSize, false);
    geometry_->SetVertexBuffer(0, vertexBuffer_);
    geometry_->SetDrawRange(TRIANGLE_LIST, 0, numIndices_, 0, numVertices_);

    float* vertices = numVertices_ ? (float*)vertexBuffer_->Lock(0, numVertices_) : nullptr;
    unsigned short* indices = numIndices_ ? (unsigned short*)indexBuffer_->Lock(0, numIndices_) : nullptr;

    if (vertices && indices)
    {
        unsigned short indexStart = 0;

        for (const Decal &d : decals_)
        {
            for (unsigned j = 0; j < d.vertices_.size(); ++j)
            {
                const DecalVertex& vertex = d.vertices_[j];
                *vertices++ = vertex.position_.x_;
                *vertices++ = vertex.position_.y_;
                *vertices++ = vertex.position_.z_;
                *vertices++ = vertex.normal_.x_;
                *vertices++ = vertex.normal_.y_;
                *vertices++ = vertex.normal_.z_;
                *vertices++ = vertex.texCoord_.x_;
                *vertices++ = vertex.texCoord_.y_;
                *vertices++ = vertex.tangent_.x_;
                *vertices++ = vertex.tangent_.y_;
                *vertices++ = vertex.tangent_.z_;
                *vertices++ = vertex.tangent_.w_;
                if (skinned_)
                {
                    *vertices++ = vertex.blendWeights_[0];
                    *vertices++ = vertex.blendWeights_[1];
                    *vertices++ = vertex.blendWeights_[2];
                    *vertices++ = vertex.blendWeights_[3];
                    *vertices++ = *((float*)vertex.blendIndices_);
                }
            }

            for (unsigned j = 0; j < d.indices_.size(); ++j)
                *indices++ = d.indices_[j] + indexStart;

            indexStart += d.vertices_.size();
        }
    }

    vertexBuffer_->Unlock();
    vertexBuffer_->ClearDataLost();
    indexBuffer_->Unlock();
    indexBuffer_->ClearDataLost();
    bufferDirty_ = false;
}

void DecalSet::UpdateSkinning()
{
    // Use model's world transform in case a bone is missing
    const Matrix3x4& worldTransform = node_->GetWorldTransform();

    for (unsigned i = 0; i < bones_.size(); ++i)
    {
        const Bone& bone = bones_[i];
        if (bone.node_)
            skinMatrices_[i] = bone.node_->GetWorldTransform() * bone.offsetMatrix_;
        else
            skinMatrices_[i] = worldTransform;
    }

    skinningDirty_ = false;
}

void DecalSet::UpdateBatch()
{
    if (skinMatrices_.size())
    {
        batches_[0].geometryType_ = GEOM_SKINNED;
        batches_[0].worldTransform_ = &skinMatrices_[0];
        batches_[0].numWorldTransforms_ = skinMatrices_.size();
    }
    else
    {
        batches_[0].geometryType_ = GEOM_STATIC;
        batches_[0].worldTransform_ = &node_->GetWorldTransform();
        batches_[0].numWorldTransforms_ = 1;
    }
}

void DecalSet::AssignBoneNodes()
{
    assignBonesPending_ = false;

    if (!node_)
        return;

    // Find the bone nodes from the node hierarchy and add listeners
    for (Bone & elem : bones_)
    {
        Node* boneNode = node_->GetChild(elem.name_, true);
        if (boneNode)
            boneNode->AddListener(this);
        elem.node_ = boneNode;
    }
}

void DecalSet::UpdateEventSubscription(bool checkAllDecals)
{
    Scene* scene = GetScene();
    if (!scene)
        return;

    bool enabled = IsEnabledEffective();

    if (enabled && checkAllDecals)
    {
        bool hasTimeLimitedDecals = false;

        for (std::deque<Decal>::const_iterator i = decals_.begin(); i != decals_.end(); ++i)
        {
            if (i->timeToLive_ > 0.0f)
            {
                hasTimeLimitedDecals = true;
                break;
            }
        }

        // If no time limited decals, no need to subscribe to scene update
        enabled = hasTimeLimitedDecals;
    }

    if (enabled && !subscribed_)
    {
        scene->scenePostUpdate.Connect(this,&DecalSet::HandleScenePostUpdate);
        subscribed_ = true;
    }
    else if (!enabled && subscribed_)
    {
        scene->scenePostUpdate.Disconnect(this,&DecalSet::HandleScenePostUpdate);
        subscribed_ = false;
    }
}

void DecalSet::HandleScenePostUpdate(Scene *,float timeStep)
{
    for (std::deque<Decal>::iterator i = decals_.begin(); i != decals_.end();)
    {
        i->timer_ += timeStep;

        // Remove the decal if time to live expired
        if (i->timeToLive_ > 0.0f && i->timer_ > i->timeToLive_)
            i = RemoveDecal(i);
        else
            ++i;
    }
}

}
