#include "rxnpch.h"
#include "AudioSystem.h"
#include "FMODBackend.h"
#include "MiniaudioBackend.h"

namespace RXNEngine {
    void AudioSystem::Init()
    {
        bool useFMOD = true;

        if (useFMOD)
            m_Backend = CreateScope<FMODBackend>();
        else
            m_Backend = CreateScope<MiniaudioBackend>();

        m_Backend->Init();
    }

    void AudioSystem::Shutdown()
    {
        m_Backend->Shutdown();
    }

    void AudioSystem::LoadFMODBank(const std::string& bankFilePath)
    {
        if (FMODBackend* fmod = dynamic_cast<FMODBackend*>(m_Backend.get()))
            fmod->LoadBank(bankFilePath);
    }

    void AudioSystem::UnloadFMODBank(const std::string& bankFilePath)
    {
        m_Backend->UnloadBank(bankFilePath);
    }
}