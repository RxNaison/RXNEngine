#pragma once

#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Scripting/ScriptEngine.h"
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

    class PhysicsContactListener : public physx::PxSimulationEventCallback
    {
    public:
        virtual void onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count) override {}
        virtual void onWake(physx::PxActor** actors, physx::PxU32 count) override {}
        virtual void onSleep(physx::PxActor** actors, physx::PxU32 count) override {}
        virtual void onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count) override {}
        virtual void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count) override {}

        virtual void onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs) override
        {
            physx::PxRigidActor* actorA = (physx::PxRigidActor*)pairHeader.actors[0];
            physx::PxRigidActor* actorB = (physx::PxRigidActor*)pairHeader.actors[1];

            if (!actorA || !actorB) return;

            uint64_t entityA = (uint64_t)actorA->userData;
            uint64_t entityB = (uint64_t)actorB->userData;

            for (physx::PxU32 i = 0; i < nbPairs; i++)
            {
                const physx::PxContactPair& cp = pairs[i];

                if (cp.events & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)
                {
                    ScriptEngine::OnCollisionEnter(entityA, entityB);
                    ScriptEngine::OnCollisionEnter(entityB, entityA);
                }
                else if (cp.events & physx::PxPairFlag::eNOTIFY_TOUCH_LOST)
                {
                    ScriptEngine::OnCollisionExit(entityA, entityB);
                    ScriptEngine::OnCollisionExit(entityB, entityA);
                }
            }
        }
    };
}