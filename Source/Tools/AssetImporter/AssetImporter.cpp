//
// Copyright (c) 2008-2018 the Urho3D project.
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

#include <Lutefisk3D/Core/Context.h>
#include <Lutefisk3D/Core/ProcessUtils.h>
#include <Lutefisk3D/Core/StringUtils.h>
#include <Lutefisk3D/Core/WorkQueue.h>
#include <Lutefisk3D/Graphics/AnimatedModel.h>
#include <Lutefisk3D/Graphics/Animation.h>
#include <Lutefisk3D/Graphics/DebugRenderer.h>
#include <Lutefisk3D/Graphics/Geometry.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Graphics/IndexBuffer.h>
#include <Lutefisk3D/Graphics/Light.h>
#include <Lutefisk3D/Graphics/Material.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/Graphics/VertexBuffer.h>
#include <Lutefisk3D/Graphics/Zone.h>
#include <Lutefisk3D/IO/File.h>
#include <Lutefisk3D/IO/FileSystem.h>
#ifdef LUTEFISK3D_PHYSICS
#include <Lutefisk3D/Physics/PhysicsWorld.h>
#endif
#include <Lutefisk3D/Resource/Image.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Resource/XMLFile.h>
#include <Lutefisk3D/Scene/Scene.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <assimp/config.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/DefaultLogger.hpp>

using namespace Urho3D;

struct OutModel
{
    QString outName_;
    aiNode* rootNode_{};
    HashSet<unsigned> meshIndices_;
    std::vector<aiMesh*> meshes_;
    std::vector<aiNode*> meshNodes_;
    std::vector<aiNode*> bones_;
    std::vector<aiNode*> pivotlessBones_;
    std::vector<aiAnimation*> animations_;
    std::vector<float> boneRadii_;
    std::vector<BoundingBox> boneHitboxes_;
    aiNode* rootBone_{};
    unsigned totalVertices_{};
    unsigned totalIndices_{};
};

struct OutScene
{
    QString outName_;
    aiNode* rootNode_{};
    std::vector<OutModel> models_;
    std::vector<aiNode*> nodes_;
    std::vector<unsigned> nodeModelIndices_;
};

// FBX transform chain
enum TransformationComp
{
    TransformationComp_Translation = 0,
    TransformationComp_RotationOffset,
    TransformationComp_RotationPivot,
    TransformationComp_PreRotation,
    TransformationComp_Rotation,
    TransformationComp_PostRotation,
    TransformationComp_RotationPivotInverse,

    TransformationComp_ScalingOffset,
    TransformationComp_ScalingPivot,
    TransformationComp_Scaling,

    // Not checking these
    // They are typically flushed out in the fbxconverter, but there
    // might be cases where they're not, hence, leaving them.
    #ifdef EXT_TRANSFORMATION_CHECK
    TransformationComp_ScalingPivotInverse,
    TransformationComp_GeometricTranslation,
    TransformationComp_GeometricRotation,
    TransformationComp_GeometricScaling,
    #endif

    TransformationComp_MAXIMUM
};

const char *transformSuffix[TransformationComp_MAXIMUM] =
{
    "Translation",          // TransformationComp_Translation = 0,
    "RotationOffset",       // TransformationComp_RotationOffset,
    "RotationPivot",        // TransformationComp_RotationPivot,
    "PreRotation",          // TransformationComp_PreRotation,
    "Rotation",             // TransformationComp_Rotation,
    "PostRotation",         // TransformationComp_PostRotation,
    "RotationPivotInverse", // TransformationComp_RotationPivotInverse,

    "ScalingOffset",        // TransformationComp_ScalingOffset,
    "ScalingPivot",         // TransformationComp_ScalingPivot,
    "Scaling",              // TransformationComp_Scaling,

    #ifdef EXT_TRANSFORMATION_CHECK
    "ScalingPivotInverse",  // TransformationComp_ScalingPivotInverse,
    "GeometricTranslation", // TransformationComp_GeometricTranslation,
    "GeometricRotation",    // TransformationComp_GeometricRotation,
    "GeometricScaling",     // TransformationComp_GeometricScaling,
    #endif
};

static const unsigned MAX_CHANNELS = 4;

std::unique_ptr<Context> context_(new Context());
const aiScene* scene_ = nullptr;
aiNode* rootNode_ = nullptr;
QString inputName_;
QString resourcePath_;
QString outPath_;
QString outName_;
bool useSubdirs_ = true;
bool localIDs_ = false;
bool saveBinary_ = false;
bool saveJson_ = false;
bool createZone_ = true;
bool noAnimations_ = false;
bool noHierarchy_ = false;
bool noMaterials_ = false;
bool noTextures_ = false;
bool noMaterialDiffuseColor_ = false;
bool noEmptyNodes_ = false;
bool saveMaterialList_ = false;
bool includeNonSkinningBones_ = false;
bool verboseLog_ = false;
bool emissiveAO_ = false;
bool noOverwriteMaterial_ = false;
bool noOverwriteTexture_ = false;
bool noOverwriteNewerTexture_ = false;
bool checkUniqueModel_ = true;
bool moveToBindPose_ = false;
unsigned maxBones_ = 64;
QStringList nonSkinningBoneIncludes_;
QStringList nonSkinningBoneExcludes_;

HashSet<aiAnimation*> allAnimations_;
std::vector<aiAnimation*> sceneAnimations_;

float defaultTicksPerSecond_ = 4800.0f;
// For subset animation import usage
float importStartTime_ = 0.0f;
float importEndTime_ = 0.0f;
bool suppressFbxPivotNodes_ = true;

int main(int argc, char** argv);
void Run(const QStringList &arguments);
void DumpNodes(aiNode* rootNode, unsigned level);

void ExportModel(const QString& outName, bool animationOnly);
void ExportAnimation(const QString& outName, bool animationOnly);
void CollectMeshes(OutModel& model, aiNode* node);
void CollectBones(OutModel& model, bool animationOnly = false);
void CollectBonesFinal(std::vector<aiNode*>& dest, const HashSet<aiNode*>& necessary, aiNode* node);
void MoveToBindPose(OutModel& model, aiNode* current);
void CollectAnimations(OutModel* model = nullptr);
void BuildBoneCollisionInfo(OutModel& model);
void BuildAndSaveModel(OutModel& model);
void BuildAndSaveAnimations(OutModel* model = nullptr);

void ExportScene(const QString& outName, bool asPrefab);
void CollectSceneModels(OutScene& scene, aiNode* node);
void CreateHierarchy(Scene* scene, aiNode* srcNode, HashMap<aiNode*, Node*>& nodeMapping);
Node* CreateSceneNode(Scene* scene, aiNode* srcNode, HashMap<aiNode*, Node*>& nodeMapping);
void BuildAndSaveScene(OutScene& scene, bool asPrefab);

void ExportMaterials(HashSet<QString>& usedTextures);
void BuildAndSaveMaterial(aiMaterial* material, HashSet<QString>& usedTextures);
void CopyTextures(const HashSet<QString>& usedTextures, const QString& sourcePath);

void CombineLods(const std::vector<float>& lodDistances, const std::vector<QString>& modelNames, const QString& outName);

void GetMeshesUnderNode(std::vector<std::pair<aiNode*, aiMesh*> >& dest, aiNode* node);
unsigned GetMeshIndex(aiMesh* mesh);
unsigned GetBoneIndex(OutModel& model, const QString& boneName);
aiBone* GetMeshBone(OutModel& model, const QString& boneName);
Matrix3x4 GetOffsetMatrix(OutModel& model, const QString& boneName);
void GetBlendData(OutModel& model, aiMesh* mesh, aiNode* meshNode, std::vector<unsigned>& boneMappings, std::vector<std::vector<unsigned char> >&
    blendIndices, std::vector<std::vector<float> >& blendWeights);
QString GetMeshMaterialName(aiMesh* mesh);
QString GetMaterialTextureName(const QString& nameIn);
QString GenerateMaterialName(aiMaterial* material);
QString GenerateTextureName(unsigned texIndex);
unsigned GetNumValidFaces(aiMesh* mesh);

void WriteShortIndices(unsigned short*& dest, aiMesh* mesh, unsigned index, unsigned offset);
void WriteLargeIndices(unsigned*& dest, aiMesh* mesh, unsigned index, unsigned offset);
void WriteVertex(float*& dest, aiMesh* mesh, unsigned index, bool isSkinned, BoundingBox& box,
    const Matrix3x4& vertexTransform, const Matrix3& normalTransform, std::vector<std::vector<unsigned char> >& blendIndices,
    std::vector<std::vector<float> >& blendWeights);
std::vector<VertexElement> GetVertexElements(aiMesh* mesh, bool isSkinned);

aiNode* GetNode(const QString& name, aiNode* rootNode, bool caseSensitive = true);
aiMatrix4x4 GetDerivedTransform(aiNode* node, aiNode* rootNode, bool rootInclusive = true);
aiMatrix4x4 GetDerivedTransform(aiMatrix4x4 transform, aiNode* node, aiNode* rootNode, bool rootInclusive = true);
aiMatrix4x4 GetMeshBakingTransform(aiNode* meshNode, aiNode* modelRootNode);
void GetPosRotScale(const aiMatrix4x4& transform, Vector3& pos, Quaternion& rot, Vector3& scale);

QString FromAIString(const aiString& str);
Vector3 ToVector3(const aiVector3D& vec);
Vector2 ToVector2(const aiVector2D& vec);
Quaternion ToQuaternion(const aiQuaternion& quat);
Matrix3x4 ToMatrix3x4(const aiMatrix4x4& mat);
aiMatrix4x4 ToAIMatrix4x4(const Matrix3x4& mat);
QString SanitateAssetName(const QString& name);

unsigned GetPivotlessBoneIndex(OutModel& model, const QString& boneName);
void ExtrapolatePivotlessAnimation(OutModel* model);
void CollectSceneNodesAsBones(OutModel &model, aiNode* rootNode);

int main(int argc, char** argv)
{
    QStringList arguments;

    #ifdef _WIN32
    arguments = ParseArguments(GetCommandLine());
    #else
    arguments = ParseArguments(argc, argv);
    #endif

    Run(arguments);
    return 0;
}

void Run(const QStringList& arguments)
{
    if (arguments.size() < 2)
    {
        ErrorExit(
            "Usage: AssetImporter <command> <input file> <output file> [options]\n"
            "See http://assimp.sourceforge.net/main_features_formats.html for input formats\n\n"
            "Commands:\n"
            "model       Output a model\n"
            "anim        Output animation(s)\n"
            "scene       Output a scene\n"
            "node        Output a node and its children (prefab)\n"
            "dump        Dump scene node structure. No output file is generated\n"
            "lod         Combine several Urho3D models as LOD levels of the output model\n"
            "            Syntax: lod <dist0> <mdl0> <dist1 <mdl1> ... <output file>\n"
            "\n"
            "Options:\n"
            "-b          Save scene in binary format, default format is XML\n"
            "-j          Save scene in JSON format, default format is XML\n"
            "-h          Generate hard instead of smooth normals if input has no normals\n"
            "-i          Use local ID's for scene nodes\n"
            "-l          Output a material list file for models\n"
            "-na         Do not output animations\n"
            "-nm         Do not output materials\n"
            "-nt         Do not output material textures\n"
            "-nc         Do not use material diffuse color value, instead output white\n"
            "-nh         Do not save full node hierarchy (scene mode only)\n"
            "-ns         Do not create subdirectories for resources\n"
            "-nz         Do not create a zone and a directional light (scene mode only)\n"
            "-nf         Do not fix infacing normals\n"
            "-ne         Do not save empty nodes (scene mode only)\n"
            "-mb <x>     Maximum number of bones per submesh. Default 64\n"
            "-p <path>   Set path for scene resources. Default is output file path\n"
            "-r <name>   Use the named scene node as root node\n"
            "-f <freq>   Animation tick frequency to use if unspecified. Default 4800\n"
            "-o          Optimize redundant submeshes. Loses scene hierarchy and animations\n"
            "-s <filter> Include non-skinning bones in the model's skeleton. Can be given a\n"
            "            case-insensitive semicolon separated filter list. Bone is included\n"
            "            if its name contains any of the filters. Prefix filter with minus\n"
            "            sign to use as an exclude. For example -s \"Bip01;-Dummy;-Helper\"\n"
            "-t          Generate tangents\n"
            "-v          Enable verbose Assimp library logging\n"
            "-eao        Interpret material emissive texture as ambient occlusion\n"
            "-cm         Check and do not overwrite if material exists\n"
            "-ct         Check and do not overwrite if texture exists\n"
            "-ctn        Check and do not overwrite if texture has newer timestamp\n"
            "-am         Export all meshes even if identical (scene mode only)\n"
            "-bp         Move bones to bind pose before saving model\n"
            "-split <start> <end> (animation model only)\n"
            "            Split animation, will only import from start frame to end frame\n"
            "-np         Do not suppress $fbx pivot nodes (FBX files only)\n"
        );
    }

    context_->m_FileSystem.reset(new FileSystem(context_.get()));
    context_->m_ResourceCache.reset(new ResourceCache(context_.get()));
    context_->m_WorkQueueSystem.reset(new WorkQueue(context_.get()));
    RegisterSceneLibrary(context_.get());
    RegisterGraphicsLibrary(context_.get());
#ifdef LUTEFISK3D_PHYSICS
    RegisterPhysicsLibrary(context_.get());
#endif

    QString command = arguments[0].toLower();
    QString rootNodeName;

    unsigned flags =
        aiProcess_ConvertToLeftHanded |
        aiProcess_JoinIdenticalVertices |
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_LimitBoneWeights |
        aiProcess_ImproveCacheLocality |
        aiProcess_RemoveRedundantMaterials |
        aiProcess_FixInfacingNormals |
        aiProcess_FindInvalidData |
        aiProcess_GenUVCoords |
        aiProcess_FindInstances |
        aiProcess_OptimizeMeshes;

    for (unsigned i = 2; i < arguments.size(); ++i)
    {
        if (arguments[i].length() > 1 && arguments[i][0] == '-')
        {
            QString argument = arguments[i].mid(1).toLower();
            QString value = i + 1 < arguments.size() ? arguments[i + 1] : QString();

            if (argument == "b")
                saveBinary_ = true;
            else if(argument == "j")
                saveJson_ = true;
            else if (argument == "h")
            {
                flags &= ~aiProcess_GenSmoothNormals;
                flags |= aiProcess_GenNormals;
            }
            else if (argument == "i")
                localIDs_ = true;
            else if (argument == "l")
                saveMaterialList_ = true;
            else if (argument == "t")
                flags |= aiProcess_CalcTangentSpace;
            else if (argument == "o")
                flags |= aiProcess_PreTransformVertices;
            else if (argument.length() == 2 && argument[0] == 'n')
            {
                switch (argument[1].toLower().toLatin1())
                {
                case 'a':
                    noAnimations_ = true;
                    break;

                case 'c':
                    noMaterialDiffuseColor_ = true;
                    break;

                case 'm':
                    noMaterials_ = true;
                    break;

                case 'h':
                    noHierarchy_ = true;
                    break;

                case 'e':
                    noEmptyNodes_ = true;
                    break;

                case 's':
                    useSubdirs_ = false;
                    break;

                case 't':
                    noTextures_ = true;
                    break;

                case 'z':
                    createZone_ = false;
                    break;

                case 'f':
                    flags &= ~aiProcess_FixInfacingNormals;
                    break;

                case 'p':
                        suppressFbxPivotNodes_ = false;
                    break;

                }
            }
            else if (argument == "mb" && !value.isEmpty())
            {
                maxBones_ = value.toUInt();
                if (maxBones_ < 1)
                    maxBones_ = 1;
                ++i;
            }
            else if (argument == "p" && !value.isEmpty())
            {
                resourcePath_ = AddTrailingSlash(value);
                ++i;
            }
            else if (argument == "r" && !value.isEmpty())
            {
                rootNodeName = value;
                ++i;
            }
            else if (argument == "f" && !value.isEmpty())
            {
                defaultTicksPerSecond_ = value.toFloat();
                ++i;
            }
            else if (argument == "s")
            {
                includeNonSkinningBones_ = true;
                if (value.length() && (value[0] != '-' || value.length() > 3))
                {
                    auto filters = value.split(';');
                    for (unsigned i = 0; i < filters.size(); ++i)
                    {
                        if (filters[i][0] == '-')
                            nonSkinningBoneExcludes_.push_back(filters[i].mid(1));
                        else
                            nonSkinningBoneIncludes_.push_back(filters[i]);
                    }
                }
            }
            else if (argument == "v")
                verboseLog_ = true;
            else if (argument == "eao")
                emissiveAO_ = true;
            else if (argument == "cm")
                noOverwriteMaterial_ = true;
            else if (argument == "ct")
                noOverwriteTexture_ = true;
            else if (argument == "ctn")
                noOverwriteNewerTexture_ = true;
            else if (argument == "am")
                checkUniqueModel_ = false;
            else if (argument == "bp")
                moveToBindPose_ = true;
            else if (argument == "split")
            {
                QString value2 = i + 2 < arguments.size() ? arguments[i + 2] : QString();
                if (value.length() && value2.length() && (value[0] != '-') && (value2[0] != '-'))
                {
                    importStartTime_ = value.toFloat();
                    importEndTime_ = value2.toFloat();
                }
            }
        }
    }

    if (command == "model" || command == "scene" || command == "anim" || command == "node" || command == "dump")
    {
        QString inFile = arguments[1];
        QString outFile;
        if (arguments.size() > 2 && arguments[2][0] != '-')
            outFile = GetInternalPath(arguments[2]);

        inputName_ = GetFileName(inFile);
        outName_ = outFile;
        outPath_ = GetPath(outFile);

        if (resourcePath_.isEmpty())
        {
            resourcePath_ = outPath_;
            // If output file already has the Models/ path (model mode), do not take it into the resource path
            if (command == "model")
            {
                if (resourcePath_.endsWith("Models/", Qt::CaseInsensitive))
                    resourcePath_ = resourcePath_.mid(0, resourcePath_.length() - 7);
            }
            if (resourcePath_.isEmpty())
                resourcePath_ = "./";
        }

        resourcePath_ = AddTrailingSlash(resourcePath_);

        if (command != "dump" && outFile.isEmpty())
            ErrorExit("No output file defined");

        if (verboseLog_)
            Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE, aiDefaultLogStream_STDOUT);

        PrintLine("Reading file " + inFile);

        if (!inFile.endsWith(".fbx", Qt::CaseInsensitive))
            suppressFbxPivotNodes_ = false;

        // Only do this for the "model" command. "anim" command extrapolates animation from the original bone definition
        if (suppressFbxPivotNodes_ && command == "model")
        {
            PrintLine("Suppressing $fbx nodes");
            aiPropertyStore *aiprops = aiCreatePropertyStore();
            aiSetImportPropertyInteger(aiprops, AI_CONFIG_IMPORT_FBX_READ_ALL_GEOMETRY_LAYERS, 1);       //default = true;
            aiSetImportPropertyInteger(aiprops, AI_CONFIG_IMPORT_FBX_READ_ALL_MATERIALS, 0);             //default = false;
            aiSetImportPropertyInteger(aiprops, AI_CONFIG_IMPORT_FBX_READ_MATERIALS, 1);                 //default = true;
            aiSetImportPropertyInteger(aiprops, AI_CONFIG_IMPORT_FBX_READ_CAMERAS, 1);                   //default = true;
            aiSetImportPropertyInteger(aiprops, AI_CONFIG_IMPORT_FBX_READ_LIGHTS, 1);                    //default = true;
            aiSetImportPropertyInteger(aiprops, AI_CONFIG_IMPORT_FBX_READ_ANIMATIONS, 1);                //default = true;
            aiSetImportPropertyInteger(aiprops, AI_CONFIG_IMPORT_FBX_STRICT_MODE, 0);                    //default = false;
            aiSetImportPropertyInteger(aiprops, AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, 0);                //**false, default = true;
            aiSetImportPropertyInteger(aiprops, AI_CONFIG_IMPORT_FBX_OPTIMIZE_EMPTY_ANIMATION_CURVES, 1);//default = true;

            scene_ = aiImportFileExWithProperties(qPrintable(GetNativePath(inFile)), flags, nullptr, aiprops);

            // prevent processing animation suppression, both cannot work simultaneously
            suppressFbxPivotNodes_ = false;
        }
        else
            scene_ = aiImportFile(qPrintable(GetNativePath(inFile)), flags);

        if (!scene_)
            ErrorExit("Could not open or parse input file " + inFile + ": " + QString(aiGetErrorString()));

        if (verboseLog_)
            Assimp::DefaultLogger::kill();

        rootNode_ = scene_->mRootNode;
        if (!rootNodeName.isEmpty())
        {
            rootNode_ = GetNode(rootNodeName, rootNode_, false);
            if (!rootNode_)
                ErrorExit("Could not find scene node " + rootNodeName);
        }

        if (command == "dump")
        {
            DumpNodes(rootNode_, 0);
            return;
        }

        if (command == "model")
            ExportModel(outFile, scene_->mFlags & AI_SCENE_FLAGS_INCOMPLETE);

        if (command == "anim")
        {
            noMaterials_ = true;
            ExportAnimation(outFile, scene_->mFlags & AI_SCENE_FLAGS_INCOMPLETE);
        }
        if (command == "scene" || command == "node")
        {
            bool asPrefab = command == "node";
            // Saving as prefab requires the hierarchy, especially the root node
            if (asPrefab)
                noHierarchy_ = false;
            ExportScene(outFile, asPrefab);
        }

        if (!noMaterials_)
        {
            HashSet<QString> usedTextures;
            ExportMaterials(usedTextures);
            if (!noTextures_)
                CopyTextures(usedTextures, GetPath(inFile));
        }
    }
    else if (command == "lod")
    {
        std::vector<float> lodDistances;
        std::vector<QString> modelNames;
        QString outFile;

        unsigned numLodArguments = 0;
        for (unsigned i = 1; i < arguments.size(); ++i)
        {
            if (arguments[i][0] == '-')
                break;
            ++numLodArguments;
        }
        if (numLodArguments < 4)
            ErrorExit("Must define at least 2 LOD levels");
        if (!(numLodArguments & 1u))
            ErrorExit("No output file defined");

        for (unsigned i = 1; i < numLodArguments + 1; ++i)
        {
            if (i == numLodArguments)
                outFile = GetInternalPath(arguments[i]);
            else
            {
                if (i & 1u)
                    lodDistances.push_back(std::max(arguments[i].toFloat(), 0.0f));
                else
                    modelNames.push_back(GetInternalPath(arguments[i]));
            }
        }

        if (lodDistances[0] != 0.0f)
        {
            PrintLine("Warning: first LOD distance forced to 0");
            lodDistances[0] = 0.0f;
        }

        CombineLods(lodDistances, modelNames, outFile);
    }
    else
        ErrorExit("Unrecognized command " + command);
}

void DumpNodes(aiNode* rootNode, unsigned level)
{
    if (!rootNode)
        return;

    QString indent(' ', level * 2);
    Vector3 pos, scale;
    Quaternion rot;
    aiMatrix4x4 transform = GetDerivedTransform(rootNode, rootNode_);
    GetPosRotScale(transform, pos, rot, scale);

    PrintLine(indent + "Node " + FromAIString(rootNode->mName) + " pos " + pos.ToString());

    if (rootNode->mNumMeshes == 1)
        PrintLine(indent + "  " + QString(rootNode->mNumMeshes) + " geometry");
    if (rootNode->mNumMeshes > 1)
        PrintLine(indent + "  " + QString(rootNode->mNumMeshes) + " geometries");

    for (unsigned i = 0; i < rootNode->mNumChildren; ++i)
        DumpNodes(rootNode->mChildren[i], level + 1);
}

void ExportModel(const QString& outName, bool animationOnly)
{
    if (outName.isEmpty())
        ErrorExit("No output file defined");

    OutModel model;
    model.rootNode_ = rootNode_;
    model.outName_ = outName;

    CollectMeshes(model, model.rootNode_);
    CollectBones(model, animationOnly);
    BuildBoneCollisionInfo(model);
    BuildAndSaveModel(model);
    if (!noAnimations_)
    {
        CollectAnimations(&model);
        BuildAndSaveAnimations(&model);

        // Save scene-global animations
        CollectAnimations();
        BuildAndSaveAnimations();
    }
}

void ExportAnimation(const QString& outName, bool animationOnly)
{
    if (outName.isEmpty())
        ErrorExit("No output file defined");

    OutModel model;
    model.rootNode_ = rootNode_;
    model.outName_ = outName;

    CollectMeshes(model, model.rootNode_);
    CollectBones(model, animationOnly);
    BuildBoneCollisionInfo(model);
    //    BuildAndSaveModel(model);
    if (!noAnimations_)
    {
        // Most fbx animation files contain only a skeleton and no skinned mesh.
        // Assume the scene node contains the model's bone definition and,
        // transfer the info to the model.
        if (suppressFbxPivotNodes_ && model.bones_.size() == 0)
            CollectSceneNodesAsBones(model, rootNode_);

        CollectAnimations(&model);
        BuildAndSaveAnimations(&model);

        // Save scene-global animations
        CollectAnimations();
        BuildAndSaveAnimations();
    }
}

void CollectMeshes(OutModel& model, aiNode* node)
{
    for (unsigned i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene_->mMeshes[node->mMeshes[i]];
        for (unsigned j = 0; j < model.meshes_.size(); ++j)
        {
            if (mesh == model.meshes_[j])
            {
                PrintLine("Warning: same mesh found multiple times");
                break;
            }
        }

        model.meshIndices_.insert(node->mMeshes[i]);
        model.meshes_.push_back(mesh);
        model.meshNodes_.push_back(node);
        model.totalVertices_ += mesh->mNumVertices;
        model.totalIndices_ += GetNumValidFaces(mesh) * 3;
    }

    for (unsigned i = 0; i < node->mNumChildren; ++i)
        CollectMeshes(model, node->mChildren[i]);
}

void CollectBones(OutModel& model, bool animationOnly)
{
    HashSet<aiNode*> necessary;
    HashSet<aiNode*> rootNodes;

    bool haveSkinnedMeshes = false;
    for (unsigned i = 0; i < model.meshes_.size(); ++i)
    {
        if (model.meshes_[i]->HasBones())
        {
            haveSkinnedMeshes = true;
            break;
        }
    }

    for (unsigned i = 0; i < model.meshes_.size(); ++i)
    {
        aiMesh* mesh = model.meshes_[i];
        aiNode* meshNode = model.meshNodes_[i];
        aiNode* meshParentNode = meshNode->mParent;
        aiNode* rootNode = nullptr;

        for (unsigned j = 0; j < mesh->mNumBones; ++j)
        {
            aiBone* bone = mesh->mBones[j];
            QString boneName(FromAIString(bone->mName));
            aiNode* boneNode = GetNode(boneName, scene_->mRootNode, true);
            if (!boneNode)
                ErrorExit("Could not find scene node for bone " + boneName);
            necessary.insert(boneNode);
            rootNode = boneNode;

            for (;;)
            {
                boneNode = boneNode->mParent;
                if (!boneNode || ((boneNode == meshNode || boneNode == meshParentNode) && !animationOnly))
                    break;
                rootNode = boneNode;
                necessary.insert(boneNode);
            }

            if (rootNodes.find(rootNode) == rootNodes.end())
                rootNodes.insert(rootNode);
        }

        // When model is partially skinned, include the attachment nodes of the rigid meshes in the skeleton
        if (haveSkinnedMeshes && !mesh->mNumBones)
        {
            aiNode* boneNode = meshNode;
            necessary.insert(boneNode);
            rootNode = boneNode;

            for (;;)
            {
                boneNode = boneNode->mParent;
                if (!boneNode || ((boneNode == meshNode || boneNode == meshParentNode) && !animationOnly))
                    break;
                rootNode = boneNode;
                necessary.insert(boneNode);
            }

            if (rootNodes.find(rootNode) == rootNodes.end())
                rootNodes.insert(rootNode);
        }
    }


    // If we find multiple root nodes, try to remedy by going back in the parent chain and finding a common parent
    if (rootNodes.size() > 1)
    {
        for (HashSet<aiNode*>::iterator i = rootNodes.begin(); i != rootNodes.end(); ++i)
        {
            aiNode* commonParent = (*i);

            while (commonParent)
            {
                unsigned found = 0;
                for (HashSet<aiNode*>::iterator j = rootNodes.begin(); j != rootNodes.end(); ++j)
                {
                    if (i == j)
                        continue;
                    aiNode* parent = *j;
                    while (parent)
                    {
                        if (parent == commonParent)
                        {
                            ++found;
                            break;
                        }
                        parent = parent->mParent;
                    }
                }

                if (found >= rootNodes.size() - 1)
                {
                    PrintLine("Multiple roots initially found, using new root node " + FromAIString(commonParent->mName));
                    rootNodes.clear();
                    rootNodes.insert(commonParent);
                    necessary.insert(commonParent);
                    break;
                }

                commonParent = commonParent->mParent;
            }

            if (rootNodes.size() == 1)
                break; // Succeeded
        }
        if (rootNodes.size() > 1)
            ErrorExit("Skeleton with multiple root nodes found, not supported");
    }

    if (rootNodes.isEmpty())
        return;

    model.rootBone_ = *rootNodes.begin();

    // Move the model to bind pose now if requested
    if (moveToBindPose_)
    {
        PrintLine("Moving bones to bind pose");
        MoveToBindPose(model, model.rootBone_);
    }

    CollectBonesFinal(model.bones_, necessary, model.rootBone_);
    // Initialize the bone collision info
    model.boneRadii_.resize(model.bones_.size());
    model.boneHitboxes_.resize(model.bones_.size());
    for (unsigned i = 0; i < model.bones_.size(); ++i)
    {
        model.boneRadii_[i] = 0.0f;
        model.boneHitboxes_[i] = BoundingBox(0.0f, 0.0f);
    }
}

void MoveToBindPose(OutModel& model, aiNode* current)
{
    QString nodeName(FromAIString(current->mName));
    Matrix3x4 bindWorldTransform = GetOffsetMatrix(model, nodeName).Inverse();
    // Skip if we get an identity offset matrix (bone lookup failed)
    if (!bindWorldTransform.Equals(Matrix3x4::IDENTITY))
    {
        if (current->mParent && current != model.rootNode_)
        {
            aiMatrix4x4 parentWorldTransform = GetDerivedTransform(current->mParent, model.rootNode_, true);
            Matrix3x4 parentInverse = ToMatrix3x4(parentWorldTransform).Inverse();
            current->mTransformation = ToAIMatrix4x4(parentInverse * bindWorldTransform);
        }
        else
            current->mTransformation = ToAIMatrix4x4(bindWorldTransform);
    }

    for (unsigned i = 0; i < current->mNumChildren; ++i)
        MoveToBindPose(model, current->mChildren[i]);
}

void CollectBonesFinal(std::vector<aiNode*>& dest, const HashSet<aiNode*>& necessary, aiNode* node)
{
    bool includeBone = necessary.find(node) != necessary.end();
    QString boneName = FromAIString(node->mName);

    // Check include/exclude filters for non-skinned bones
    if (!includeBone && includeNonSkinningBones_)
    {
        // If no includes specified, include by default but check for excludes
        if (nonSkinningBoneIncludes_.isEmpty())
            includeBone = true;

        // Check against includes/excludes
        for (unsigned i = 0; i < nonSkinningBoneIncludes_.size(); ++i)
        {
            if (boneName.contains(nonSkinningBoneIncludes_[i], Qt::CaseInsensitive))
            {
                includeBone = true;
                break;
            }
        }
        for (unsigned i = 0; i < nonSkinningBoneExcludes_.size(); ++i)
        {
            if (boneName.contains(nonSkinningBoneExcludes_[i], Qt::CaseInsensitive))
            {
                includeBone = false;
                break;
            }
        }

        if (includeBone)
            PrintLine("Including non-skinning bone " + boneName);
    }

    if (includeBone)
        dest.push_back(node);

    for (unsigned i = 0; i < node->mNumChildren; ++i)
        CollectBonesFinal(dest, necessary, node->mChildren[i]);
}

void CollectAnimations(OutModel* model)
{
    const aiScene* scene = scene_;
    for (unsigned i = 0; i < scene->mNumAnimations; ++i)
    {
        aiAnimation* anim = scene->mAnimations[i];
        if (allAnimations_.contains(anim))
            continue;

        if (model)
        {
            bool modelBoneFound = false;
            for (unsigned j = 0; j < anim->mNumChannels; ++j)
            {
                aiNodeAnim* channel = anim->mChannels[j];
                QString channelName = FromAIString(channel->mNodeName);
                if (GetBoneIndex(*model, channelName) != M_MAX_UNSIGNED)
                {
                    modelBoneFound = true;
                    break;
                }
            }
            if (modelBoneFound)
            {
                model->animations_.push_back(anim);
                allAnimations_.insert(anim);
            }
        }
        else
        {
            sceneAnimations_.push_back(anim);
            allAnimations_.insert(anim);
        }
    }

    /// \todo Vertex morphs are ignored for now
}

void BuildBoneCollisionInfo(OutModel& model)
{
    for (unsigned i = 0; i < model.meshes_.size(); ++i)
    {
        aiMesh* mesh = model.meshes_[i];
        for (unsigned j = 0; j < mesh->mNumBones; ++j)
        {
            aiBone* bone = mesh->mBones[j];
            QString boneName = FromAIString(bone->mName);
            unsigned boneIndex = GetBoneIndex(model, boneName);
            if (boneIndex == M_MAX_UNSIGNED)
                continue;
            for (unsigned k = 0; k < bone->mNumWeights; ++k)
            {
                float weight = bone->mWeights[k].mWeight;
                // Require skinning weight to be sufficiently large before vertex contributes to bone hitbox
                if (weight > 0.33f)
                {
                    aiVector3D vertexBoneSpace = bone->mOffsetMatrix * mesh->mVertices[bone->mWeights[k].mVertexId];
                    Vector3 vertex = ToVector3(vertexBoneSpace);
                    float radius = vertex.Length();
                    if (radius > model.boneRadii_[boneIndex])
                        model.boneRadii_[boneIndex] = radius;
                    model.boneHitboxes_[boneIndex].Merge(vertex);
                }
            }
        }
    }
}

void BuildAndSaveModel(OutModel& model)
{
    if (!model.rootNode_)
    {
        PrintLine("Null root node for model, skipping model save");
        return;
    }

    QString rootNodeName = FromAIString(model.rootNode_->mName);
    if (!model.meshes_.size())
    {
        PrintLine("No geometries found starting from node " + rootNodeName + ", skipping model save");
        return;
    }

    PrintLine("Writing model " + rootNodeName);

    SharedPtr<Model> outModel(new Model(context_.get()));
    std::vector<std::vector<unsigned> > allBoneMappings;
    BoundingBox box;

    unsigned numValidGeometries = 0;

    bool combineBuffers = true;
    // Check if buffers can be combined (same vertex elements, under 65535 vertices)
    std::vector<VertexElement> elements = GetVertexElements(model.meshes_[0], model.bones_.size() > 0);
    for (unsigned i = 0; i < model.meshes_.size(); ++i)
    {
        if (GetNumValidFaces(model.meshes_[i]))
        {
            ++numValidGeometries;
            if (i > 0 && GetVertexElements(model.meshes_[i], model.bones_.size() > 0) != elements)
                combineBuffers = false;
        }
    }

    // Check if keeping separate buffers allows to avoid 32-bit indices
    if (combineBuffers && model.totalVertices_ > 65535)
    {
        bool allUnder65k = true;
        for (unsigned i = 0; i < model.meshes_.size(); ++i)
        {
            if (GetNumValidFaces(model.meshes_[i]))
            {
                if (model.meshes_[i]->mNumVertices > 65535)
                    allUnder65k = false;
            }
        }
        if (allUnder65k == true)
            combineBuffers = false;
    }

    SharedPtr<IndexBuffer> ib;
    SharedPtr<VertexBuffer> vb;
    std::vector<SharedPtr<VertexBuffer> > vbVector;
    std::vector<SharedPtr<IndexBuffer> > ibVector;
    unsigned startVertexOffset = 0;
    unsigned startIndexOffset = 0;
    unsigned destGeomIndex = 0;
    bool isSkinned = model.bones_.size() > 0;

    outModel->SetNumGeometries(numValidGeometries);

    for (unsigned i = 0; i < model.meshes_.size(); ++i)
    {
        aiMesh* mesh = model.meshes_[i];
        std::vector<VertexElement> elements = GetVertexElements(mesh, isSkinned);
        unsigned validFaces = GetNumValidFaces(mesh);
        if (!validFaces)
            continue;

        bool largeIndices;
        if (combineBuffers)
            largeIndices = model.totalIndices_ > 65535;
        else
            largeIndices = mesh->mNumVertices > 65535;

        // Create new buffers if necessary
        if (!combineBuffers || vbVector.empty())
        {
            vb = new VertexBuffer(context_.get());
            ib = new IndexBuffer(context_.get());

            if (combineBuffers)
            {
                ib->SetSize(model.totalIndices_, largeIndices);
                vb->SetSize(model.totalVertices_, elements);
            }
            else
            {
                ib->SetSize(validFaces * 3, largeIndices);
                vb->SetSize(mesh->mNumVertices, elements);
            }

            vbVector.push_back(vb);
            ibVector.push_back(ib);
            startVertexOffset = 0;
            startIndexOffset = 0;
        }

        // Get the world transform of the mesh for baking into the vertices
        Matrix3x4 vertexTransform;
        Matrix3 normalTransform;
        Vector3 pos, scale;
        Quaternion rot;
        GetPosRotScale(GetMeshBakingTransform(model.meshNodes_[i], model.rootNode_), pos, rot, scale);
        vertexTransform = Matrix3x4(pos, rot, scale);
        normalTransform = rot.RotationMatrix();

        SharedPtr<Geometry> geom(new Geometry(context_.get()));

        PrintLine("Writing geometry " + QString(i) + " with " + QString(mesh->mNumVertices) + " vertices " +
            QString(validFaces * 3) + " indices");

        if (model.bones_.size() > 0 && !mesh->HasBones())
            PrintLine("Warning: model has bones but geometry " + QString(i) + " has no skinning information");

        unsigned char* vertexData = vb->GetShadowData();
        unsigned char* indexData = ib->GetShadowData();

        // Build the index data
        if (!largeIndices)
        {
            unsigned short* dest = (unsigned short*)indexData + startIndexOffset;
            for (unsigned j = 0; j < mesh->mNumFaces; ++j)
                WriteShortIndices(dest, mesh, j, startVertexOffset);
        }
        else
        {
            unsigned* dest = (unsigned*)indexData + startIndexOffset;
            for (unsigned j = 0; j < mesh->mNumFaces; ++j)
                WriteLargeIndices(dest, mesh, j, startVertexOffset);
        }

        // Build the vertex data
        // If there are bones, get blend data
        std::vector<std::vector<unsigned char> > blendIndices;
        std::vector<std::vector<float> > blendWeights;
        std::vector<unsigned> boneMappings;
        if (model.bones_.size())
            GetBlendData(model, mesh, model.meshNodes_[i], boneMappings, blendIndices, blendWeights);

        auto* dest = (float*)(vertexData + startVertexOffset * vb->GetVertexSize());
        for (unsigned j = 0; j < mesh->mNumVertices; ++j)
            WriteVertex(dest, mesh, j, isSkinned, box, vertexTransform, normalTransform, blendIndices, blendWeights);

        // Calculate the geometry center
        Vector3 center = Vector3::ZERO;
        if (validFaces)
        {
            for (unsigned j = 0; j < mesh->mNumFaces; ++j)
            {
                if (mesh->mFaces[j].mNumIndices == 3)
                {
                    center += vertexTransform * ToVector3(mesh->mVertices[mesh->mFaces[j].mIndices[0]]);
                    center += vertexTransform * ToVector3(mesh->mVertices[mesh->mFaces[j].mIndices[1]]);
                    center += vertexTransform * ToVector3(mesh->mVertices[mesh->mFaces[j].mIndices[2]]);
                }
            }

            center /= (float)validFaces * 3;
        }

        // Define the geometry
        geom->SetIndexBuffer(ib);
        geom->SetVertexBuffer(0, vb);
        geom->SetDrawRange(TRIANGLE_LIST, startIndexOffset, validFaces * 3, true);
        outModel->SetNumGeometryLodLevels(destGeomIndex, 1);
        outModel->SetGeometry(destGeomIndex, 0, geom);
        outModel->SetGeometryCenter(destGeomIndex, center);
        if (model.bones_.size() > maxBones_)
            allBoneMappings.push_back(boneMappings);

        startVertexOffset += mesh->mNumVertices;
        startIndexOffset += validFaces * 3;
        ++destGeomIndex;
    }

    // Define the model buffers and bounding box
    std::vector<unsigned> emptyMorphRange;
    outModel->SetVertexBuffers(vbVector, emptyMorphRange, emptyMorphRange);
    outModel->SetIndexBuffers(ibVector);
    outModel->SetBoundingBox(box);

    // Build skeleton if necessary
    if (model.bones_.size() && model.rootBone_)
    {
        PrintLine("Writing skeleton with " + QString::number(model.bones_.size()) + " bones, rootbone " +
            FromAIString(model.rootBone_->mName));

        Skeleton skeleton;
        std::vector<Bone>& bones = skeleton.GetModifiableBones();

        for (unsigned i = 0; i < model.bones_.size(); ++i)
        {
            aiNode* boneNode = model.bones_[i];
            QString boneName(FromAIString(boneNode->mName));

            Bone newBone;
            newBone.name_ = boneName;

            aiMatrix4x4 transform = boneNode->mTransformation;
            // Make the root bone transform relative to the model's root node, if it is not already
            // (in case there are nodes between that are not accounted for otherwise)
            if (boneNode == model.rootBone_)
                transform = GetDerivedTransform(boneNode, model.rootNode_, false);

            GetPosRotScale(transform, newBone.initialPosition_, newBone.initialRotation_, newBone.initialScale_);

            // Get offset information if exists
            newBone.offsetMatrix_ = GetOffsetMatrix(model, boneName);
            newBone.radius_ = model.boneRadii_[i];
            newBone.boundingBox_ = model.boneHitboxes_[i];
            newBone.collisionMask_ = BONECOLLISION_SPHERE | BONECOLLISION_BOX;
            newBone.parentIndex_ = i;
            bones.push_back(newBone);
        }
        // Set the bone hierarchy
        for (unsigned i = 1; i < model.bones_.size(); ++i)
        {
            QString parentName = FromAIString(model.bones_[i]->mParent->mName);
            for (unsigned j = 0; j < bones.size(); ++j)
            {
                if (bones[j].name_ == parentName)
                {
                    bones[i].parentIndex_ = j;
                    break;
                }
            }
        }

        outModel->SetSkeleton(skeleton);
        if (model.bones_.size() > maxBones_)
            outModel->SetGeometryBoneMappings(allBoneMappings);
    }

    File outFile(context_.get());
    if (!outFile.Open(model.outName_, FILE_WRITE))
        ErrorExit("Could not open output file " + model.outName_);
    outModel->Save(outFile);

    // If exporting materials, also save material list for use by the editor
    if (!noMaterials_ && saveMaterialList_)
    {
        QString materialListName = ReplaceExtension(model.outName_, ".txt");
        File listFile(context_.get());
        if (listFile.Open(materialListName, FILE_WRITE))
        {
            for (unsigned i = 0; i < model.meshes_.size(); ++i)
                listFile.WriteLine(GetMeshMaterialName(model.meshes_[i]));
        }
        else
            PrintLine("Warning: could not write material list file " + materialListName);
    }
}

void BuildAndSaveAnimations(OutModel* model)
{
    // extrapolate anim
    ExtrapolatePivotlessAnimation(model);

    // build and save anim
    const std::vector<aiAnimation*>& animations = model ? model->animations_ : sceneAnimations_;

    for (unsigned i = 0; i < animations.size(); ++i)
    {
        aiAnimation* anim = animations[i];

        auto duration = (float)anim->mDuration;
        QString animName = FromAIString(anim->mName);
        QString animOutName;

        float thisImportEndTime = importEndTime_;
        float thisImportStartTime = importStartTime_;

        // If no animation split specified, set the end time to duration
        if (thisImportEndTime == 0.0f)
            thisImportEndTime = duration;

        if (animName.isEmpty())
            animName = "Anim" + QString(i + 1);

        QString outName = model ? model->outName_ : outName_;

        if (context_->m_FileSystem->DirExists(outName))
        {
            animName = SanitateAssetName(animName);
            outName = AddTrailingSlash(outName);
        }
        else
            animName = GetFileName(outName) + "_" + SanitateAssetName(animName);

        animOutName = GetPath(outName) + animName + ".ani";

        auto ticksPerSecond = (float)anim->mTicksPerSecond;
        // If ticks per second not specified, it's probably a .X file. In this case use the default tick rate
        if (ticksPerSecond < M_EPSILON)
            ticksPerSecond = defaultTicksPerSecond_;
        float tickConversion = 1.0f / ticksPerSecond;

        // find out the start time of animation from each channel's first keyframe for adjusting the keyframe times
        // to start from zero
        float startTime = duration;
        for (unsigned j = 0; j < anim->mNumChannels; ++j)
        {
            aiNodeAnim* channel = anim->mChannels[j];
            if (channel->mNumPositionKeys > 0)
                startTime = Min(startTime, (float)channel->mPositionKeys[0].mTime);
            if (channel->mNumRotationKeys > 0)
                startTime = Min(startTime, (float)channel->mRotationKeys[0].mTime);
            if (channel->mNumScalingKeys > 0)
                startTime = Min(startTime, (float)channel->mScalingKeys[0].mTime);
        }
        if (startTime > thisImportStartTime)
            thisImportStartTime = startTime;
        duration = thisImportEndTime - thisImportStartTime;

        SharedPtr<Animation> outAnim(new Animation(context_.get()));
        outAnim->SetAnimationName(animName);
        outAnim->SetLength(duration * tickConversion);

        PrintLine(QString("Writing animation %1 length %2").arg(animName).arg(outAnim->GetLength()));
        for (unsigned j = 0; j < anim->mNumChannels; ++j)
        {
            aiNodeAnim* channel = anim->mChannels[j];
            QString channelName = FromAIString(channel->mNodeName);
            aiNode* boneNode = nullptr;

            if (model)
            {
                unsigned boneIndex;
                int pos = channelName.indexOf("_$AssimpFbx$");

                if (!suppressFbxPivotNodes_ || pos == -1)
                {
                    boneIndex = GetBoneIndex(*model, channelName);
                    if (boneIndex == M_MAX_UNSIGNED)
                    {
                        PrintLine("Warning: skipping animation track " + channelName + " not found in model skeleton");
                        outAnim->RemoveTrack(channelName);
                        continue;
                    }
                    boneNode = model->bones_[boneIndex];
                }
                else
                {
                    channelName = channelName.mid(0, pos);

                    // every first $fbx animation channel for a bone will consolidate other $fbx animation to a single channel
                    // skip subsequent $fbx animation channel for the same bone
                    if (outAnim->GetTrack(channelName) != nullptr)
                        continue;

                    boneIndex = GetPivotlessBoneIndex(*model, channelName);
                    if (boneIndex == M_MAX_UNSIGNED)
                    {
                        PrintLine("Warning: skipping animation track " + channelName + " not found in model skeleton");
                        outAnim->RemoveTrack(channelName);
                        continue;
                    }

                    boneNode = model->pivotlessBones_[boneIndex];
                }
            }
            else
            {
                boneNode = GetNode(channelName, scene_->mRootNode);
                if (!boneNode)
                {
                    PrintLine("Warning: skipping animation track " + channelName + " whose scene node was not found");
                    outAnim->RemoveTrack(channelName);
                    continue;
                }
            }

            // To export single frame animation, check if first key frame is identical to bone transformation
            aiVector3D bonePos, boneScale;
            aiQuaternion boneRot;
            boneNode->mTransformation.Decompose(boneScale, boneRot, bonePos);

            bool posEqual = true;
            bool scaleEqual = true;
            bool rotEqual = true;

            if (channel->mNumPositionKeys > 0 && !ToVector3(bonePos).Equals(ToVector3(channel->mPositionKeys[0].mValue)))
                posEqual = false;
            if (channel->mNumScalingKeys > 0 && !ToVector3(boneScale).Equals(ToVector3(channel->mScalingKeys[0].mValue)))
                scaleEqual = false;
            if (channel->mNumRotationKeys > 0 && !ToQuaternion(boneRot).Equals(ToQuaternion(channel->mRotationKeys[0].mValue)))
                rotEqual = false;

            AnimationTrack* track = outAnim->CreateTrack(channelName);

            // Check which channels are used
            track->channelMask_ = CHANNEL_NONE;
            if (channel->mNumPositionKeys > 1 || !posEqual)
                track->channelMask_ |= CHANNEL_POSITION;
            if (channel->mNumRotationKeys > 1 || !rotEqual)
                track->channelMask_ |= CHANNEL_ROTATION;
            if (channel->mNumScalingKeys > 1 || !scaleEqual)
                track->channelMask_ |= CHANNEL_SCALE;
            // Check for redundant identity scale in all keyframes and remove in that case
            if (track->channelMask_ & CHANNEL_SCALE)
            {
                bool redundantScale = true;
                for (unsigned k = 0; k < channel->mNumScalingKeys; ++k)
                {
                    float SCALE_EPSILON = 0.000001f;
                    Vector3 scaleVec = ToVector3(channel->mScalingKeys[k].mValue);
                    if (fabsf(scaleVec.x_ - 1.0f) >= SCALE_EPSILON || fabsf(scaleVec.y_ - 1.0f) >= SCALE_EPSILON ||
                        fabsf(scaleVec.z_ - 1.0f) >= SCALE_EPSILON)
                    {
                        redundantScale = false;
                        break;
                    }
                }
                if (redundantScale)
                    track->channelMask_ &= ~CHANNEL_SCALE;
            }

            if (!track->channelMask_)
            {
                PrintLine("Warning: skipping animation track " + channelName + " with no keyframes");
                outAnim->RemoveTrack(channelName);
                continue;
            }

            // Currently only same amount of keyframes is supported
            // Note: should also check the times of individual keyframes for match
            if ((channel->mNumPositionKeys > 1 && channel->mNumRotationKeys > 1 && channel->mNumPositionKeys != channel->mNumRotationKeys) ||
                (channel->mNumPositionKeys > 1 && channel->mNumScalingKeys > 1 && channel->mNumPositionKeys != channel->mNumScalingKeys) ||
                (channel->mNumRotationKeys > 1 && channel->mNumScalingKeys > 1 && channel->mNumRotationKeys != channel->mNumScalingKeys))
            {
                PrintLine("Warning: differing amounts of channel keyframes, skipping animation track " + channelName);
                outAnim->RemoveTrack(channelName);
                continue;
            }

            unsigned keyFrames = channel->mNumPositionKeys;
            if (channel->mNumRotationKeys > keyFrames)
                keyFrames = channel->mNumRotationKeys;
            if (channel->mNumScalingKeys > keyFrames)
                keyFrames = channel->mNumScalingKeys;

            std::vector<AnimationKeyFrame> toAdd;
            toAdd.reserve(keyFrames);
            for (unsigned k = 0; k < keyFrames; ++k)
            {
                AnimationKeyFrame kf;
                kf.time_ = 0.0f;
                kf.position_ = Vector3::ZERO;
                kf.rotation_ = Quaternion::IDENTITY;
                kf.scale_ = Vector3::ONE;

                // Get time for the keyframe. Adjust with animation's start time
                if (track->channelMask_ & CHANNEL_POSITION && k < channel->mNumPositionKeys)
                    kf.time_ = ((float)channel->mPositionKeys[k].mTime - startTime);
                else if (track->channelMask_ & CHANNEL_ROTATION && k < channel->mNumRotationKeys)
                    kf.time_ = ((float)channel->mRotationKeys[k].mTime - startTime);
                else if (track->channelMask_ & CHANNEL_SCALE && k < channel->mNumScalingKeys)
                    kf.time_ = ((float)channel->mScalingKeys[k].mTime - startTime);

                // Make sure time stays positive
                kf.time_ = Max(kf.time_, 0.0f);

                // Start with the bone's base transform
                aiMatrix4x4 boneTransform = boneNode->mTransformation;
                aiVector3D pos, scale;
                aiQuaternion rot;
                boneTransform.Decompose(scale, rot, pos);
                // Then apply the active channels
                if (track->channelMask_ & CHANNEL_POSITION && k < channel->mNumPositionKeys)
                    pos = channel->mPositionKeys[k].mValue;
                if (track->channelMask_ & CHANNEL_ROTATION && k < channel->mNumRotationKeys)
                    rot = channel->mRotationKeys[k].mValue;
                if (track->channelMask_ & CHANNEL_SCALE && k < channel->mNumScalingKeys)
                    scale = channel->mScalingKeys[k].mValue;

                // If root bone, transform with nodes in between model root node (if any)
                if (model && boneNode == model->rootBone_)
                {
                    aiMatrix4x4 transMat, scaleMat, rotMat;
                    aiMatrix4x4::Translation(pos, transMat);
                    aiMatrix4x4::Scaling(scale, scaleMat);
                    rotMat = aiMatrix4x4(rot.GetMatrix());
                    aiMatrix4x4 tform = transMat * rotMat * scaleMat;
                    aiMatrix4x4 tformOld = tform;
                    tform = GetDerivedTransform(tform, boneNode, model->rootNode_, false);
                    // Do not decompose if did not actually change
                    if (tform != tformOld)
                        tform.Decompose(scale, rot, pos);
                }

                if (track->channelMask_ & CHANNEL_POSITION)
                    kf.position_ = ToVector3(pos);
                if (track->channelMask_ & CHANNEL_ROTATION)
                    kf.rotation_ = ToQuaternion(rot);
                if (track->channelMask_ & CHANNEL_SCALE)
                    kf.scale_ = ToVector3(scale);
                if (kf.time_ >= thisImportStartTime && kf.time_ <= thisImportEndTime)
                {
                    kf.time_ = (kf.time_ - thisImportStartTime) * tickConversion;
                    toAdd.push_back(kf);
                }
            }
            track->SetAllKeyFrames(toAdd.data(), toAdd.size());
        }

        File outFile(context_.get());
        if (!outFile.Open(animOutName, FILE_WRITE))
            ErrorExit("Could not open output file " + animOutName);
        outAnim->Save(outFile);
    }
}

void ExportScene(const QString& outName, bool asPrefab)
{
    OutScene outScene;
    outScene.outName_ = outName;
    outScene.rootNode_ = rootNode_;

    if (useSubdirs_)
        context_->m_FileSystem->CreateDir(resourcePath_ + "Models");

    CollectSceneModels(outScene, rootNode_);

    // Save models, their material lists and animations
    for (unsigned i = 0; i < outScene.models_.size(); ++i)
        BuildAndSaveModel(outScene.models_[i]);

    // Save scene-global animations
    if (!noAnimations_)
    {
        CollectAnimations();
        BuildAndSaveAnimations();
    }

    // Save scene
    BuildAndSaveScene(outScene, asPrefab);
}

void CollectSceneModels(OutScene& scene, aiNode* node)
{
    std::vector<std::pair<aiNode*, aiMesh*> > meshes;
    GetMeshesUnderNode(meshes, node);

    if (meshes.size())
    {
        OutModel model;
        model.rootNode_ = node;
        model.outName_ = resourcePath_ + (useSubdirs_ ? "Models/" : "") + SanitateAssetName(FromAIString(node->mName)) + ".mdl";
        for (unsigned i = 0; i < meshes.size(); ++i)
        {
            aiMesh* mesh = meshes[i].second;
            unsigned meshIndex = GetMeshIndex(mesh);
            model.meshIndices_.insert(meshIndex);
            model.meshes_.push_back(mesh);
            model.meshNodes_.push_back(meshes[i].first);
            model.totalVertices_ += mesh->mNumVertices;
            model.totalIndices_ += GetNumValidFaces(mesh) * 3;
        }

        // Check if a model with identical mesh indices already exists. If yes, do not export twice
        bool unique = true;
        if (checkUniqueModel_)
        {
            for (unsigned i = 0; i < scene.models_.size(); ++i)
            {
                if (scene.models_[i].meshIndices_ == model.meshIndices_)
                {
                    PrintLine("Added node " + FromAIString(node->mName));
                    scene.nodes_.push_back(node);
                    scene.nodeModelIndices_.push_back(i);
                    unique = false;
                    break;
                }
            }
        }
        if (unique)
        {
            PrintLine("Added model " + model.outName_);
            PrintLine("Added node " + FromAIString(node->mName));
            CollectBones(model);
            BuildBoneCollisionInfo(model);
            if (!noAnimations_)
            {
                CollectAnimations(&model);
                BuildAndSaveAnimations(&model);
            }

            scene.models_.push_back(model);
            scene.nodes_.push_back(node);
            scene.nodeModelIndices_.push_back(scene.models_.size() - 1);
        }
    }

    for (unsigned i = 0; i < node->mNumChildren; ++i)
        CollectSceneModels(scene, node->mChildren[i]);
}

void CreateHierarchy(Scene* scene, aiNode* srcNode, HashMap<aiNode*, Node*>& nodeMapping)
{
    CreateSceneNode(scene, srcNode, nodeMapping);
    for (unsigned i = 0; i < srcNode->mNumChildren; ++i)
        CreateHierarchy(scene, srcNode->mChildren[i], nodeMapping);
}

Node* CreateSceneNode(Scene* scene, aiNode* srcNode, HashMap<aiNode*, Node*>& nodeMapping)
{
    if (hashContains(nodeMapping,srcNode))
        return nodeMapping[srcNode];
    // Flatten hierarchy if requested
    if (noHierarchy_)
    {
        Node* outNode = scene->CreateChild(FromAIString(srcNode->mName), localIDs_ ? LOCAL : REPLICATED);
        Vector3 pos, scale;
        Quaternion rot;
        GetPosRotScale(GetDerivedTransform(srcNode, rootNode_), pos, rot, scale);
        outNode->SetTransform(pos, rot, scale);
        nodeMapping[srcNode] = outNode;

        return outNode;
    }

    if (srcNode == rootNode_ || !srcNode->mParent)
    {
        Node* outNode = scene->CreateChild(FromAIString(srcNode->mName), localIDs_ ? LOCAL : REPLICATED);
        Vector3 pos, scale;
        Quaternion rot;
        GetPosRotScale(srcNode->mTransformation, pos, rot, scale);
        outNode->SetTransform(pos, rot, scale);
        nodeMapping[srcNode] = outNode;

        return outNode;
    }
    else
    {
        // Ensure the existence of the parent chain as in the original file
        if (!hashContains(nodeMapping,srcNode->mParent))
            CreateSceneNode(scene, srcNode->mParent, nodeMapping);

        Node* parent = nodeMapping[srcNode->mParent];
        Node* outNode = parent->CreateChild(FromAIString(srcNode->mName), localIDs_ ? LOCAL : REPLICATED);
        Vector3 pos, scale;
        Quaternion rot;
        GetPosRotScale(srcNode->mTransformation, pos, rot, scale);
        outNode->SetTransform(pos, rot, scale);
        nodeMapping[srcNode] = outNode;

        return outNode;
    }
}

void BuildAndSaveScene(OutScene& scene, bool asPrefab)
{
    if (!asPrefab)
        PrintLine("Writing scene");
    else
        PrintLine("Writing node hierarchy");

    SharedPtr<Scene> outScene(new Scene(context_.get()));

    if (!asPrefab)
    {
        #ifdef LUTEFISK3D_PHYSICS
        /// \todo Make the physics properties configurable
        outScene->CreateComponent<PhysicsWorld>();
        #endif

        /// \todo Make the octree properties configurable, or detect from the scene contents
        outScene->CreateComponent<Octree>();

        outScene->CreateComponent<DebugRenderer>();

        if (createZone_)
        {
            Node* zoneNode = outScene->CreateChild("Zone", localIDs_ ? LOCAL : REPLICATED);
            auto* zone = zoneNode->CreateComponent<Zone>();
            zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.f));
            zone->SetAmbientColor(Color(0.25f, 0.25f, 0.25f));

            // Create default light only if scene does not define them
            if (!scene_->HasLights())
            {
                Node* lightNode = outScene->CreateChild("GlobalLight", localIDs_ ? LOCAL : REPLICATED);
                auto* light = lightNode->CreateComponent<Light>();
                light->SetLightType(LIGHT_DIRECTIONAL);
                lightNode->SetRotation(Quaternion(60.0f, 30.0f, 0.0f));
            }
        }
    }

    auto* cache = context_->m_ResourceCache.get();

    HashMap<aiNode*, Node*> nodeMapping;

    Node* outRootNode = nullptr;
    if (asPrefab)
        outRootNode = CreateSceneNode(outScene, rootNode_, nodeMapping);
    else
    {
        // If not saving as a prefab, associate the root node with the scene first to prevent unnecessary creation of a root
        // However do not do that if the root node does not have an identity matrix, or itself contains a model
        // (models at the Urho scene root are not preferable)
        if (ToMatrix3x4(rootNode_->mTransformation).Equals(Matrix3x4::IDENTITY) &&
                scene.nodes_.end()==std::find(scene.nodes_.begin(),scene.nodes_.end(),rootNode_))
           nodeMapping[rootNode_] = outScene;
    }

    // If is allowed to export empty nodes, export the full Assimp node hierarchy first
    if (!noHierarchy_ && !noEmptyNodes_)
        CreateHierarchy(outScene, rootNode_, nodeMapping);

    // Create geometry nodes
    for (unsigned i = 0; i < scene.nodes_.size(); ++i)
    {
        const OutModel& model = scene.models_[scene.nodeModelIndices_[i]];
        Node* modelNode = CreateSceneNode(outScene, scene.nodes_[i], nodeMapping);
        auto* staticModel =
            static_cast<StaticModel*>(
                model.bones_.empty() ? modelNode->CreateComponent<StaticModel>() : modelNode->CreateComponent<AnimatedModel>());

        // Create a dummy model so that the reference can be stored
        QString modelName = (useSubdirs_ ? "Models/" : "") + GetFileNameAndExtension(model.outName_);
        if (!cache->Exists(modelName))
        {
            auto* dummyModel = new Model(context_.get());
            dummyModel->SetName(modelName);
            dummyModel->SetNumGeometries(model.meshes_.size());
            cache->AddManualResource(dummyModel);
        }
        staticModel->SetModel(cache->GetResource<Model>(modelName));

        // Set materials if they are known
        for (unsigned j = 0; j < model.meshes_.size(); ++j)
        {
            QString matName = GetMeshMaterialName(model.meshes_[j]);
            // Create a dummy material so that the reference can be stored
            if (!cache->Exists(matName))
            {
                auto* dummyMat = new Material(context_.get());
                dummyMat->SetName(matName);
                cache->AddManualResource(dummyMat);
            }
            staticModel->SetMaterial(j, cache->GetResource<Material>(matName));
        }
    }

    // Create lights
    if (!asPrefab)
    {
        for (unsigned i = 0; i < scene_->mNumLights; ++i)
        {
            aiLight* light = scene_->mLights[i];
            aiNode* lightNode = GetNode(FromAIString(light->mName), rootNode_, true);
            if (!lightNode)
                continue;
            Node* outNode = CreateSceneNode(outScene, lightNode, nodeMapping);

            Vector3 lightAdjustPosition = ToVector3(light->mPosition);
            Vector3 lightAdjustDirection = ToVector3(light->mDirection);
            // If light is not aligned at the scene node, an adjustment node needs to be created
            if (!lightAdjustPosition.Equals(Vector3::ZERO) || (light->mType != aiLightSource_POINT &&
                !lightAdjustDirection.Equals(Vector3::FORWARD)))
            {
                outNode = outNode->CreateChild("LightAdjust");
                outNode->SetPosition(lightAdjustPosition);
                outNode->SetDirection(lightAdjustDirection);
            }

            auto* outLight = outNode->CreateComponent<Light>();
            outLight->SetColor(Color(light->mColorDiffuse.r, light->mColorDiffuse.g, light->mColorDiffuse.b));

            switch (light->mType)
            {
            case aiLightSource_DIRECTIONAL:
                outLight->SetLightType(LIGHT_DIRECTIONAL);
                break;
            case aiLightSource_SPOT:
                outLight->SetLightType(LIGHT_SPOT);
                outLight->SetFov(light->mAngleOuterCone * 0.5f * M_RADTODEG);
                break;
            case aiLightSource_POINT:
                outLight->SetLightType(LIGHT_POINT);
                break;
            default:
                break;
            }

            // Calculate range from attenuation parameters so that light intensity has been reduced to 10% at that distance
            if (light->mType != aiLightSource_DIRECTIONAL)
            {
                float a = light->mAttenuationQuadratic;
                float b = light->mAttenuationLinear;
                float c = -10.0f;
                if (!Equals(a, 0.0f))
                {
                    float root1 = (-b + sqrtf(b * b - 4.0f * a * c)) / (2.0f * a);
                    float root2 = (-b - sqrtf(b * b - 4.0f * a * c)) / (2.0f * a);
                    outLight->SetRange(Max(root1, root2));
                }
                else if (!Equals(b, 0.0f))
                    outLight->SetRange(-c / b);
            }
        }
    }

    File file(context_.get());
    if (!file.Open(scene.outName_, FILE_WRITE))
        ErrorExit("Could not open output file " + scene.outName_);
    if (!asPrefab)
    {
        if (saveBinary_)
            outScene->Save(file);
        else if (saveJson_)
            outScene->SaveJSON(file);
        else
            outScene->SaveXML(file);
    }
    else
    {
        if (saveBinary_)
            outRootNode->Save(file);
        else if (saveJson_)
            outRootNode->SaveJSON(file);
        else
            outRootNode->SaveXML(file);
    }
}

void ExportMaterials(HashSet<QString>& usedTextures)
{
    if (useSubdirs_)
        context_->m_FileSystem->CreateDir(resourcePath_ + "Materials");

    for (unsigned i = 0; i < scene_->mNumMaterials; ++i)
        BuildAndSaveMaterial(scene_->mMaterials[i], usedTextures);
}

void BuildAndSaveMaterial(aiMaterial* material, HashSet<QString>& usedTextures)
{
    aiString matNameStr;
    material->Get(AI_MATKEY_NAME, matNameStr);
    QString matName = SanitateAssetName(FromAIString(matNameStr));
    if (matName.trimmed().isEmpty())
        matName = GenerateMaterialName(material);

    // Do not actually create a material instance, but instead craft an xml file manually
    XMLFile outMaterial(context_.get());
    XMLElement materialElem = outMaterial.CreateRoot("material");

    QString diffuseTexName;
    QString normalTexName;
    QString specularTexName;
    QString lightmapTexName;
    QString emissiveTexName;
    Color diffuseColor = Color::WHITE;
    Color specularColor;
    Color emissiveColor = Color::BLACK;
    bool hasAlpha = false;
    bool twoSided = false;
    float specPower = 1.0f;

    aiString stringVal;
    float floatVal;
    int intVal;
    aiColor3D colorVal;

    if (material->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), stringVal) == AI_SUCCESS)
        diffuseTexName = GetFileNameAndExtension(FromAIString(stringVal));
    if (material->Get(AI_MATKEY_TEXTURE(aiTextureType_NORMALS, 0), stringVal) == AI_SUCCESS)
        normalTexName = GetFileNameAndExtension(FromAIString(stringVal));
    if (material->Get(AI_MATKEY_TEXTURE(aiTextureType_SPECULAR, 0), stringVal) == AI_SUCCESS)
        specularTexName = GetFileNameAndExtension(FromAIString(stringVal));
    if (material->Get(AI_MATKEY_TEXTURE(aiTextureType_LIGHTMAP, 0), stringVal) == AI_SUCCESS)
        lightmapTexName = GetFileNameAndExtension(FromAIString(stringVal));
    if (material->Get(AI_MATKEY_TEXTURE(aiTextureType_EMISSIVE, 0), stringVal) == AI_SUCCESS)
        emissiveTexName = GetFileNameAndExtension(FromAIString(stringVal));
    if (!noMaterialDiffuseColor_)
    {
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, colorVal) == AI_SUCCESS)
            diffuseColor = Color(colorVal.r, colorVal.g, colorVal.b);
    }
    if (material->Get(AI_MATKEY_COLOR_SPECULAR, colorVal) == AI_SUCCESS)
        specularColor = Color(colorVal.r, colorVal.g, colorVal.b);
    if (!emissiveAO_)
    {
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, colorVal) == AI_SUCCESS)
            emissiveColor = Color(colorVal.r, colorVal.g, colorVal.b);
    }
    if (material->Get(AI_MATKEY_OPACITY, floatVal) == AI_SUCCESS)
    {
        /// \hack New Assimp behavior - some materials may return 0 opacity, which is invisible.
        /// Revert to full opacity in that case
        if (floatVal < M_EPSILON)
            floatVal = 1.0f;

        if (floatVal < 1.0f)
            hasAlpha = true;
        diffuseColor.a_ = floatVal;
    }
    if (material->Get(AI_MATKEY_SHININESS, floatVal) == AI_SUCCESS)
        specPower = floatVal;
    if (material->Get(AI_MATKEY_TWOSIDED, intVal) == AI_SUCCESS)
        twoSided = (intVal != 0);

    QString techniqueName = "Techniques/NoTexture";
    if (!diffuseTexName.isEmpty())
    {
        techniqueName = "Techniques/Diff";
        if (!normalTexName.isEmpty())
            techniqueName += "Normal";
        if (!specularTexName.isEmpty())
            techniqueName += "Spec";
        // For now lightmap does not coexist with normal & specular
        if (normalTexName.isEmpty() && specularTexName.isEmpty() && !lightmapTexName.isEmpty())
            techniqueName += "LightMap";
        if (lightmapTexName.isEmpty() && !emissiveTexName.isEmpty())
            techniqueName += emissiveAO_ ? "AO" : "Emissive";
    }
    if (hasAlpha)
        techniqueName += "Alpha";

    XMLElement techniqueElem = materialElem.CreateChild("technique");
    techniqueElem.SetString("name", techniqueName + ".xml");

    if (!diffuseTexName.isEmpty())
    {
        XMLElement diffuseElem = materialElem.CreateChild("texture");
        diffuseElem.SetString("unit", "diffuse");
        diffuseElem.SetString("name", GetMaterialTextureName(diffuseTexName));
        usedTextures.insert(diffuseTexName);
    }
    if (!normalTexName.isEmpty())
    {
        XMLElement normalElem = materialElem.CreateChild("texture");
        normalElem.SetString("unit", "normal");
        normalElem.SetString("name", GetMaterialTextureName(normalTexName));
        usedTextures.insert(normalTexName);
    }
    if (!specularTexName.isEmpty())
    {
        XMLElement specularElem = materialElem.CreateChild("texture");
        specularElem.SetString("unit", "specular");
        specularElem.SetString("name", GetMaterialTextureName(specularTexName));
        usedTextures.insert(specularTexName);
    }
    if (!lightmapTexName.isEmpty())
    {
        XMLElement lightmapElem = materialElem.CreateChild("texture");
        lightmapElem.SetString("unit", "emissive");
        lightmapElem.SetString("name", GetMaterialTextureName(lightmapTexName));
        usedTextures.insert(lightmapTexName);
    }
    if (!emissiveTexName.isEmpty())
    {
        XMLElement emissiveElem = materialElem.CreateChild("texture");
        emissiveElem.SetString("unit", "emissive");
        emissiveElem.SetString("name", GetMaterialTextureName(emissiveTexName));
        usedTextures.insert(emissiveTexName);
    }

    XMLElement diffuseColorElem = materialElem.CreateChild("parameter");
    diffuseColorElem.SetString("name", "MatDiffColor");
    diffuseColorElem.SetColor("value", diffuseColor);
    XMLElement specularElem = materialElem.CreateChild("parameter");
    specularElem.SetString("name", "MatSpecColor");
    specularElem.SetVector4("value", Vector4(specularColor.r_, specularColor.g_, specularColor.b_, specPower));
    XMLElement emissiveColorElem = materialElem.CreateChild("parameter");
    emissiveColorElem.SetString("name", "MatEmissiveColor");
    emissiveColorElem.SetColor("value", emissiveColor);

    if (twoSided)
    {
        XMLElement cullElem = materialElem.CreateChild("cull");
        XMLElement shadowCullElem = materialElem.CreateChild("shadowcull");
        cullElem.SetString("value", "none");
        shadowCullElem.SetString("value", "none");
    }

    auto* fileSystem = context_->m_FileSystem.get();

    QString outFileName = resourcePath_ + (useSubdirs_ ? "Materials/" : "" ) + matName + ".xml";
    if (noOverwriteMaterial_ && fileSystem->FileExists(outFileName))
    {
        PrintLine("Skipping save of existing material " + matName);
        return;
    }

    PrintLine("Writing material " + matName);

    File outFile(context_.get());
    if (!outFile.Open(outFileName, FILE_WRITE))
        ErrorExit("Could not open output file " + outFileName);
    outMaterial.Save(outFile);
}

void CopyTextures(const HashSet<QString>& usedTextures, const QString& sourcePath)
{
    auto* fileSystem = context_->m_FileSystem.get();

    if (useSubdirs_)
        fileSystem->CreateDir(resourcePath_ + "Textures");

    for (auto i = usedTextures.begin(); i != usedTextures.end(); ++i)
    {
        // Handle assimp embedded textures
        if (i->length() && i->at(0) == '*')
        {
            unsigned texIndex = i->midRef(1).toUInt();
            if (texIndex >= scene_->mNumTextures)
                PrintLine("Skipping out of range texture index " + QString::number(texIndex));
            else
            {
                aiTexture* tex = scene_->mTextures[texIndex];
                QString fullDestName = resourcePath_ + GenerateTextureName(texIndex);
                bool destExists = fileSystem->FileExists(fullDestName);
                if (destExists && noOverwriteTexture_)
                {
                    PrintLine("Skipping copy of existing embedded texture " + GetFileNameAndExtension(fullDestName));
                    continue;
                }
                // Encoded texture
                if (!tex->mHeight)
                {
                    PrintLine("Saving embedded texture " + GetFileNameAndExtension(fullDestName));
                    File dest(context_.get(), fullDestName, FILE_WRITE);
                    dest.Write((const void*)tex->pcData, tex->mWidth);
                }
                // RGBA8 texture
                else
                {
                    PrintLine("Saving embedded RGBA texture " + GetFileNameAndExtension(fullDestName));
                    Image image(context_.get());
                    image.SetSize(tex->mWidth, tex->mHeight, 4);
                    memcpy(image.GetData(), (const void*)tex->pcData, (size_t)tex->mWidth * tex->mHeight * 4);
                    image.SavePNG(fullDestName);
                }
            }
        }
        else
        {
            QString fullSourceName = sourcePath + *i;
            QString fullDestName = resourcePath_ + (useSubdirs_ ? "Textures/" : "") + *i;

            if (!fileSystem->FileExists(fullSourceName))
            {
                PrintLine("Skipping copy of nonexisting material texture " + *i);
                continue;
            }
            {
                File test(context_.get(), fullSourceName);
                if (!test.GetSize())
                {
                    PrintLine("Skipping copy of zero-size material texture " + *i);
                    continue;
                }
            }

            bool destExists = fileSystem->FileExists(fullDestName);
            if (destExists && noOverwriteTexture_)
            {
                PrintLine("Skipping copy of existing texture " + *i);
                continue;
            }
            if (destExists && noOverwriteNewerTexture_ && fileSystem->GetLastModifiedTime(fullDestName) >
                fileSystem->GetLastModifiedTime(fullSourceName))
            {
                PrintLine("Skipping copying of material texture " + *i + ", destination is newer");
                continue;
            }

            PrintLine("Copying material texture " + *i);
            fileSystem->Copy(fullSourceName, fullDestName);
        }
    }
}

void CombineLods(const std::vector<float>& lodDistances, const std::vector<QString>& modelNames, const QString& outName)
{
    // Load models
    std::vector<SharedPtr<Model> > srcModels;
    for (unsigned i = 0; i < modelNames.size(); ++i)
    {
        PrintLine(QString("Reading LOD level %1: model %2 distance %3").arg(i).arg(modelNames[i]).arg(lodDistances[i]));
        File srcFile(context_.get());
        srcFile.Open(modelNames[i]);
        SharedPtr<Model> srcModel(new Model(context_.get()));
        if (!srcModel->Load(srcFile))
            ErrorExit("Could not load input model " + modelNames[i]);
        srcModels.push_back(srcModel);
    }

    // Check that none of the models already has LOD levels
    for (unsigned i = 0; i < srcModels.size(); ++i)
    {
        for (unsigned j = 0; j < srcModels[i]->GetNumGeometries(); ++j)
        {
            if (srcModels[i]->GetNumGeometryLodLevels(j) > 1)
                ErrorExit(modelNames[i] + " already has multiple LOD levels defined");
        }
    }

    // Check for number of geometries (need to have same amount for now)
    for (unsigned i = 1; i < srcModels.size(); ++i)
    {
        if (srcModels[i]->GetNumGeometries() != srcModels[0]->GetNumGeometries())
            ErrorExit(modelNames[i] + " has different amount of geometries than " + modelNames[0]);
    }

    // If there are bones, check for compatibility (need to have exact match for now)
    for (unsigned i = 1; i < srcModels.size(); ++i)
    {
        if (srcModels[i]->GetSkeleton().GetNumBones() != srcModels[0]->GetSkeleton().GetNumBones())
            ErrorExit(modelNames[i] + " has different amount of bones than " + modelNames[0]);
        for (unsigned j = 0; j < srcModels[0]->GetSkeleton().GetNumBones(); ++j)
        {
            if (srcModels[i]->GetSkeleton().GetBone(j)->name_ != srcModels[0]->GetSkeleton().GetBone(j)->name_)
                ErrorExit(modelNames[i] + " has different bones than " + modelNames[0]);
        }
        if (srcModels[i]->GetGeometryBoneMappings() != srcModels[0]->GetGeometryBoneMappings())
            ErrorExit(modelNames[i] + " has different per-geometry bone mappings than " + modelNames[0]);
    }

    std::vector<SharedPtr<VertexBuffer> > vbVector;
    std::vector<SharedPtr<IndexBuffer> > ibVector;
    std::vector<unsigned> emptyMorphRange;

    // Create the final model
    SharedPtr<Model> outModel(new Model(context_.get()));
    outModel->SetNumGeometries(srcModels[0]->GetNumGeometries());
    for (unsigned i = 0; i < srcModels[0]->GetNumGeometries(); ++i)
    {
        outModel->SetNumGeometryLodLevels(i, srcModels.size());
        for (unsigned j = 0; j < srcModels.size(); ++j)
        {
            Geometry* geometry = srcModels[j]->GetGeometry(i, 0);
            geometry->SetLodDistance(lodDistances[j]);
            outModel->SetGeometry(i, j, geometry);

            for (unsigned k = 0; k < geometry->GetNumVertexBuffers(); ++k)
            {
                SharedPtr<VertexBuffer> vb(geometry->GetVertexBuffer(k));
                if (std::find(vbVector.begin(),vbVector.end(),vb)==vbVector.end())
                    vbVector.push_back(vb);
            }

            SharedPtr<IndexBuffer> ib(geometry->GetIndexBuffer());
            if (ibVector.end()==std::find(ibVector.begin(),ibVector.end(),ib))
                ibVector.push_back(ib);
        }
    }

    outModel->SetVertexBuffers(vbVector, emptyMorphRange, emptyMorphRange);
    outModel->SetIndexBuffers(ibVector);
    outModel->SetSkeleton(srcModels[0]->GetSkeleton());
    outModel->SetGeometryBoneMappings(srcModels[0]->GetGeometryBoneMappings());
    outModel->SetBoundingBox(srcModels[0]->GetBoundingBox());
    /// \todo Vertex morphs are ignored for now

    // Save the final model
    PrintLine("Writing output model");
    File outFile(context_.get());
    if (!outFile.Open(outName, FILE_WRITE))
        ErrorExit("Could not open output file " + outName);
    outModel->Save(outFile);
}

void GetMeshesUnderNode(std::vector<std::pair<aiNode*, aiMesh*> >& dest, aiNode* node)
{
    for (unsigned i = 0; i < node->mNumMeshes; ++i)
        dest.push_back(std::make_pair(node, scene_->mMeshes[node->mMeshes[i]]));
}

unsigned GetMeshIndex(aiMesh* mesh)
{
    for (unsigned i = 0; i < scene_->mNumMeshes; ++i)
    {
        if (scene_->mMeshes[i] == mesh)
            return i;
    }
    return M_MAX_UNSIGNED;
}

unsigned GetBoneIndex(OutModel& model, const QString& boneName)
{
    for (unsigned i = 0; i < model.bones_.size(); ++i)
    {
        if (boneName == model.bones_[i]->mName.data)
            return i;
    }
    return M_MAX_UNSIGNED;
}

aiBone* GetMeshBone(OutModel& model, const QString& boneName)
{
    for (unsigned i = 0; i < model.meshes_.size(); ++i)
    {
        aiMesh* mesh = model.meshes_[i];
        for (unsigned j = 0; j < mesh->mNumBones; ++j)
        {
            aiBone* bone = mesh->mBones[j];
            if (boneName == bone->mName.data)
                return bone;
        }
    }
    return nullptr;
}

Matrix3x4 GetOffsetMatrix(OutModel& model, const QString& boneName)
{
    for (unsigned i = 0; i < model.meshes_.size(); ++i)
    {
        aiMesh* mesh = model.meshes_[i];
        aiNode* node = model.meshNodes_[i];
        for (unsigned j = 0; j < mesh->mNumBones; ++j)
        {
            aiBone* bone = mesh->mBones[j];
            if (boneName == bone->mName.data)
            {
                aiMatrix4x4 offset = bone->mOffsetMatrix;
                aiMatrix4x4 nodeDerivedInverse = GetMeshBakingTransform(node, model.rootNode_);
                nodeDerivedInverse.Inverse();
                offset *= nodeDerivedInverse;
                return ToMatrix3x4(offset);
            }
        }
    }

    // Fallback for rigid skinning for which actual offset matrix information doesn't exist
    for (unsigned i = 0; i < model.meshes_.size(); ++i)
    {
        aiMesh* mesh = model.meshes_[i];
        aiNode* node = model.meshNodes_[i];
        if (!mesh->HasBones() && boneName == node->mName.data)
        {
            aiMatrix4x4 nodeDerivedInverse = GetMeshBakingTransform(node, model.rootNode_);
            nodeDerivedInverse.Inverse();
            return ToMatrix3x4(nodeDerivedInverse);
        }
    }

    return Matrix3x4::IDENTITY;
}

void GetBlendData(OutModel& model, aiMesh* mesh, aiNode* meshNode, std::vector<unsigned>& boneMappings, std::vector<std::vector<unsigned char> >&
    blendIndices, std::vector<std::vector<float> >& blendWeights)
{
    blendIndices.resize(mesh->mNumVertices);
    blendWeights.resize(mesh->mNumVertices);
    boneMappings.clear();

    // If model has more bones than can fit vertex shader parameters, write the per-geometry mappings
    if (model.bones_.size() > maxBones_)
    {
        if (mesh->mNumBones > maxBones_)
        {
            ErrorExit(
                "Geometry (submesh) has over " + QString(maxBones_) + " bone influences. Try splitting to more submeshes\n"
                "that each stay at " + QString(maxBones_) + " bones or below."
            );
        }
        if (mesh->mNumBones > 0)
        {
            boneMappings.resize(mesh->mNumBones);
            for (unsigned i = 0; i < mesh->mNumBones; ++i)
            {
                aiBone* bone = mesh->mBones[i];
                QString boneName = FromAIString(bone->mName);
                unsigned globalIndex = GetBoneIndex(model, boneName);
                if (globalIndex == M_MAX_UNSIGNED)
                    ErrorExit("Bone " + boneName + " not found");
                boneMappings[i] = globalIndex;
                for (unsigned j = 0; j < bone->mNumWeights; ++j)
                {
                    unsigned vertex = bone->mWeights[j].mVertexId;
                    blendIndices[vertex].push_back(i);
                    blendWeights[vertex].push_back(bone->mWeights[j].mWeight);
                }
            }
        }
        else
        {
            // If mesh does not have skinning information, implement rigid skinning so that it stays compatible with AnimatedModel
            QString boneName = FromAIString(meshNode->mName);
            unsigned globalIndex = GetBoneIndex(model, boneName);
            if (globalIndex == M_MAX_UNSIGNED)
                PrintLine("Warning: bone " + boneName + " not found, skipping rigid skinning");
            else
            {
                boneMappings.push_back(globalIndex);
                for (unsigned i = 0; i < mesh->mNumVertices; ++i)
                {
                    blendIndices[i].push_back(0);
                    blendWeights[i].push_back(1.0f);
                }
            }
        }
    }
    else
    {
        if (mesh->mNumBones > 0)
        {
            for (unsigned i = 0; i < mesh->mNumBones; ++i)
            {
                aiBone* bone = mesh->mBones[i];
                QString boneName = FromAIString(bone->mName);
                unsigned globalIndex = GetBoneIndex(model, boneName);
                if (globalIndex == M_MAX_UNSIGNED)
                    ErrorExit("Bone " + boneName + " not found");
                for (unsigned j = 0; j < bone->mNumWeights; ++j)
                {
                    unsigned vertex = bone->mWeights[j].mVertexId;
                    blendIndices[vertex].push_back(globalIndex);
                    blendWeights[vertex].push_back(bone->mWeights[j].mWeight);
                }
            }
        }
        else
        {
            QString boneName = FromAIString(meshNode->mName);
            unsigned globalIndex = GetBoneIndex(model, boneName);
            if (globalIndex == M_MAX_UNSIGNED)
                PrintLine("Warning: bone " + boneName + " not found, skipping rigid skinning");
            else
            {
                for (unsigned i = 0; i < mesh->mNumVertices; ++i)
                {
                    blendIndices[i].push_back(globalIndex);
                    blendWeights[i].push_back(1.0f);
                }
            }
        }
    }

    // Normalize weights now if necessary, also remove too many influences
    for (unsigned i = 0; i < blendWeights.size(); ++i)
    {
        if (blendWeights[i].size() > 4)
        {
            PrintLine("Warning: more than 4 bone influences in vertex " + QString(i));

            while (blendWeights[i].size() > 4)
            {
                unsigned lowestIndex = 0;
                float lowest = M_INFINITY;
                for (unsigned j = 0; j < blendWeights[i].size(); ++j)
                {
                    if (blendWeights[i][j] < lowest)
                    {
                        lowest = blendWeights[i][j];
                        lowestIndex = j;
                    }
                }
                blendWeights[i].erase(blendWeights[i].begin()+lowestIndex);
                blendIndices[i].erase(blendIndices[i].begin()+lowestIndex);
            }
        }

        float sum = 0.0f;
        for (unsigned j = 0; j < blendWeights[i].size(); ++j)
            sum += blendWeights[i][j];
        if (sum != 1.0f && sum != 0.0f)
        {
            for (unsigned j = 0; j < blendWeights[i].size(); ++j)
                blendWeights[i][j] /= sum;
        }
    }
}

QString GetMeshMaterialName(aiMesh* mesh)
{
    aiMaterial* material = scene_->mMaterials[mesh->mMaterialIndex];
    aiString matNameStr;
    material->Get(AI_MATKEY_NAME, matNameStr);
    QString matName = SanitateAssetName(FromAIString(matNameStr));
    if (matName.trimmed().isEmpty())
        matName = GenerateMaterialName(material);

    return (useSubdirs_ ? "Materials/" : "") + matName + ".xml";
}

QString GenerateMaterialName(aiMaterial* material)
{
    for (unsigned i = 0; i < scene_->mNumMaterials; ++i)
    {
        if (scene_->mMaterials[i] == material)
            return inputName_ + "_Material" + QString(i);
    }

    // Should not go here
    return QString();
}

QString GetMaterialTextureName(const QString& nameIn)
{
    // Detect assimp embedded texture
    if (nameIn.length() && nameIn[0] == '*')
        return GenerateTextureName(nameIn.midRef(1).toInt());
    else
        return (useSubdirs_ ? "Textures/" : "") + nameIn;
}

QString GenerateTextureName(unsigned texIndex)
{
    if (texIndex < scene_->mNumTextures)
    {
        // If embedded texture contains encoded data, use the format hint for file extension. Else save RGBA8 data as PNG
        aiTexture* tex = scene_->mTextures[texIndex];
        if (!tex->mHeight)
            return (useSubdirs_ ? "Textures/" : "") + inputName_ + "_Texture" + QString(texIndex) + "." + tex->achFormatHint;
        else
            return (useSubdirs_ ? "Textures/" : "") + inputName_ + "_Texture" + QString(texIndex) + ".png";
    }

    // Should not go here
    return QString();
}

unsigned GetNumValidFaces(aiMesh* mesh)
{
    unsigned ret = 0;

    for (unsigned j = 0; j < mesh->mNumFaces; ++j)
    {
        if (mesh->mFaces[j].mNumIndices == 3)
            ++ret;
    }

    return ret;
}

void WriteShortIndices(unsigned short*& dest, aiMesh* mesh, unsigned index, unsigned offset)
{
    if (mesh->mFaces[index].mNumIndices == 3)
    {
        *dest++ = mesh->mFaces[index].mIndices[0] + offset;
        *dest++ = mesh->mFaces[index].mIndices[1] + offset;
        *dest++ = mesh->mFaces[index].mIndices[2] + offset;
    }
}

void WriteLargeIndices(unsigned*& dest, aiMesh* mesh, unsigned index, unsigned offset)
{
    if (mesh->mFaces[index].mNumIndices == 3)
    {
        *dest++ = mesh->mFaces[index].mIndices[0] + offset;
        *dest++ = mesh->mFaces[index].mIndices[1] + offset;
        *dest++ = mesh->mFaces[index].mIndices[2] + offset;
    }
}

void WriteVertex(float*& dest, aiMesh* mesh, unsigned index, bool isSkinned, BoundingBox& box,
    const Matrix3x4& vertexTransform, const Matrix3& normalTransform, std::vector<std::vector<unsigned char> >& blendIndices,
    std::vector<std::vector<float> >& blendWeights)
{
    Vector3 vertex = vertexTransform * ToVector3(mesh->mVertices[index]);
    box.Merge(vertex);
    *dest++ = vertex.x_;
    *dest++ = vertex.y_;
    *dest++ = vertex.z_;

    if (mesh->HasNormals())
    {
        Vector3 normal = normalTransform * ToVector3(mesh->mNormals[index]);
        *dest++ = normal.x_;
        *dest++ = normal.y_;
        *dest++ = normal.z_;
    }

    for (unsigned i = 0; i < mesh->GetNumColorChannels() && i < MAX_CHANNELS; ++i)
    {
        *((unsigned*)dest) = Color(mesh->mColors[i][index].r, mesh->mColors[i][index].g, mesh->mColors[i][index].b,
            mesh->mColors[i][index].a).ToUInt();
        ++dest;
    }

    for (unsigned i = 0; i < mesh->GetNumUVChannels() && i < MAX_CHANNELS; ++i)
    {
        Vector3 texCoord = ToVector3(mesh->mTextureCoords[i][index]);
        *dest++ = texCoord.x_;
        *dest++ = texCoord.y_;
    }

    if (mesh->HasTangentsAndBitangents())
    {
        Vector3 tangent = normalTransform * ToVector3(mesh->mTangents[index]);
        Vector3 normal = normalTransform * ToVector3(mesh->mNormals[index]);
        Vector3 bitangent = normalTransform * ToVector3(mesh->mBitangents[index]);
        // Check handedness
        float w = 1.0f;
        if ((tangent.CrossProduct(normal)).DotProduct(bitangent) < 0.5f)
            w = -1.0f;

        *dest++ = tangent.x_;
        *dest++ = tangent.y_;
        *dest++ = tangent.z_;
        *dest++ = w;
    }

    if (isSkinned)
    {
        for (unsigned i = 0; i < 4; ++i)
        {
            if (i < blendWeights[index].size())
                *dest++ = blendWeights[index][i];
            else
                *dest++ = 0.0f;
        }

        auto* destBytes = (unsigned char*)dest;
        ++dest;
        for (unsigned i = 0; i < 4; ++i)
        {
            if (i < blendIndices[index].size())
                *destBytes++ = blendIndices[index][i];
            else
                *destBytes++ = 0;
        }
    }
}

std::vector<VertexElement> GetVertexElements(aiMesh* mesh, bool isSkinned)
{
    std::vector<VertexElement> ret;

    // Position must always be first and of type Vector3 for raycasts to work
    ret.push_back(VertexElement(TYPE_VECTOR3, SEM_POSITION));

    if (mesh->HasNormals())
        ret.push_back(VertexElement(TYPE_VECTOR3, SEM_NORMAL));

    for (unsigned i = 0; i < mesh->GetNumColorChannels() && i < MAX_CHANNELS; ++i)
        ret.push_back(VertexElement(TYPE_UBYTE4_NORM, SEM_COLOR, i));

    /// \todo Assimp mesh structure can specify 3D UV-coords. How to determine the difference? For now always treated as 2D.
    for (unsigned i = 0; i < mesh->GetNumUVChannels() && i < MAX_CHANNELS; ++i)
        ret.push_back(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD, i));

    if (mesh->HasTangentsAndBitangents())
        ret.push_back(VertexElement(TYPE_VECTOR4, SEM_TANGENT));

    if (isSkinned)
    {
        ret.push_back(VertexElement(TYPE_VECTOR4, SEM_BLENDWEIGHTS));
        ret.push_back(VertexElement(TYPE_UBYTE4, SEM_BLENDINDICES));
    }

    return ret;
}

aiNode* GetNode(const QString& name, aiNode* rootNode, bool caseSensitive)
{
    if (!rootNode)
        return nullptr;
    if (!name.compare(rootNode->mName.data, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive))
        return rootNode;
    for (unsigned i = 0; i < rootNode->mNumChildren; ++i)
    {
        aiNode* found = GetNode(name, rootNode->mChildren[i], caseSensitive);
        if (found)
            return found;
    }
    return nullptr;
}

aiMatrix4x4 GetDerivedTransform(aiNode* node, aiNode* rootNode, bool rootInclusive)
{
    return GetDerivedTransform(node->mTransformation, node, rootNode, rootInclusive);
}

aiMatrix4x4 GetDerivedTransform(aiMatrix4x4 transform, aiNode* node, aiNode* rootNode, bool rootInclusive)
{
    // If basenode is defined, go only up to it in the parent chain
    while (node && node != rootNode)
    {
        node = node->mParent;
        if (!rootInclusive && node == rootNode)
            break;
        if (node)
            transform = node->mTransformation * transform;
    }
    return transform;
}

aiMatrix4x4 GetMeshBakingTransform(aiNode* meshNode, aiNode* modelRootNode)
{
    if (meshNode == modelRootNode)
        return {};
    else
        return GetDerivedTransform(meshNode, modelRootNode);
}

void GetPosRotScale(const aiMatrix4x4& transform, Vector3& pos, Quaternion& rot, Vector3& scale)
{
    aiVector3D aiPos;
    aiQuaternion aiRot;
    aiVector3D aiScale;
    transform.Decompose(aiScale, aiRot, aiPos);
    pos = ToVector3(aiPos);
    rot = ToQuaternion(aiRot);
    scale = ToVector3(aiScale);
}


QString FromAIString(const aiString& str)
{
    return QString(str.data);
}

Vector3 ToVector3(const aiVector3D& vec)
{
    return Vector3(vec.x, vec.y, vec.z);
}

Vector2 ToVector2(const aiVector2D& vec)
{
    return Vector2(vec.x, vec.y);
}

Quaternion ToQuaternion(const aiQuaternion& quat)
{
    return Quaternion(quat.w, quat.x, quat.y, quat.z);
}

Matrix3x4 ToMatrix3x4(const aiMatrix4x4& mat)
{
    Matrix3x4 ret;
    memcpy(&ret.m00_, &mat.a1, sizeof(Matrix3x4));
    return ret;
}

aiMatrix4x4 ToAIMatrix4x4(const Matrix3x4& mat)
{
    aiMatrix4x4 ret;
    memcpy(&ret.a1, &mat.m00_, sizeof(Matrix3x4));
    return ret;
}

QString SanitateAssetName(const QString& name)
{
    QString fixedName = name;
    fixedName.replace("<", "");
    fixedName.replace(">", "");
    fixedName.replace("?", "");
    fixedName.replace("*", "");
    fixedName.replace(":", "");
    fixedName.replace("\"", "");
    fixedName.replace("/", "");
    fixedName.replace("\\", "");
    fixedName.replace("|", "");

    return fixedName;
}

unsigned GetPivotlessBoneIndex(OutModel& model, const QString& boneName)
{
    for (unsigned i = 0; i < model.pivotlessBones_.size(); ++i)
    {
        if (boneName == model.pivotlessBones_[i]->mName.data)
            return i;
    }
    return M_MAX_UNSIGNED;
}

void FillChainTransforms(OutModel &model, aiMatrix4x4 *chain, const QString& mainBoneName)
{
    for (unsigned j = 0; j < TransformationComp_MAXIMUM; ++j)
    {
        QString transfBoneName = mainBoneName + "_$AssimpFbx$_" + QString(transformSuffix[j]);

        for (unsigned k = 0; k < model.bones_.size(); ++k)
        {
            QString boneName = QString(model.bones_[k]->mName.data);

            if (boneName == transfBoneName)
            {
                chain[j] = model.bones_[k]->mTransformation;
                break;
            }
        }
    }
}

void ExpandAnimatedChannelKeys(aiAnimation* anim, unsigned mainChannel, const int *channelIndices)
{
    aiNodeAnim* channel = anim->mChannels[mainChannel];
    unsigned int poskeyFrames = channel->mNumPositionKeys;
    unsigned int rotkeyFrames = channel->mNumRotationKeys;
    unsigned int scalekeyFrames = channel->mNumScalingKeys;

    // Get max key frames
    for (unsigned i = 0; i < TransformationComp_MAXIMUM; ++i)
    {
        if (channelIndices[i] != -1 && channelIndices[i] != mainChannel)
        {
            aiNodeAnim* channel2 = anim->mChannels[channelIndices[i]];

            if (channel2->mNumPositionKeys > poskeyFrames)
                poskeyFrames = channel2->mNumPositionKeys;
            if (channel2->mNumRotationKeys > rotkeyFrames)
                rotkeyFrames = channel2->mNumRotationKeys;
            if (channel2->mNumScalingKeys  > scalekeyFrames)
                scalekeyFrames = channel2->mNumScalingKeys;
        }
    }

    // resize and init vector key array
    if (poskeyFrames > channel->mNumPositionKeys)
    {
        auto* newKeys  = new aiVectorKey[poskeyFrames];
        for (unsigned i = 0; i < poskeyFrames; ++i)
        {
            if (i < channel->mNumPositionKeys )
                newKeys[i] = aiVectorKey(channel->mPositionKeys[i].mTime, channel->mPositionKeys[i].mValue);
            else
                newKeys[i].mValue = aiVector3D(0.0f, 0.0f, 0.0f);
        }
        delete[] channel->mPositionKeys;
        channel->mPositionKeys = newKeys;
        channel->mNumPositionKeys = poskeyFrames;
    }
    if (rotkeyFrames > channel->mNumRotationKeys)
    {
        auto* newKeys  = new aiQuatKey[rotkeyFrames];
        for (unsigned i = 0; i < rotkeyFrames; ++i)
        {
            if (i < channel->mNumRotationKeys)
                newKeys[i] = aiQuatKey(channel->mRotationKeys[i].mTime, channel->mRotationKeys[i].mValue);
            else
                newKeys[i].mValue = aiQuaternion();
        }
        delete[] channel->mRotationKeys;
        channel->mRotationKeys = newKeys;
        channel->mNumRotationKeys = rotkeyFrames;
    }
    if (scalekeyFrames > channel->mNumScalingKeys)
    {
        auto* newKeys  = new aiVectorKey[scalekeyFrames];
        for (unsigned i = 0; i < scalekeyFrames; ++i)
        {
            if ( i < channel->mNumScalingKeys)
                newKeys[i] = aiVectorKey(channel->mScalingKeys[i].mTime, channel->mScalingKeys[i].mValue);
            else
                newKeys[i].mValue = aiVector3D(1.0f, 1.0f, 1.0f);
        }
        delete[] channel->mScalingKeys;
        channel->mScalingKeys = newKeys;
        channel->mNumScalingKeys = scalekeyFrames;
    }
}

void InitAnimatedChainTransformIndices(aiAnimation* anim, unsigned mainChannel, const QString& mainBoneName, int *channelIndices)
{
    int numTransforms = 0;

    for (unsigned j = 0; j < TransformationComp_MAXIMUM; ++j)
    {
        QString transfBoneName = mainBoneName + "_$AssimpFbx$_" + QString(transformSuffix[j]);
        channelIndices[j] = -1;

        for (unsigned k = 0; k < anim->mNumChannels; ++k)
        {
            aiNodeAnim* channel = anim->mChannels[k];
            QString channelName = FromAIString(channel->mNodeName);

            if (channelName == transfBoneName)
            {
                ++numTransforms;
                channelIndices[j] = k;
                break;
            }
        }
    }

    // resize animated channel key size
    if (numTransforms > 1)
        ExpandAnimatedChannelKeys(anim, mainChannel, channelIndices);
}

void CreatePivotlessFbxBoneStruct(OutModel &model)
{
    // Init
    model.pivotlessBones_.clear();
    aiMatrix4x4 chains[TransformationComp_MAXIMUM];

    for (unsigned i = 0; i < model.bones_.size(); ++i)
    {
        QString mainBoneName = QString(model.bones_[i]->mName.data);

        // Skip $fbx nodes
        if (mainBoneName.contains("$AssimpFbx$"))
            continue;

        std::fill_n(chains, static_cast<unsigned int>(TransformationComp_MAXIMUM), aiMatrix4x4());
        FillChainTransforms(model, &chains[0], mainBoneName);

        // Calculate chained transform
        aiMatrix4x4 finalTransform;
        for (const auto& chain : chains)
            finalTransform = finalTransform * chain;

        // New bone node
        auto*pnode = new aiNode;
        pnode->mName = model.bones_[i]->mName;
        pnode->mTransformation = finalTransform * model.bones_[i]->mTransformation;

        model.pivotlessBones_.push_back(pnode);
    }
}

void ExtrapolatePivotlessAnimation(OutModel* model)
{
    if (suppressFbxPivotNodes_ && model)
    {
        PrintLine("Suppressing $fbx nodes");

        // Construct new bone structure from suppressed $fbx pivot nodes
        CreatePivotlessFbxBoneStruct(*model);

        // Extrapolate anim
        const std::vector<aiAnimation *> &animations = model->animations_;
        for (unsigned i = 0; i < animations.size(); ++i)
        {
            aiAnimation* anim = animations[i];
            QStringList mainBoneCompleteList;
            mainBoneCompleteList.clear();

            for (unsigned j = 0; j < anim->mNumChannels; ++j)
            {
                aiNodeAnim* channel = anim->mChannels[j];
                QString channelName = FromAIString(channel->mNodeName);
                int pos = channelName.indexOf("_$AssimpFbx$");

                if (pos != -1)
                {
                    // Every first $fbx animation channel for a bone will consolidate other $fbx animation to a single channel
                    // skip subsequent $fbx animation channel for the same bone
                    QString mainBoneName = channelName.mid(0, pos);

                    if (mainBoneCompleteList.contains(mainBoneName))
                        continue;

                    mainBoneCompleteList.push_back(mainBoneName);
                    unsigned boneIdx = GetBoneIndex(*model, mainBoneName);

                    // This condition exists if a geometry, not a bone, has a key animation
                    if (boneIdx == M_MAX_UNSIGNED)
                        continue;

                    // Init chain indices and fill transforms
                    aiMatrix4x4 mainboneTransform = model->bones_[boneIdx]->mTransformation;
                    aiMatrix4x4 chain[TransformationComp_MAXIMUM];
                    int channelIndices[TransformationComp_MAXIMUM];

                    InitAnimatedChainTransformIndices(anim, j, mainBoneName, &channelIndices[0]);
                    std::fill_n(chain, static_cast<unsigned int>(TransformationComp_MAXIMUM), aiMatrix4x4());
                    FillChainTransforms(*model, &chain[0], mainBoneName);

                    unsigned keyFrames = channel->mNumPositionKeys;
                    if (channel->mNumRotationKeys > keyFrames)
                        keyFrames = channel->mNumRotationKeys;
                    if (channel->mNumScalingKeys  > keyFrames)
                        keyFrames = channel->mNumScalingKeys;

                    for (unsigned k = 0; k < keyFrames; ++k)
                    {
                        double frameTime = 0.0;
                        aiMatrix4x4 finalTransform;

                        // Chain transform animated values
                        for (unsigned l = 0; l < TransformationComp_MAXIMUM; ++l)
                        {
                            // It's either the chain transform or animation channel transform
                            if (channelIndices[l] != -1)
                            {
                                aiMatrix4x4 animtform, tempMat;
                                aiNodeAnim* animchannel = anim->mChannels[channelIndices[l]];

                                if (k < animchannel->mNumPositionKeys)
                                {
                                    aiMatrix4x4::Translation(animchannel->mPositionKeys[k].mValue, tempMat);
                                    animtform = animtform * tempMat;
                                    frameTime = Max(animchannel->mPositionKeys[k].mTime, frameTime);
                                }
                                if (k < animchannel->mNumRotationKeys)
                                {
                                    tempMat = aiMatrix4x4(animchannel->mRotationKeys[k].mValue.GetMatrix());
                                    animtform = animtform * tempMat;
                                    frameTime = Max(animchannel->mRotationKeys[k].mTime, frameTime);
                                }
                                if (k < animchannel->mNumScalingKeys)
                                {
                                    aiMatrix4x4::Scaling(animchannel->mScalingKeys[k].mValue, tempMat);
                                    animtform = animtform * tempMat;
                                    frameTime = Max(animchannel->mScalingKeys[k].mTime, frameTime);
                                }

                                finalTransform = finalTransform * animtform;
                            }
                            else
                                finalTransform = finalTransform * chain[l];
                        }

                        aiVector3D animPos, animScale;
                        aiQuaternion animRot;
                        finalTransform = finalTransform * mainboneTransform;
                        finalTransform.Decompose(animScale, animRot, animPos);

                        // New values
                        if (k < channel->mNumPositionKeys)
                        {
                            channel->mPositionKeys[k].mValue = animPos;
                            channel->mPositionKeys[k].mTime = frameTime;
                        }

                        if (k < channel->mNumRotationKeys)
                        {
                            channel->mRotationKeys[k].mValue = animRot;
                            channel->mRotationKeys[k].mTime = frameTime;
                        }

                        if (k < channel->mNumScalingKeys)
                        {
                            channel->mScalingKeys[k].mValue = animScale;
                            channel->mScalingKeys[k].mTime = frameTime;
                        }
                    }
                }
            }
        }
    }
}

void CollectSceneNodesAsBones(OutModel &model, aiNode* rootNode)
{
    if (!rootNode)
        return;

    model.bones_.push_back(rootNode);

    for (unsigned i = 0; i < rootNode->mNumChildren; ++i)
    {
        CollectSceneNodesAsBones(model, rootNode->mChildren[i]);
    }
}
