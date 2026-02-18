#include "rxnpch.h"
#include "PhysicsSystem.h"

namespace RXNEngine {

    using namespace physx;

    PxFoundation* PhysicsSystem::s_Foundation = nullptr;
    PxPhysics* PhysicsSystem::s_Physics = nullptr;
    PxDefaultCpuDispatcher* PhysicsSystem::s_Dispatcher = nullptr;
    PxScene* PhysicsSystem::s_Scene = nullptr;
    PxPvd* PhysicsSystem::s_Pvd = nullptr;
    PxControllerManager* PhysicsSystem::s_ControllerManager = nullptr;

    PxDefaultAllocator PhysicsSystem::s_Allocator;
    PxDefaultErrorCallback PhysicsSystem::s_ErrorCallback;

    void PhysicsSystem::Init()
    {
        s_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, s_Allocator, s_ErrorCallback);
        RXN_CORE_ASSERT(s_Foundation, "PxCreateFoundation Failed!");

        s_Pvd = PxCreatePvd(*s_Foundation);
        PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
        s_Pvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

        s_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *s_Foundation, PxTolerancesScale(), true, s_Pvd);
        RXN_CORE_ASSERT(s_Physics, "PxCreatePhysics Failed!");

        // TODO: std::thread::hardware_concurrency()
        s_Dispatcher = PxDefaultCpuDispatcherCreate(2);

        RXN_CORE_INFO("PhysX Initialized Successfully!");
    }

    void PhysicsSystem::Shutdown()
    {
        DestroyScene();

        if (s_Dispatcher) s_Dispatcher->release();
        if (s_Physics)    s_Physics->release();
        if (s_Pvd)
        {
            PxPvdTransport* transport = s_Pvd->getTransport();
            s_Pvd->release();
            transport->release();
        }
        if (s_Foundation) s_Foundation->release();
    }

    void PhysicsSystem::CreateScene()
    {
        if (s_Scene)
            DestroyScene();

        PxSceneDesc sceneDesc(s_Physics->getTolerancesScale());

        sceneDesc.cpuDispatcher = s_Dispatcher;

        sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);

        sceneDesc.filterShader = PxDefaultSimulationFilterShader;

        s_Scene = s_Physics->createScene(sceneDesc);

        s_ControllerManager = PxCreateControllerManager(*s_Scene);

        PxPvdSceneClient* pvdClient = s_Scene->getScenePvdClient();
        if (pvdClient)
        {
            pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
            pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
            pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
        }
    }

    void PhysicsSystem::DestroyScene()
    {
        if (s_ControllerManager) s_ControllerManager->release();
        if (s_Scene) s_Scene->release();

        s_ControllerManager = nullptr;
        s_Scene = nullptr;
    }

    void PhysicsSystem::Update(float dt)
    {
        if (!s_Scene) return;

        s_Scene->simulate(dt);
        s_Scene->fetchResults(true);
    }

}