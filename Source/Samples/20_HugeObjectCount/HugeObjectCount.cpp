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
#include "HugeObjectCount.h"

#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/UI/Font.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Graphics/Material.h>
#include <Lutefisk3D/Graphics/Model.h>
#include <Lutefisk3D/Graphics/Light.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/Core/Profiler.h>
#include <Lutefisk3D/Graphics/Renderer.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/Graphics/StaticModelGroup.h>
#include <Lutefisk3D/UI/Text.h>
#include <Lutefisk3D/UI/UI.h>
#include <Lutefisk3D/Graphics/Zone.h>


URHO3D_DEFINE_APPLICATION_MAIN(HugeObjectCount)

HugeObjectCount::HugeObjectCount(Context* context) :
    Sample("HugeObjectCount",context),
    animate_(false),
    useGroups_(false)
{
}

void HugeObjectCount::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Create the UI content
    CreateInstructions();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update events
    SubscribeToEvents();
}

void HugeObjectCount::CreateScene()
{
    ResourceCache* cache = GetContext()->m_ResourceCache.get();

    if (!scene_)
        scene_ = new Scene(GetContext());
    else
    {
        scene_->Clear();
        boxNodes_.clear();
    }

    // Create the Octree component to the scene so that drawable objects can be rendered. Use default volume
    // (-1000, -1000, -1000) to (1000, 1000, 1000)
    scene_->CreateComponent<Octree>();

    // Create a Zone for ambient light & fog control
    Node* zoneNode = scene_->CreateChild("Zone");
    Zone* zone = zoneNode->CreateComponent<Zone>();
    zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
    zone->SetFogColor(Color(0.2f, 0.2f, 0.2f));
    zone->SetFogStart(200.0f);
    zone->SetFogEnd(300.0f);

    // Create a directional light
    Node* lightNode = scene_->CreateChild("DirectionalLight");
    lightNode->SetDirection(Vector3(-0.6f, -1.0f, -0.8f)); // The direction vector does not need to be normalized
    Light* light = lightNode->CreateComponent<Light>();
    light->SetLightType(LIGHT_DIRECTIONAL);

    if (!useGroups_)
    {
        light->SetColor(Color(0.7f, 0.35f, 0.0f));

        // Create individual box StaticModels in the scene
        for (int y = -125; y < 125; ++y)
        {
            for (int x = -125; x < 125; ++x)
            {
                Node* boxNode = scene_->CreateChild("Box");
                boxNode->SetPosition(Vector3(x * 0.3f, 0.0f, y * 0.3f));
                boxNode->SetScale(0.25f);
                StaticModel* boxObject = boxNode->CreateComponent<StaticModel>();
                boxObject->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
                boxNodes_.push_back(SharedPtr<Node>(boxNode));
            }
        }
    }
    else
    {
        light->SetColor(Color(0.6f, 0.6f, 0.6f));
        light->SetSpecularIntensity(1.5f);

        // Create StaticModelGroups in the scene
        StaticModelGroup* lastGroup = nullptr;

        for (int y = -125; y < 125; ++y)
        {
            for (int x = -125; x < 125; ++x)
            {
                // Create new group if no group yet, or the group has already "enough" objects. The tradeoff is between culling
                // accuracy and the amount of CPU processing needed for all the objects. Note that the group's own transform
                // does not matter, and it does not render anything if instance nodes are not added to it
                if (!lastGroup || lastGroup->GetNumInstanceNodes() >= 25 * 25)
                {
                    Node* boxGroupNode = scene_->CreateChild("BoxGroup");
                    lastGroup = boxGroupNode->CreateComponent<StaticModelGroup>();
                    lastGroup->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
                }

                Node* boxNode = scene_->CreateChild("Box");
                boxNode->SetPosition(Vector3(x * 0.3f, 0.0f, y * 0.3f));
                boxNode->SetScale(0.25f);
                boxNodes_.push_back(SharedPtr<Node>(boxNode));
                lastGroup->AddInstanceNode(boxNode);
            }
        }
    }

    // Create the camera. Create it outside the scene so that we can clear the whole scene without affecting it
    if (!cameraNode_)
    {
        cameraNode_ = new Node(GetContext());
        cameraNode_->SetPosition(Vector3(0.0f, 10.0f, -100.0f));
        Camera* camera = cameraNode_->CreateComponent<Camera>();
        camera->setFarClipDistance(300.0f);
    }
}

void HugeObjectCount::CreateInstructions()
{
    ResourceCache* cache = GetContext()->m_ResourceCache.get();
    UI* ui = GetContext()->m_UISystem.get();

    // Construct new Text object, set string to display and font to use
    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText(
        "Use WASD keys and mouse/touch to move\n"
        "Space to toggle animation\n"
        "G to toggle object group optimization"
    );
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    // The text has multiple rows. Center them in relation to each other
    instructionText->SetTextAlignment(HA_CENTER);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void HugeObjectCount::SetupViewport()
{
    Renderer* renderer = GetContext()->m_Renderer.get();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(GetContext(), scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void HugeObjectCount::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
//    LUTEFISK_SUBSCRIBE_GLOBAL(E_UPDATE,HandleUpdate);
    g_coreSignals.update.Connect(this,&HugeObjectCount::HandleUpdate);
}

void HugeObjectCount::MoveCamera(float timeStep)
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
}

void HugeObjectCount::AnimateObjects(float timeStep)
{
    URHO3D_PROFILE(AnimateObjects);

    const float ROTATE_SPEED = 15.0f;
    // Rotate about the Z axis (roll)
    Quaternion rotateQuat(ROTATE_SPEED * timeStep, Vector3::FORWARD);

    for (unsigned i = 0; i < boxNodes_.size(); ++i)
        boxNodes_[i]->Rotate(rotateQuat);
}

void HugeObjectCount::HandleUpdate(float timeStep)
{
    // Take the frame time step, which is stored as a float
    // Toggle animation with space
    Input* input = GetContext()->m_InputSystem.get();
    if (input->GetKeyPress(KEY_SPACE))
        animate_ = !animate_;

    // Toggle grouped / ungrouped mode
    if (input->GetKeyPress('G'))
    {
        useGroups_ = !useGroups_;
        CreateScene();
    }

    // Move the camera, scale movement with time step
    MoveCamera(timeStep);

    // Animate scene if enabled
    if (animate_)
        AnimateObjects(timeStep);
}
