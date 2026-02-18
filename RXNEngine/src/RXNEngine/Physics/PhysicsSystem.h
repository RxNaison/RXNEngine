#pragma once

#include "RXNEngine/Core/Base.h"
#include <PxPhysicsAPI.h>

namespace RXNEngine {

    class PhysicsSystem
    {
    public:
        static void Init();
        static void Shutdown();

        static physx::PxPhysics* GetPhysics() { return s_Physics; }
        static physx::PxScene* GetScene() { return s_Scene; }
        static physx::PxControllerManager* GetControllerManager() { return s_ControllerManager; }

        static void CreateScene();
        static void DestroyScene();
        static void Update(float dt);

    private:
        static physx::PxFoundation* s_Foundation;
        static physx::PxPhysics* s_Physics;
        static physx::PxDefaultCpuDispatcher* s_Dispatcher;
        static physx::PxScene* s_Scene;
        static physx::PxPvd* s_Pvd;
        static physx::PxControllerManager* s_ControllerManager;

        static physx::PxDefaultAllocator s_Allocator;
        static physx::PxDefaultErrorCallback s_ErrorCallback;
    };

}