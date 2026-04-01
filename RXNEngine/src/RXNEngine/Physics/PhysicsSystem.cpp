#include "rxnpch.h"
#include "PhysicsSystem.h"

#include <cooking/PxCooking.h>

namespace RXNEngine {

    void PhysicsSystem::Init()
    {
        m_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_ErrorCallback);
        RXN_CORE_ASSERT(m_Foundation, "PxCreateFoundation Failed!");

        m_Pvd = physx::PxCreatePvd(*m_Foundation);
        physx::PxPvdTransport* transport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
        m_Pvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);

        m_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_Foundation, physx::PxTolerancesScale(), true, m_Pvd);
        RXN_CORE_ASSERT(m_Physics, "PxCreatePhysics Failed!");

        m_Dispatcher = physx::PxDefaultCpuDispatcherCreate(2);
        RXN_CORE_INFO("PhysX Core Initialized Successfully!");
    }

    void PhysicsSystem::Shutdown()
    {
        if (m_Dispatcher) m_Dispatcher->release();
        if (m_Physics) m_Physics->release();
        if (m_Pvd)
        {
            physx::PxPvdTransport* transport = m_Pvd->getTransport();
            m_Pvd->release();
            transport->release();
        }
        if (m_Foundation) m_Foundation->release();
    }

    physx::PxConvexMesh* PhysicsSystem::CreateConvexMesh(Ref<StaticMesh> mesh)
    {
        const auto& vertices = mesh->GetVertices();
        if (vertices.empty())
            return nullptr;

        std::vector<physx::PxVec3> physicsVertices;
        physicsVertices.resize(vertices.size());

        for (const auto& submesh : mesh->GetSubmeshes())
        {
            for (uint32_t i = 0; i < submesh.VertexCount; i++)
            {
                uint32_t vIndex = submesh.BaseVertex + i;
                if (vIndex >= vertices.size())
                    continue;

                glm::vec4 localPos = glm::vec4(vertices[vIndex].Position, 1.0f);
                glm::vec4 worldPos = submesh.LocalTransform * localPos;

                physicsVertices[vIndex] = physx::PxVec3(worldPos.x, worldPos.y, worldPos.z);
            }
        }

        physx::PxConvexMeshDesc convexDesc;
        convexDesc.points.count = (physx::PxU32)physicsVertices.size();
        convexDesc.points.stride = sizeof(physx::PxVec3);
        convexDesc.points.data = physicsVertices.data();
        convexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;

        physx::PxTolerancesScale scale;
        physx::PxCookingParams params(scale);

        physx::PxConvexMeshCookingResult::Enum result;
        physx::PxConvexMesh* convexMesh = PxCreateConvexMesh(params, convexDesc, m_Physics->getPhysicsInsertionCallback(), &result);

        if (!convexMesh)
            RXN_CORE_ERROR("Failed to cook Convex Mesh!");

        return convexMesh;
    }

    physx::PxTriangleMesh* PhysicsSystem::CreateTriangleMesh(Ref<StaticMesh> mesh)
    {
        const auto& vertices = mesh->GetVertices();
        const auto& indices = mesh->GetIndices();

        if (vertices.empty() || indices.empty())
            return nullptr;

        std::vector<physx::PxVec3> physicsVertices;
        physicsVertices.resize(vertices.size());

        for (const auto& submesh : mesh->GetSubmeshes())
        {
            for (uint32_t i = 0; i < submesh.VertexCount; i++)
            {
                uint32_t vIndex = submesh.BaseVertex + i;
                if (vIndex >= vertices.size())
                    continue;

                glm::vec4 localPos = glm::vec4(vertices[vIndex].Position, 1.0f);
                glm::vec4 worldPos = submesh.LocalTransform * localPos;

                physicsVertices[vIndex] = physx::PxVec3(worldPos.x, worldPos.y, worldPos.z);
            }
        }

        physx::PxTriangleMeshDesc meshDesc;
        meshDesc.points.count = (physx::PxU32)physicsVertices.size();
        meshDesc.points.stride = sizeof(physx::PxVec3);
        meshDesc.points.data = physicsVertices.data();

        meshDesc.triangles.count = (physx::PxU32)(indices.size() / 3);
        meshDesc.triangles.stride = 3 * sizeof(uint32_t);
        meshDesc.triangles.data = indices.data();

        physx::PxTolerancesScale scale;
        physx::PxCookingParams params(scale);

        params.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;
        params.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eDISABLE_ACTIVE_EDGES_PRECOMPUTE;

        physx::PxTriangleMeshCookingResult::Enum result;
        physx::PxTriangleMesh* triMesh = PxCreateTriangleMesh(params, meshDesc, m_Physics->getPhysicsInsertionCallback(), &result);

        if (!triMesh)
            RXN_CORE_ERROR("Failed to cook Triangle Mesh!");

        return triMesh;
    }

}