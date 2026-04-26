#pragma once

#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Core/Assert.h"

#include <unordered_map>
#include <vector>
#include <typeindex>

namespace RXNEngine {

    class Subsystem
    {
    public:
        virtual ~Subsystem() = default;

        virtual void Init() {}
        virtual void Update(float deltaTime) {}
        virtual void Shutdown() {}
    };

    class SubsystemRegistry
    {
    public:
        virtual ~SubsystemRegistry()
        {
            ShutdownSubsystems();
        }

        template<typename T, typename... Args>
        void AddSubsystem(Args&&... args)
        {
            static_assert(std::is_base_of<Subsystem, T>::value, "System must inherit from RXNEngine::Subsystem!");
            Ref<T> subsystem = CreateRef<T>(std::forward<Args>(args)...);
            subsystem->Init();
            m_Subsystems[typeid(T)] = subsystem;
            m_SubsystemList.push_back(subsystem);
        }

        template<typename TInterface>
        void AddSubsystem(Ref<TInterface> instance)
        {
            static_assert(std::is_base_of<Subsystem, TInterface>::value, "System must inherit from RXNEngine::Subsystem!");
            instance->Init();
            m_Subsystems[typeid(TInterface)] = instance;
            m_SubsystemList.push_back(instance);
        }

        template<typename T>
        Ref<T> GetSubsystem()
        {
            auto it = m_Subsystems.find(typeid(T));
            //RXN_CORE_ASSERT(it != m_Subsystems.end(), "Requested Subsystem is not registered!");
            return std::static_pointer_cast<T>(it->second);
        }

    protected:
        void ShutdownSubsystems()
        {
            for (auto it = m_SubsystemList.rbegin(); it != m_SubsystemList.rend(); ++it)
                (*it)->Shutdown();

            m_Subsystems.clear();
            m_SubsystemList.clear();
        }

    protected:
        std::unordered_map<std::type_index, Ref<Subsystem>> m_Subsystems;
        std::vector<Ref<Subsystem>> m_SubsystemList;
    };

}