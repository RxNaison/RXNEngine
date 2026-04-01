#pragma once

#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Asset/StaticMesh.h"
#include "RXNEngine/Core/Subsystem.h"
#include <PxPhysicsAPI.h>

namespace RXNEngine {

    class PhysicsSystem : public Subsystem
    {
    public:
        virtual void Init() override;
        virtual void Shutdown() override;

        inline physx::PxPhysics* GetPhysics() { return m_Physics; }
        inline physx::PxDefaultCpuDispatcher* GetDispatcher() { return m_Dispatcher; }

        physx::PxConvexMesh* CreateConvexMesh(Ref<StaticMesh> mesh);
        physx::PxTriangleMesh* CreateTriangleMesh(Ref<StaticMesh> mesh);

    private:
        physx::PxFoundation* m_Foundation = nullptr;
        physx::PxPhysics* m_Physics = nullptr;
        physx::PxDefaultCpuDispatcher* m_Dispatcher = nullptr;
        physx::PxPvd* m_Pvd = nullptr;

        physx::PxDefaultAllocator m_Allocator;
        physx::PxDefaultErrorCallback m_ErrorCallback;
    };
}