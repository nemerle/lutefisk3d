#
# Copyright (c) 2008-2017 the Urho3D project.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

set(BOX2D_Collision_SRCS
    Collision/b2BroadPhase.cpp
    Collision/b2CollideCircle.cpp
    Collision/b2CollideEdge.cpp
    Collision/b2CollidePolygon.cpp
    Collision/b2Collision.cpp
    Collision/b2Distance.cpp
    Collision/b2DynamicTree.cpp
    Collision/b2TimeOfImpact.cpp
)
set(BOX2D_Collision_HDRS
    Collision/b2BroadPhase.h
    Collision/b2Collision.h
    Collision/b2Distance.h
    Collision/b2DynamicTree.h
    Collision/b2TimeOfImpact.h
)
set(BOX2D_Shapes_SRCS
    Collision/Shapes/b2CircleShape.cpp
    Collision/Shapes/b2EdgeShape.cpp
    Collision/Shapes/b2ChainShape.cpp
    Collision/Shapes/b2PolygonShape.cpp
)
set(BOX2D_Shapes_HDRS
    Collision/Shapes/b2CircleShape.h
    Collision/Shapes/b2EdgeShape.h
    Collision/Shapes/b2ChainShape.h
    Collision/Shapes/b2PolygonShape.h
    Collision/Shapes/b2Shape.h
)
set(BOX2D_Common_SRCS
    Common/b2BlockAllocator.cpp
    Common/b2Draw.cpp
    Common/b2Math.cpp
    Common/b2Settings.cpp
    Common/b2StackAllocator.cpp
    Common/b2Timer.cpp
)
set(BOX2D_Common_HDRS
    Common/b2BlockAllocator.h
    Common/b2Draw.h
    Common/b2GrowableStack.h
    Common/b2Math.h
    Common/b2Settings.h
    Common/b2StackAllocator.h
    Common/b2Timer.h
)
set(BOX2D_Dynamics_SRCS
    Dynamics/b2Body.cpp
    Dynamics/b2ContactManager.cpp
    Dynamics/b2Fixture.cpp
    Dynamics/b2Island.cpp
    Dynamics/b2World.cpp
    Dynamics/b2WorldCallbacks.cpp
)
set(BOX2D_Dynamics_HDRS
    Dynamics/b2Body.h
    Dynamics/b2ContactManager.h
    Dynamics/b2Fixture.h
    Dynamics/b2Island.h
    Dynamics/b2TimeStep.h
    Dynamics/b2World.h
    Dynamics/b2WorldCallbacks.h
)
set(BOX2D_Contacts_SRCS
    Dynamics/Contacts/b2CircleContact.cpp
    Dynamics/Contacts/b2Contact.cpp
    Dynamics/Contacts/b2ContactSolver.cpp
    Dynamics/Contacts/b2PolygonAndCircleContact.cpp
    Dynamics/Contacts/b2EdgeAndCircleContact.cpp
    Dynamics/Contacts/b2EdgeAndPolygonContact.cpp
    Dynamics/Contacts/b2ChainAndCircleContact.cpp
    Dynamics/Contacts/b2ChainAndPolygonContact.cpp
    Dynamics/Contacts/b2PolygonContact.cpp
)
set(BOX2D_Contacts_HDRS
    Dynamics/Contacts/b2CircleContact.h
    Dynamics/Contacts/b2Contact.h
    Dynamics/Contacts/b2ContactSolver.h
    Dynamics/Contacts/b2PolygonAndCircleContact.h
    Dynamics/Contacts/b2EdgeAndCircleContact.h
    Dynamics/Contacts/b2EdgeAndPolygonContact.h
    Dynamics/Contacts/b2ChainAndCircleContact.h
    Dynamics/Contacts/b2ChainAndPolygonContact.h
    Dynamics/Contacts/b2PolygonContact.h
)
set(BOX2D_Joints_SRCS
    Dynamics/Joints/b2DistanceJoint.cpp
    Dynamics/Joints/b2FrictionJoint.cpp
    Dynamics/Joints/b2GearJoint.cpp
    Dynamics/Joints/b2Joint.cpp
    Dynamics/Joints/b2MotorJoint.cpp
    Dynamics/Joints/b2MouseJoint.cpp
    Dynamics/Joints/b2PrismaticJoint.cpp
    Dynamics/Joints/b2PulleyJoint.cpp
    Dynamics/Joints/b2RevoluteJoint.cpp
    Dynamics/Joints/b2RopeJoint.cpp
    Dynamics/Joints/b2WeldJoint.cpp
    Dynamics/Joints/b2WheelJoint.cpp
)
set(BOX2D_Joints_HDRS
    Dynamics/Joints/b2DistanceJoint.h
    Dynamics/Joints/b2FrictionJoint.h
    Dynamics/Joints/b2GearJoint.h
    Dynamics/Joints/b2Joint.h
    Dynamics/Joints/b2MotorJoint.h
    Dynamics/Joints/b2MouseJoint.h
    Dynamics/Joints/b2PrismaticJoint.h
    Dynamics/Joints/b2PulleyJoint.h
    Dynamics/Joints/b2RevoluteJoint.h
    Dynamics/Joints/b2RopeJoint.h
    Dynamics/Joints/b2WeldJoint.h
    Dynamics/Joints/b2WheelJoint.h
)
set(BOX2D_Rope_SRCS
    Rope/b2Rope.cpp
)
set(BOX2D_Rope_HDRS
    Rope/b2Rope.h
)
set(BOX2D_General_HDRS
    Box2D.h
)

# Define source files
file (GLOB CPP_FILES
    ${BOX2D_Collision_SRCS}
    ${BOX2D_Shapes_SRCS}
    ${BOX2D_Common_SRCS}
    ${BOX2D_Dynamics_SRCS}
    ${BOX2D_Contacts_SRCS}
    ${BOX2D_Joints_SRCS}
    ${BOX2D_Rope_SRCS})

file (GLOB H_FILES
    ${BOX2D_Collision_HDRS}
    ${BOX2D_Shapes_HDRS}
    ${BOX2D_Common_HDRS}
    ${BOX2D_Dynamics_HDRS}
    ${BOX2D_Contacts_HDRS}
    ${BOX2D_Joints_HDRS}
    ${BOX2D_Rope_HDRS}
    ${BOX2D_General_HDRS})

set (SOURCE_FILES ${CPP_FILES} ${H_FILES})


# These are used to create visual studio folders.
source_group(Collision FILES ${BOX2D_Collision_SRCS} ${BOX2D_Collision_HDRS})
source_group(Collision\\Shapes FILES ${BOX2D_Shapes_SRCS} ${BOX2D_Shapes_HDRS})
source_group(Common FILES ${BOX2D_Common_SRCS} ${BOX2D_Common_HDRS})
source_group(Dynamics FILES ${BOX2D_Dynamics_SRCS} ${BOX2D_Dynamics_HDRS})
source_group(Dynamics\\Contacts FILES ${BOX2D_Contacts_SRCS} ${BOX2D_Contacts_HDRS})
source_group(Dynamics\\Joints FILES ${BOX2D_Joints_SRCS} ${BOX2D_Joints_HDRS})
source_group(Include FILES ${BOX2D_General_HDRS})
source_group(Rope FILES ${BOX2D_Rope_SRCS} ${BOX2D_Rope_HDRS})

add_library(Box2D STATIC ${SOURCE_FILES} ${INCLUDES})
#install (FILES ${H_FILES} DESTINATION include/ThirdParty/Box2D)
set_property(TARGET Box2D PROPERTY POSITION_INDEPENDENT_CODE ON)
