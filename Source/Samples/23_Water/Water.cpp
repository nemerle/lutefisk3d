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

#include "Water.h"

#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/Engine/Application.h>
#include <Lutefisk3D/IO/File.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/UI/Font.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Graphics/Light.h>
#include <Lutefisk3D/Graphics/Material.h>
#include <Lutefisk3D/Graphics/Model.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/Graphics/Renderer.h>
#include <Lutefisk3D/Graphics/RenderSurface.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/Graphics/Skybox.h>
#include <Lutefisk3D/Graphics/StaticModel.h>
#include <Lutefisk3D/Graphics/Terrain.h>
#include <Lutefisk3D/UI/Text.h>
#include <Lutefisk3D/Graphics/Texture2D.h>
#include <Lutefisk3D/UI/UI.h>
#include <Lutefisk3D/Graphics/Zone.h>

using namespace Urho3D;

URHO3D_DEFINE_APPLICATION_MAIN(Water)

Text* text=nullptr;
Water::Water(Context* context) :
    Sample("Water",context)
{
}

void Water::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Create the UI content
    CreateInstructions();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update event
    SubscribeToEvents();
}

void Water::CreateScene()
{
    ResourceCache* cache = GetContext()->m_ResourceCache.get();

    scene_ = new Scene(GetContext());

    // Create octree, use default volume (-1000, -1000, -1000) to (1000, 1000, 1000)
    scene_->CreateComponent<Octree>();

    // Create a Zone component for ambient lighting & fog control
    Node* zoneNode = scene_->CreateChild("Zone");
    Zone* zone = zoneNode->CreateComponent<Zone>();
    zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
    zone->SetAmbientColor(Color(0.15f, 0.15f, 0.15f));
    zone->SetFogColor(Color(1.0f, 1.0f, 1.0f));
    zone->SetFogStart(500.0f);
    zone->SetFogEnd(750.0f);

    // Create a directional light to the world. Enable cascaded shadows on it
    Node* lightNode = scene_->CreateChild("DirectionalLight");
    lightNode->SetDirection(Vector3(0.6f, -1.0f, 0.8f));
    Light* light = lightNode->CreateComponent<Light>();
    light->SetLightType(LIGHT_DIRECTIONAL);
    light->SetCastShadows(true);
    light->SetShadowBias(BiasParameters(0.00025f, 0.5f));
    light->SetShadowCascade(CascadeParameters(10.0f, 50.0f, 200.0f, 0.0f, 0.8f));
    light->SetSpecularIntensity(0.5f);
    // Apply slightly overbright lighting to match the skybox
    light->SetColor(Color(1.2f, 1.2f, 1.2f));

    // Create skybox. The Skybox component is used like StaticModel, but it will be always located at the camera, giving the
    // illusion of the box planes being far away. Use just the ordinary Box model and a suitable material, whose shader will
    // generate the necessary 3D texture coordinates for cube mapping
    Node* skyNode = scene_->CreateChild("Sky");
    skyNode->SetScale(500.0f); // The scale actually does not matter
    Skybox* skybox = skyNode->CreateComponent<Skybox>();
    skybox->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
    skybox->SetMaterial(cache->GetResource<Material>("Materials/Skybox.xml"));

    // Create heightmap terrain
    Node* terrainNode = scene_->CreateChild("Terrain");
    terrainNode->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    Terrain* terrain = terrainNode->CreateComponent<Terrain>();
    terrain->SetPatchSize(64);
    terrain->SetSpacing(Vector3(2.0f, 0.5f, 2.0f)); // Spacing between vertices and vertical resolution of the height map
    terrain->SetSmoothing(true);
    terrain->SetHeightMap(cache->GetResource<Image>("Textures/HeightMap.png"));
    terrain->SetMaterial(cache->GetResource<Material>("Materials/Terrain.xml"));
    // The terrain consists of large triangles, which fits well for occlusion rendering, as a hill can occlude all
    // terrain patches and other objects behind it
    terrain->SetOccluder(true);

    // Create 1000 boxes in the terrain. Always face outward along the terrain normal
    unsigned NUM_OBJECTS = 1000;
    for (unsigned i = 0; i < NUM_OBJECTS; ++i)
    {
        Node* objectNode = scene_->CreateChild("Box");
        Vector3 position(Random(2000.0f) - 1000.0f, 0.0f, Random(2000.0f) - 1000.0f);
        position.y_ = terrain->GetHeight(position) + 2.25f;
        objectNode->SetPosition(position);
        // Create a rotation quaternion from up vector to terrain normal
        objectNode->SetRotation(Quaternion(Vector3(0.0f, 1.0f, 0.0f), terrain->GetNormal(position)));
        objectNode->SetScale(5.0f);
        StaticModel* object = objectNode->CreateComponent<StaticModel>();
        object->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
        object->SetMaterial(cache->GetResource<Material>("Materials/Stone.xml"));
        object->SetCastShadows(true);
    }
    Node* shipNode = scene_->CreateChild("Ship");
    shipNode->SetPosition(Vector3(0.0f, 4.6f, 0.0f));
    //shipNode->SetRotation(Quaternion(0.0f, Random(360.0f), 0.0f));
    shipNode->SetScale(0.5f + Random(2.0f));
    StaticModel* shipObject = shipNode->CreateComponent<StaticModel>();
    shipObject->SetModel(cache->GetResource<Model>("Models/ship04.mdl"));
    shipObject->SetMaterial(0,cache->GetResource<Material>("Materials/ship04_Material0.xml"));
    shipObject->SetMaterial(1,cache->GetResource<Material>("Materials/ship04_Material1.xml"));
    shipObject->SetMaterial(2,cache->GetResource<Material>("Materials/ship04_Material2.xml"));
    shipObject->SetCastShadows(true);

    // Create a water plane object that is as large as the terrain
    waterNode_ = scene_->CreateChild("Water");
    waterNode_->SetScale(Vector3(2048.0f, 1.0f, 2048.0f));
    waterNode_->SetPosition(Vector3(0.0f, 5.0f, 0.0f));
    StaticModel* water = waterNode_->CreateComponent<StaticModel>();
    water->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));
    water->SetMaterial(cache->GetResource<Material>("Materials/Water.xml"));
    // Set a different viewmask on the water plane to be able to hide it from the reflection camera
    water->SetViewMask(0x80000000);

    // Create the camera. Set far clip to match the fog. Note: now we actually create the camera node outside
    // the scene, because we want it to be unaffected by scene load / save
    cameraNode_ = new Node(GetContext());
    Camera* camera = cameraNode_->CreateComponent<Camera>();
    camera->setFarClipDistance(750.0f);

    // Set an initial position for the camera scene node above the ground
    cameraNode_->SetPosition(Vector3(0.0f, 7.0f, -20.0f));
}

void Water::CreateInstructions()
{
    ResourceCache* cache = GetContext()->m_ResourceCache.get();
    UI* ui = GetContext()->m_UISystem.get();

    //    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    text = ui->GetRoot()->CreateChild<Text>();
    text->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 12);
    text->SetTextAlignment(HA_CENTER);
    text->SetVerticalAlignment(VA_TOP);

    //    // Construct new Text object, set string to display and font to use
    //    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    //    instructionText->SetText("Use WASD keys and mouse/touch to move");
    //    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    //    instructionText->SetTextAlignment(HA_CENTER);

    //    // Position the text relative to the screen center
    //    instructionText->SetHorizontalAlignment(HA_CENTER);
    //    instructionText->SetVerticalAlignment(VA_CENTER);
    //    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void Water::SetupViewport()
{
    Graphics* graphics = GetContext()->m_Graphics.get();
    Renderer* renderer = GetContext()->m_Renderer.get();
    ResourceCache* cache = GetContext()->m_ResourceCache.get();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(GetContext(), scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);

    // Create a mathematical plane to represent the water in calculations
    waterPlane_ = Plane(waterNode_->GetWorldRotation() * Vector3(0.0f, 1.0f, 0.0f), waterNode_->GetWorldPosition());
    // Create a downward biased plane for reflection view clipping. Biasing is necessary to avoid too aggressive clipping
    waterClipPlane_ = Plane(waterNode_->GetWorldRotation() * Vector3(0.0f, 1.0f, 0.0f), waterNode_->GetWorldPosition() -
                            Vector3(0.0f, 0.1f, 0.0f));

    // Create camera for water reflection
    // It will have the same farclip and position as the main viewport camera, but uses a reflection plane to modify
    // its position when rendering
    reflectionCameraNode_ = cameraNode_->CreateChild();
    Camera* reflectionCamera = reflectionCameraNode_->CreateComponent<Camera>();
    reflectionCamera->setFarClipDistance(750.0);
    reflectionCamera->SetViewMask(0x7fffffff); // Hide objects with only bit 31 in the viewmask (the water plane)
    reflectionCamera->SetAutoAspectRatio(false);
    reflectionCamera->SetUseReflection(true);
    reflectionCamera->SetReflectionPlane(waterPlane_);
    reflectionCamera->SetUseClipping(true); // Enable clipping of geometry behind water plane
    reflectionCamera->SetClipPlane(waterClipPlane_);
    // The water reflection texture is rectangular. Set reflection camera aspect ratio to match
    reflectionCamera->SetAspectRatio((float)graphics->GetWidth() / (float)graphics->GetHeight());
    // View override flags could be used to optimize reflection rendering. For example disable shadows
    //reflectionCamera->SetViewOverrideFlags(VO_DISABLE_SHADOWS);

    // Create a texture and setup viewport for water reflection. Assign the reflection texture to the diffuse
    // texture unit of the water material
    int texSize = 1024;
    SharedPtr<Texture2D> renderTexture(new Texture2D(GetContext()));
    renderTexture->SetSize(texSize, texSize, Graphics::GetRGBFormat(), TEXTURE_RENDERTARGET);
    renderTexture->SetFilterMode(FILTER_BILINEAR);
    RenderSurface* surface = renderTexture->GetRenderSurface();
    SharedPtr<Viewport> rttViewport(new Viewport(GetContext(), scene_, reflectionCamera));
    surface->SetViewport(0, rttViewport);
    Material* waterMat = cache->GetResource<Material>("Materials/Water.xml");
    waterMat->SetTexture(TU_DIFFUSE, renderTexture);
}

void Water::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    g_coreSignals.update.Connect(this,&Water::HandleUpdate);
}

void Water::MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (GetContext()->m_UISystem.get()->GetFocusElement())
        return;

    Input* input = GetContext()->m_InputSystem.get();

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    IntVector2 mouseMove = input->GetMouseMove();
    yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
    pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
    pitch_ = Clamp(pitch_, -90.0f, 90.0f);

    // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);

    // In case resolution has changed, adjust the reflection camera aspect ratio
    Graphics* graphics = GetContext()->m_Graphics.get();
    Camera* reflectionCamera = reflectionCameraNode_->GetComponent<Camera>();
    reflectionCamera->SetAspectRatio((float)graphics->GetWidth() / (float)graphics->GetHeight());
}

void Water::HandleUpdate(float timeStep)
{
    if(text) {
        FrameInfo frameInfo = GetContext()->m_Renderer.get()->GetFrameInfo();
        text->SetText(QString("FPS: ") + QString::number(1.0 / frameInfo.timeStep_));
    }
    // Move the camera, scale movement with time step
    MoveCamera(timeStep);
}
