#include "rxnpch.h"
#include "AudioSystem.h"
#include "FMODBackend.h"
#include "MiniaudioBackend.h"

namespace RXNEngine {

    void AudioSystem::Init()
    {
        ApplyProjectSettings(m_UsingFMOD);
    }

    void AudioSystem::ApplyProjectSettings(bool useFMOD)
    {
        if (m_Backend)
            m_Backend->Shutdown();

        m_UsingFMOD = useFMOD;
        if (m_UsingFMOD)
        {
            RXN_CORE_INFO("AudioSystem: Booting FMOD Studio Backend.");
            m_Backend = CreateScope<FMODBackend>();
        }
        else
        {
            RXN_CORE_INFO("AudioSystem: Booting Miniaudio Backend.");
            m_Backend = CreateScope<MiniaudioBackend>();
        }
        m_Backend->Init();
    }

    void AudioSystem::Shutdown()
    {
        if (m_Backend)
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