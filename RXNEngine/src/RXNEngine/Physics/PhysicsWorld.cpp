#include "rxnpch.h"
#include "PhysicsWorld.h"
#include "PhysicsSystem.h"

namespace RXNEngine {

    physx::PxFilterFlags ContactReportFilterShader(
        physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0,
        physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
        physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize)
    {
        pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;
        pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND | physx::PxPairFlag::eNOTIFY_TOUCH_LOST;
        pairFlags |= physx::PxPairFlag::eDETECT_CCD_CONTACT;
        return physx::PxFilterFlag::eDEFAULT;
    }

    void PhysicsWorld::Init()
    {
        auto physicsSys = Application::Get().GetSubsystem<PhysicsSystem>();
        physx::PxPhysics* physics = physicsSys->GetPhysics();

        physx::PxSceneDesc sceneDesc(physics->getTolerancesScale());
        sceneDesc.cpuDispatcher = physicsSys->GetDispatcher();
        sceneDesc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
        sceneDesc.filterShader = ContactReportFilterShader;
        sceneDesc.simulationEventCallback = &m_ContactListener;
        sceneDesc.flags |= physx::PxSceneFlag::eENABLE_CCD;
        sceneDesc.flags |= physx::PxSceneFlag::eREQUIRE_RW_LOCK;

        m_Scene = physics->createScene(sceneDesc);
        m_Scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0f);
        m_Scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);

        m_ControllerManager = PxCreateControllerManager(*m_Scene);
    }

    void PhysicsWorld::Update(float deltaTime)
    {
        OPTICK_EVENT();
        if (!m_Scene) return;

        m_Scene->simulate(deltaTime);
        m_Scene->fetchResults(true);
    }

    void PhysicsWorld::Shutdown()
    {
        if (m_ControllerManager)
            m_ControllerManager->release();

        if (m_Scene)
            m_Scene->release();

        m_ControllerManager = nullptr;
        m_Scene = nullptr;
    }
}