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

#include "Serializable.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/Deserializer.h"
#include "Lutefisk3D/IO/Log.h"
#include "ReplicationState.h"
#include "SceneEvents.h"
#include "Lutefisk3D/IO/Serializer.h"
#include "Lutefisk3D/Resource/XMLElement.h"
#include "Lutefisk3D/Resource/Resource.h"
#include "Lutefisk3D/Resource/JSONValue.h"

namespace Urho3D
{

static unsigned RemapAttributeIndex(const std::vector<AttributeInfo>* attributes, const AttributeInfo& netAttr, unsigned netAttrIndex)
{
    if (attributes == nullptr)
        return netAttrIndex; // Could not remap

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        // Compare accessor to avoid name string compare
        if (attr.accessor_.Get() && attr.accessor_.Get() == netAttr.accessor_.Get())
            return i;
    }

    return netAttrIndex; // Could not remap
}
Serializable::Serializable(Context* context) :
    Object(context),
    setInstanceDefault_(false),
    temporary_(false)
{
}

Serializable::~Serializable()
{

}

void Serializable::OnSetAttribute(const AttributeInfo& attr, const Variant& src)
{
    // TODO: may be could use an observer pattern here
    if (setInstanceDefault_)
        SetInstanceDefault(attr.name_, src);
    // Check for accessor function mode
    if (attr.accessor_ != nullptr)
    {
        attr.accessor_->Set(this, src);
        return;
    }

    // Get the destination address
    assert(attr.ptr_);
    void* dest = attr.ptr_;

    switch (attr.type_)
    {
    case VAR_INT:
        // If enum type, use the low 8 bits only
        if (attr.enumNames_ != nullptr)
            *(reinterpret_cast<unsigned char*>(dest)) = src.GetInt();
        else
            *(reinterpret_cast<int*>(dest)) = src.GetInt();
        break;

    case VAR_INT64:
        *(reinterpret_cast<long long*>(dest)) = src.GetInt64();
        break;
    case VAR_BOOL:
        *(reinterpret_cast<bool*>(dest)) = src.GetBool();
        break;

    case VAR_FLOAT:
        *(reinterpret_cast<float*>(dest)) = src.GetFloat();
        break;

    case VAR_VECTOR2:
        *(reinterpret_cast<Vector2*>(dest)) = src.GetVector2();
        break;

    case VAR_VECTOR3:
        *(reinterpret_cast<Vector3*>(dest)) = src.GetVector3();
        break;

    case VAR_VECTOR4:
        *(reinterpret_cast<Vector4*>(dest)) = src.GetVector4();
        break;

    case VAR_QUATERNION:
        *(reinterpret_cast<Quaternion*>(dest)) = src.GetQuaternion();
        break;

    case VAR_COLOR:
        *(reinterpret_cast<Color*>(dest)) = src.GetColor();
        break;

    case VAR_STRING:
        *(reinterpret_cast<QString*>(dest)) = src.GetString();
        break;

    case VAR_BUFFER:
        *(reinterpret_cast<std::vector<unsigned char>*>(dest)) = src.GetBuffer();
        break;

    case VAR_RESOURCEREF:
        *(reinterpret_cast<ResourceRef*>(dest)) = src.GetResourceRef();
        break;

    case VAR_RESOURCEREFLIST:
        *(reinterpret_cast<ResourceRefList*>(dest)) = src.GetResourceRefList();
        break;

    case VAR_VARIANTVECTOR:
        *(reinterpret_cast<VariantVector*>(dest)) = src.GetVariantVector();
        break;

    case VAR_STRINGVECTOR:
        *(reinterpret_cast<QStringList *>(dest)) = src.GetStringVector();
        break;

    case VAR_VARIANTMAP:
        *(reinterpret_cast<VariantMap*>(dest)) = src.GetVariantMap();
        break;

    case VAR_INTRECT:
        *(reinterpret_cast<IntRect*>(dest)) = src.GetIntRect();
        break;

    case VAR_INTVECTOR2:
        *(reinterpret_cast<IntVector2*>(dest)) = src.GetIntVector2();
        break;

    case VAR_INTVECTOR3:
        *(reinterpret_cast<IntVector3*>(dest)) = src.GetIntVector3();
        break;

    case VAR_DOUBLE:
        *(reinterpret_cast<double*>(dest)) = src.GetDouble();
        break;
    default:
        URHO3D_LOGERROR("Unsupported attribute type for OnSetAttribute()");
        return;
    }

    // If it is a network attribute then mark it for next network update
    if (attr.mode_ & AM_NET)
        MarkNetworkUpdate();
}

void Serializable::OnGetAttribute(const AttributeInfo& attr, Variant& dest) const
{
    // Check for accessor function mode
    if (attr.accessor_)
    {
        attr.accessor_->Get(this, dest);
        return;
    }

    // Get the source address
    assert(attr.ptr_);
    const void* src = attr.ptr_;

    switch (attr.type_)
    {
    case VAR_INT:
        // If enum type, use the low 8 bits only
        if (attr.enumNames_)
            dest = *(reinterpret_cast<const unsigned char*>(src));
        else
            dest = *(reinterpret_cast<const int*>(src));
        break;

    case VAR_INT64:
        dest = *(reinterpret_cast<const int64_t*>(src));
        break;

    case VAR_BOOL:
        dest = *(reinterpret_cast<const bool*>(src));
        break;

    case VAR_FLOAT:
        dest = *(reinterpret_cast<const float*>(src));
        break;


    case VAR_VECTOR2:
        dest = *(reinterpret_cast<const Vector2*>(src));
        break;

    case VAR_VECTOR3:
        dest = *(reinterpret_cast<const Vector3*>(src));
        break;

    case VAR_VECTOR4:
        dest = *(reinterpret_cast<const Vector4*>(src));
        break;

    case VAR_QUATERNION:
        dest = *(reinterpret_cast<const Quaternion*>(src));
        break;

    case VAR_COLOR:
        dest = *(reinterpret_cast<const Color*>(src));
        break;

    case VAR_STRING:
        dest = *(reinterpret_cast<const QString*>(src));
        break;

    case VAR_BUFFER:
        dest = *(reinterpret_cast<const std::vector<unsigned char>*>(src));
        break;

    case VAR_RESOURCEREF:
        dest = *(reinterpret_cast<const ResourceRef*>(src));
        break;

    case VAR_RESOURCEREFLIST:
        dest = *(reinterpret_cast<const ResourceRefList*>(src));
        break;

    case VAR_VARIANTVECTOR:
        dest = *(reinterpret_cast<const VariantVector*>(src));
        break;

    case VAR_STRINGVECTOR:
        dest = *(reinterpret_cast<const QStringList *>(src));
        break;

    case VAR_VARIANTMAP:
        dest = *(reinterpret_cast<const VariantMap*>(src));
        break;

    case VAR_INTRECT:
        dest = *(reinterpret_cast<const IntRect*>(src));
        break;

    case VAR_INTVECTOR2:
        dest = *(reinterpret_cast<const IntVector2*>(src));
        break;

    case VAR_INTVECTOR3:
        dest = *(reinterpret_cast<const IntVector3*>(src));
        break;

    case VAR_DOUBLE:
        dest = *(reinterpret_cast<const double*>(src));
        break;

    default:
        URHO3D_LOGERROR("Unsupported attribute type for OnGetAttribute()");
        return;
    }
}

const std::vector<AttributeInfo>* Serializable::GetAttributes() const
{
    return context_->GetAttributes(GetType());
}

const std::vector<AttributeInfo>* Serializable::GetNetworkAttributes() const
{
    return networkState_ != nullptr ? networkState_->attributes_ : context_->GetNetworkAttributes(GetType());
}

bool Serializable::Load(Deserializer& source)
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
        return true;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if ((attr.mode_ & AM_FILE) == 0u)
            continue;

        if (source.IsEof())
        {
            URHO3D_LOGERROR("Could not load " + GetTypeName() + ", stream not open or at end");
            return false;
        }

        Variant varValue = source.ReadVariant(attr.type_);
        OnSetAttribute(attr, varValue);

    }

    return true;
}

bool Serializable::Save(Serializer& dest) const
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
        return true;

    Variant value;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (((attr.mode_ & AM_FILE) == 0u) || (attr.mode_ & AM_FILEREADONLY) == AM_FILEREADONLY)
            continue;

        OnGetAttribute(attr, value);

        if (!dest.WriteVariantData(value))
        {
            URHO3D_LOGERROR("Could not save " + GetTypeName() + ", writing to stream failed");
            return false;
        }
    }

    return true;
}

bool Serializable::LoadXML(const XMLElement& source)
{
    if (source.IsNull())
    {
        URHO3D_LOGERROR("Could not load " + GetTypeName() + ", null source element");
        return false;
    }

    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
        return true;

    XMLElement attrElem = source.GetChild("attribute");
    unsigned startIndex = 0;

    while (attrElem)
    {
        QString name = attrElem.GetAttribute("name");
        unsigned i = startIndex;
        unsigned attempts = attributes->size();

        while (attempts != 0u)
        {
            const AttributeInfo& attr = attributes->at(i);
            if (((attr.mode_ & AM_FILE) != 0u) && (attr.name_.compare(name) == 0))
            {
                Variant varValue;

                // If enums specified, do enum lookup and int assignment. Otherwise assign the variant directly
                if (attr.enumNames_ != nullptr)
                {
                    QString value = attrElem.GetAttribute("value");
                    bool enumFound = false;
                    int enumValue = 0;
                    const char*const* enumPtr = attr.enumNames_;
                    while (*enumPtr != nullptr)
                    {
                        if (value.compare(*enumPtr,Qt::CaseInsensitive) == 0)
                        {
                            enumFound = true;
                            break;
                        }
                        ++enumPtr;
                        ++enumValue;
                    }
                    if (enumFound)
                        varValue = enumValue;
                    else
                        URHO3D_LOGWARNING("Unknown enum value " + value + " in attribute " + attr.name_);
                }
                else
                    varValue = attrElem.GetVariantValue(attr.type_);

                if (!varValue.IsEmpty())
                    OnSetAttribute(attr, varValue);

                startIndex = (i + 1) % attributes->size();
                break;
            }
            else
            {
                i = (i + 1) % attributes->size();
                --attempts;
            }
        }

        if (attempts == 0u)
            URHO3D_LOGWARNING("Unknown attribute " + name + " in XML data");

        attrElem = attrElem.GetNext("attribute");
    }

    return true;
}

bool Serializable::LoadJSON(const JSONValue& source)
{
    if (source.IsNull())
    {
        URHO3D_LOGERROR("Could not load " + GetTypeName() + ", null JSON source element");
        return false;
    }

    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
        return true;

    // Get attributes value
    JSONValue attributesValue = source.Get("attributes");
    if (attributesValue.IsNull())
        return true;
    // Warn if the attributes value isn't an object
    if (!attributesValue.IsObject())
    {
        URHO3D_LOGWARNING("'attributes' object is present in " + GetTypeName() + " but is not a JSON object; skipping load");
        return true;
    }

    const JSONObject& attributesObject = attributesValue.GetObject();

    unsigned startIndex = 0;

    for (JSONObject::const_iterator it = attributesObject.begin(); it != attributesObject.end();)
    {
        QString name = MAP_KEY(it);
        const JSONValue& value = MAP_VALUE(it);
        unsigned i = startIndex;
        unsigned attempts = attributes->size();

        while (attempts != 0u)
        {
            const AttributeInfo& attr = attributes->at(i);
            if (((attr.mode_ & AM_FILE) != 0u) && (attr.name_.compare(name) == 0))
            {
                Variant varValue;

                // If enums specified, do enum lookup ad int assignment. Otherwise assign variant directly
                if (attr.enumNames_ != nullptr)
                {
                    QString valueStr = value.GetString();
                    bool enumFound = false;
                    int enumValue = 0;
                    const char* const * enumPtr = attr.enumNames_;
                    while (*enumPtr != nullptr)
                    {
                        if (valueStr.compare(*enumPtr,Qt::CaseInsensitive) == 0)
                        {
                            enumFound = true;
                            break;
                        }
                        ++enumPtr;
                        ++enumValue;
                    }
                    if (enumFound)
                        varValue = enumValue;
                    else
                        URHO3D_LOGWARNING("Unknown enum value " + valueStr + " in attribute " + attr.name_);
                }
                else
                    varValue = value.GetVariantValue(attr.type_);

                if (!varValue.IsEmpty())
                    OnSetAttribute(attr, varValue);

                startIndex = (i + 1) % attributes->size();
                break;
            }
            else
            {
                i = (i + 1) % attributes->size();
                --attempts;
            }
        }

        if (attempts == 0)
            URHO3D_LOGWARNING("Unknown attribute " + name + " in JSON data");

        it++;
    }

    return true;
}
bool Serializable::SaveXML(XMLElement& dest) const
{
    if (dest.IsNull())
    {
        URHO3D_LOGERROR("Could not save " + GetTypeName() + ", null destination element");
        return false;
    }

    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
        return true;

    Variant value;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (((attr.mode_ & AM_FILE) == 0u) || (attr.mode_ & AM_FILEREADONLY) == AM_FILEREADONLY)
            continue;

        OnGetAttribute(attr, value);
        Variant defaultValue(GetAttributeDefault(i));

        // In XML serialization default values can be skipped. This will make the file easier to read or edit manually
        if (value == defaultValue && !SaveDefaultAttributes())
            continue;

        XMLElement attrElem = dest.CreateChild("attribute");
        attrElem.SetAttribute("name", attr.name_);
        // If enums specified, set as an enum string. Otherwise set directly as a Variant
        if (attr.enumNames_ != nullptr)
        {
            int enumValue = value.GetInt();
            attrElem.SetAttribute("value", attr.enumNames_[enumValue]);
        }
        else
            attrElem.SetVariantValue(value);
    }

    return true;
}

bool Serializable::SaveJSON(JSONValue& dest) const
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
        return true;

    Variant value;
    JSONValue attributesValue;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (((attr.mode_ & AM_FILE) == 0u) || (attr.mode_ & AM_FILEREADONLY) == AM_FILEREADONLY)
            continue;

        OnGetAttribute(attr, value);
        Variant defaultValue(GetAttributeDefault(i));

        // In JSON serialization default values can be skipped. This will make the file easier to read or edit manually
        if (value == defaultValue && !SaveDefaultAttributes())
            continue;

        JSONValue attrVal;
        // If enums specified, set as an enum string. Otherwise set directly as a Variant
        if (attr.enumNames_ != nullptr)
        {
            int enumValue = value.GetInt();
            attrVal = attr.enumNames_[enumValue];
        }
        else
            attrVal.SetVariantValue(value, context_);

        attributesValue.Set(attr.name_, attrVal);
    }
    dest.Set("attributes", attributesValue);

    return true;
}
bool Serializable::SetAttribute(unsigned index, const Variant& value)
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return false;
    }
    if (index >= attributes->size())
    {
        URHO3D_LOGERROR("Attribute index out of bounds");
        return false;
    }

    const AttributeInfo& attr = attributes->at(index);

    // Check that the new value's type matches the attribute type
    if (value.GetType() == attr.type_)
    {
        OnSetAttribute(attr, value);
        return true;
    }


    URHO3D_LOGERROR("Could not set attribute " + attr.name_ + ": expected type " + Variant::GetTypeName(attr.type_) +
                    " but got " + value.GetTypeName());
    return false;

}

bool Serializable::SetAttribute(const QString& name, const Variant& value)
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return false;
    }

    for (std::vector<AttributeInfo>::const_iterator i = attributes->begin(); i != attributes->end(); ++i)
    {
        if (i->name_.compare(name) == 0)
        {
            // Check that the new value's type matches the attribute type
            if (value.GetType() == i->type_)
            {
                OnSetAttribute(*i, value);
                return true;
            }


            URHO3D_LOGERROR("Could not set attribute " + i->name_ + ": expected type " + Variant::GetTypeName(i->type_)
                            + " but got " + value.GetTypeName());
            return false;

        }
    }

    URHO3D_LOGERROR("Could not find attribute " + name + " in " + GetTypeName());
    return false;
}

void Serializable::ResetToDefault()
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
        return;

    for (const AttributeInfo& attr : *attributes)
    {
        if ((attr.mode_ & (AM_NOEDIT | AM_NODEID | AM_COMPONENTID | AM_NODEIDVECTOR)) != 0u)
            continue;

        Variant defaultValue = GetInstanceDefault(attr.name_);
        if (defaultValue.IsEmpty())
            defaultValue = attr.defaultValue_;

        OnSetAttribute(attr, defaultValue);
    }
}

void Serializable::RemoveInstanceDefault()
{
    instanceDefaultValues_.reset();
}

void Serializable::SetTemporary(bool enable)
{
    if (enable != temporary_)
    {
        temporary_ = enable;
        g_sceneSignals.temporaryChanged(this);
    }
}

void Serializable::SetInterceptNetworkUpdate(const QString& attributeName, bool enable)
{
    AllocateNetworkState();
    const std::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (attributes == nullptr)
        return;


    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (attr.name_.compare(attributeName) == 0)
        {
            if (enable)
                networkState_->interceptMask_ |= 1ULL << i;
            else
                networkState_->interceptMask_ &= ~(1ULL << i);
            break;
        }
    }
}

void Serializable::AllocateNetworkState()
{
    if (networkState_ != nullptr)
        return;
        const std::vector<AttributeInfo>* networkAttributes = GetNetworkAttributes();
        networkState_.reset(new NetworkState());
        networkState_->attributes_ = networkAttributes;
    if (!networkAttributes)
        return;

    unsigned numAttributes = networkAttributes->size();

    if (networkState_->currentValues_.size() != numAttributes)
    {
        networkState_->currentValues_.resize(numAttributes);
        networkState_->previousValues_.resize(numAttributes);

        // Copy the default attribute values to the previous state as a starting point
        for (unsigned i = 0; i < numAttributes; ++i)
            networkState_->previousValues_[i] = networkAttributes->at(i).defaultValue_;
    }
}

void Serializable::WriteInitialDeltaUpdate(Serializer& dest, unsigned char timeStamp)
{
    if (networkState_ == nullptr)
    {
        URHO3D_LOGERROR("WriteInitialDeltaUpdate called without allocated NetworkState");
        return;
    }

    const std::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (attributes == nullptr)
        return;

    unsigned numAttributes = attributes->size();
    DirtyBits attributeBits;

    // Compare against defaults
    for (unsigned i = 0; i < numAttributes; ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (networkState_->currentValues_[i] != attr.defaultValue_)
            attributeBits.Set(i);
    }

    // First write the change bitfield, then attribute data for non-default attributes
    dest.WriteUByte(timeStamp);
    dest.Write(attributeBits.data_, (numAttributes + 7) >> 3);

    for (unsigned i = 0; i < numAttributes; ++i)
    {
        if (attributeBits.IsSet(i))
            dest.WriteVariantData(networkState_->currentValues_[i]);
    }
}

void Serializable::WriteDeltaUpdate(Serializer& dest, const DirtyBits& attributeBits, unsigned char timeStamp)
{
    if (networkState_ == nullptr)
    {
        URHO3D_LOGERROR("WriteDeltaUpdate called without allocated NetworkState");
        return;
    }

    const std::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (attributes == nullptr)
        return;

    unsigned numAttributes = attributes->size();

    // First write the change bitfield, then attribute data for changed attributes
    // Note: the attribute bits should not contain LATESTDATA attributes
    dest.WriteUByte(timeStamp);
    dest.Write(attributeBits.data_, (numAttributes + 7) >> 3);

    for (unsigned i = 0; i < numAttributes; ++i)
    {
        if (attributeBits.IsSet(i))
            dest.WriteVariantData(networkState_->currentValues_[i]);
    }
}

void Serializable::WriteLatestDataUpdate(Serializer& dest, unsigned char timeStamp)
{
    if (networkState_ == nullptr)
    {
        URHO3D_LOGERROR("WriteLatestDataUpdate called without allocated NetworkState");
        return;
    }

    const std::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (attributes == nullptr)
        return;

    unsigned numAttributes = attributes->size();
    dest.WriteUByte(timeStamp);

    for (unsigned i = 0; i < numAttributes; ++i)
    {
        if ((attributes->at(i).mode_ & AM_LATESTDATA) != 0u)
            dest.WriteVariantData(networkState_->currentValues_[i]);
    }
}

bool Serializable::ReadDeltaUpdate(Deserializer& source)
{
    const std::vector<AttributeInfo>* attributes = GetNetworkAttributes();
    if (attributes == nullptr)
        return false;

    unsigned numAttributes = attributes->size();
    DirtyBits attributeBits;
    bool changed = false;

    uint64_t interceptMask = networkState_ != nullptr ? networkState_->interceptMask_ : 0;
    unsigned char timeStamp = source.ReadUByte();
    source.Read(attributeBits.data_, (numAttributes + 7) >> 3);

    for (unsigned i = 0; i < numAttributes && !source.IsEof(); ++i)
    {
        if (attributeBits.IsSet(i))
        {
            const AttributeInfo& attr = attributes->at(i);
            if ((interceptMask & (1ULL << i)) == 0u)
            {
                OnSetAttribute(attr, source.ReadVariant(attr.type_));
                changed = true;
            }
            else
            {
                g_sceneSignals.interceptNetworkUpdate(this, timeStamp,
                                                           RemapAttributeIndex(GetAttributes(), attr, i), attr.name_,
                                                           source.ReadVariant(attr.type_));
            }
        }
    }

    return changed;
}

bool Serializable::ReadLatestDataUpdate(Deserializer& source)
{
    const std::vector<AttributeInfo>* attributes = GetNetworkAttributes();
    if (attributes == nullptr)
        return false;

    unsigned numAttributes = attributes->size();
    bool changed = false;

    unsigned long long interceptMask = networkState_ != nullptr ? networkState_->interceptMask_ : 0;
    unsigned char timeStamp = source.ReadUByte();

    for (unsigned i = 0; i < numAttributes && !source.IsEof(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if ((attr.mode_ & AM_LATESTDATA) != 0u)
        {
            if ((interceptMask & (1ULL << i)) == 0u)
            {
                OnSetAttribute(attr, source.ReadVariant(attr.type_));
                changed = true;
            }
            else
            {
                g_sceneSignals.interceptNetworkUpdate(this, timeStamp,
                                                           RemapAttributeIndex(GetAttributes(), attr, i), attr.name_,
                                                           source.ReadVariant(attr.type_));

            }
        }
    }

    return changed;
}

Variant Serializable::GetAttribute(unsigned index) const
{
    Variant ret;

    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return ret;
    }
    if (index >= attributes->size())
    {
        URHO3D_LOGERROR("Attribute index out of bounds");
        return ret;
    }

    OnGetAttribute(attributes->at(index), ret);
    return ret;
}

Variant Serializable::GetAttribute(const QString& name) const
{
    Variant ret;

    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return ret;
    }

    for (const AttributeInfo & attribute : *attributes)
    {
        if (attribute.name_.compare(name) == 0)
        {
            OnGetAttribute(attribute, ret);
            return ret;
        }
    }

    URHO3D_LOGERROR("Could not find attribute " + name + " in " + GetTypeName());
    return ret;
}

Variant Serializable::GetAttributeDefault(unsigned index) const
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return Variant::EMPTY;
    }
    if (index >= attributes->size())
    {
        URHO3D_LOGERROR("Attribute index out of bounds");
        return Variant::EMPTY;
    }

    AttributeInfo attr = attributes->at(index);
    Variant defaultValue = GetInstanceDefault(attr.name_);
    return defaultValue.IsEmpty() ? attr.defaultValue_ : defaultValue;
}

Variant Serializable::GetAttributeDefault(const QString& name) const
{
    Variant defaultValue = GetInstanceDefault(name);
    if (!defaultValue.IsEmpty())
        return defaultValue;

    const std::vector<AttributeInfo>* attributes = GetAttributes();
    if (attributes == nullptr)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return Variant::EMPTY;
    }

    for (const AttributeInfo & attribute : *attributes)
    {
        if (attribute.name_.compare(name) == 0)
            return attribute.defaultValue_;
    }

    URHO3D_LOGERROR("Could not find attribute " + name + " in " + GetTypeName());
    return Variant::EMPTY;
}

unsigned Serializable::GetNumAttributes() const
{
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    return attributes != nullptr ? attributes->size() : 0;
}

unsigned Serializable::GetNumNetworkAttributes() const
{
    const std::vector<AttributeInfo>* attributes = networkState_ != nullptr ? networkState_->attributes_ :
                                                                   context_->GetNetworkAttributes(GetType());
    return attributes != nullptr ? attributes->size() : 0;
}

bool Serializable::GetInterceptNetworkUpdate(const QString& attributeName) const
{
    const std::vector<AttributeInfo>* attributes = GetNetworkAttributes();
    if (attributes == nullptr)
        return false;

    unsigned long long interceptMask = networkState_ != nullptr ? networkState_->interceptMask_ : 0;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (attr.name_.compare(attributeName) == 0)
            return (interceptMask & (1ULL << i)) != 0u;
    }

    return false;
}

void Serializable::SetInstanceDefault(const QString& name, const Variant& defaultValue)
{
    // Allocate the instance level default value
    if (instanceDefaultValues_ == nullptr)
        instanceDefaultValues_.reset(new VariantMap());
    instanceDefaultValues_->operator[] (name) = defaultValue;
}

Variant Serializable::GetInstanceDefault(const QString& name) const
{
    if (instanceDefaultValues_ != nullptr)
    {
        VariantMap::const_iterator i = instanceDefaultValues_->find(name);
        if (i != instanceDefaultValues_->end())
            return MAP_VALUE(i);
    }

    return Variant::EMPTY;
}

}
