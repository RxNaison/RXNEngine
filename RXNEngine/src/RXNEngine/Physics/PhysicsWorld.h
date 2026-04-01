#pragma once
#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Core/Subsystem.h"
#include "RXNEngine/Scripting/ScriptEngine.h"
#include "RXNEngine/Core/Application.h"
#include <PxPhysicsAPI.h>

namespace RXNEngine {

    class PhysicsContactListener : public physx::PxSimulationEventCallback
    {
    public:
        virtual void onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count) override {}
        virtual void onWake(physx::PxActor** actors, physx::PxU32 count) override {}
        virtual void onSleep(physx::PxActor** actors, physx::PxU32 count) override {}
        virtual void onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count) override {}

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
                    Application::Get().GetSubsystem<ScriptEngine>()->OnCollisionEnter(entityA, entityB);
                    Application::Get().GetSubsystem<ScriptEngine>()->OnCollisionEnter(entityB, entityA);
                }
                else if (cp.events & physx::PxPairFlag::eNOTIFY_TOUCH_LOST)
                {
                    Application::Get().GetSubsystem<ScriptEngine>()->OnCollisionExit(entityA, entityB);
                    Application::Get().GetSubsystem<ScriptEngine>()->OnCollisionExit(entityB, entityA);
                }
            }
        }

        virtual void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count) override
        {
            for (physx::PxU32 i = 0; i < count; i++)
            {
                physx::PxTriggerPair& tp = pairs[i];

                if (tp.flags & (physx::PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | physx::PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
                    continue;

                physx::PxRigidActor* triggerActor = (physx::PxRigidActor*)tp.triggerActor;
                physx::PxRigidActor* otherActor = (physx::PxRigidActor*)tp.otherActor;

                if (!triggerActor || !otherActor) continue;

                uint64_t triggerEntity = (uint64_t)triggerActor->userData;
                uint64_t otherEntity = (uint64_t)otherActor->userData;

                if (tp.status & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)
                {
                    Application::Get().GetSubsystem<ScriptEngine>()->OnTriggerEnter(triggerEntity, otherEntity);
                    Application::Get().GetSubsystem<ScriptEngine>()->OnTriggerEnter(otherEntity, triggerEntity);

                }
                else if (tp.status & physx::PxPairFlag::eNOTIFY_TOUCH_LOST)
                {
                    Application::Get().GetSubsystem<ScriptEngine>()->OnTriggerExit(triggerEntity, otherEntity);
                    Application::Get().GetSubsystem<ScriptEngine>()->OnTriggerExit(otherEntity, triggerEntity);

                }
            }
        }
    };

    class PhysicsWorld : public Subsystem
    {
    public:
        virtual void Init() override;
        virtual void Update(float deltaTime) override;
        virtual void Shutdown() override;

        inline physx::PxScene* GetScene() { return m_Scene; }
        inline physx::PxControllerManager* GetControllerManager() { return m_ControllerManager; }

        void LockWrite() { if (m_Scene) m_Scene->lockWrite(); }
        void UnlockWrite() { if (m_Scene) m_Scene->unlockWrite(); }
        void LockRead() { if (m_Scene) m_Scene->lockRead(); }
        void UnlockRead() { if (m_Scene) m_Scene->unlockRead(); }

    private:
        physx::PxScene* m_Scene = nullptr;
        physx::PxControllerManager* m_ControllerManager = nullptr;
        PhysicsContactListener m_ContactListener;
    };
}