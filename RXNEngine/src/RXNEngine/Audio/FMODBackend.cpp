#include "rxnpch.h"
#include "FMODBackend.h"

namespace RXNEngine {

    inline FMOD_VECTOR GLMToFMOD(const glm::vec3& vec)
    {
        return { vec.x, vec.y, vec.z * -1.0f };
    }

    struct FMODAudioSource
    {
        FMOD::Sound* Sound = nullptr;
        FMOD::Channel* Channel = nullptr;
        bool IsStudioEvent = false;
        FMOD::Studio::EventInstance* EventInstance = nullptr;
    };

    void FMODBackend::Init()
    {
        FMOD::Studio::System::create(&m_StudioSystem);
        m_StudioSystem->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr);
        m_StudioSystem->getCoreSystem(&m_CoreSystem);

        RXN_CORE_INFO("FMOD Studio Backend Initialized Successfully.");
    }

    void FMODBackend::Shutdown()
    {
        m_StudioSystem->release();
    }

    void FMODBackend::LoadBank(const std::string& bankFilePath)
    {
        if (m_BankCache.find(bankFilePath)
            != m_BankCache.end()) return;

        FMOD::Studio::Bank* bank = nullptr;
        m_StudioSystem->loadBankFile(bankFilePath.c_str(), FMOD_STUDIO_LOAD_BANK_NORMAL, &bank);
        if (bank)
            m_BankCache[bankFilePath] = bank;
    }

    void FMODBackend::UpdateListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up)
    {
        FMOD_3D_ATTRIBUTES attributes = { {0} };
        attributes.position = GLMToFMOD(position);
        attributes.forward = GLMToFMOD(forward);
        attributes.up = GLMToFMOD(up);
        attributes.velocity = { 0, 0, 0 };

        m_StudioSystem->setListenerAttributes(0, &attributes);
        m_StudioSystem->update();
    }

    void FMODBackend::PlayOneShot(const std::string& path, float volume)
    {
        if (path.find("event:/") == 0)
        {
            FMOD::Studio::EventDescription* eventDesc = nullptr;
            m_StudioSystem->getEvent(path.c_str(), &eventDesc);
            if (eventDesc)
            {
                FMOD::Studio::EventInstance* instance = nullptr;
                eventDesc->createInstance(&instance);
                instance->setVolume(volume);
                instance->start();
                instance->release();
            }
        }
        else
        {
            FMOD::Sound* sound = nullptr;
            if (m_SoundCache.find(path) == m_SoundCache.end())
            {
                m_CoreSystem->createSound(path.c_str(), FMOD_DEFAULT, nullptr, &sound);
                m_SoundCache[path] = sound;
            }
            else
            {
                sound = m_SoundCache[path];
            }

            if (sound) 
            {
                FMOD::Channel* channel = nullptr;
                m_CoreSystem->playSound(sound, nullptr, false, &channel);

                if (channel) 
                    channel->setVolume(volume);
            }
        }
    }

    void* FMODBackend::CreateSoundSource(const std::string& filepath, bool looping, float minDistance, float maxDistance)
    {
        FMODAudioSource* source = new FMODAudioSource();

        if (filepath.find("event:/") == 0)
        {
            source->IsStudioEvent = true;
            FMOD::Studio::EventDescription* eventDesc = nullptr;
            m_StudioSystem->getEvent(filepath.c_str(), &eventDesc);

            if (eventDesc)
                eventDesc->createInstance(&source->EventInstance);
        }
        else
        {
            FMOD_MODE mode = FMOD_3D | (looping ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
            m_CoreSystem->createSound(filepath.c_str(), mode, nullptr, &source->Sound);

            if (source->Sound)
                source->Sound->set3DMinMaxDistance(minDistance, maxDistance);
        }

        return source;
    }

    void FMODBackend::UpdateSoundSource(void* sourceData, const glm::vec3& position, float volume, float pitch)
    {
        if (!sourceData)
            return;

        FMODAudioSource* source = static_cast<FMODAudioSource*>(sourceData);

        if (source->IsStudioEvent && source->EventInstance)
        {
            FMOD_3D_ATTRIBUTES attributes = { {0} };
            attributes.position = GLMToFMOD(position);
            attributes.forward = { 0,0,1 };
            attributes.up = { 0,1,0 };
            source->EventInstance->set3DAttributes(&attributes);
            source->EventInstance->setVolume(volume);
            source->EventInstance->setPitch(pitch);
        }
        else if (source->Channel)
        {
            FMOD_VECTOR pos = GLMToFMOD(position);
            source->Channel->set3DAttributes(&pos, nullptr);
            source->Channel->setVolume(volume);
            source->Channel->setPitch(pitch);
        }
    }

    void FMODBackend::PlaySoundSource(void* sourceData)
    {
        if (!sourceData)
            return;

        FMODAudioSource* source = static_cast<FMODAudioSource*>(sourceData);

        if (source->IsStudioEvent && source->EventInstance)
        {
            source->EventInstance->start();
        }
        else if (source->Sound)
        {
            m_CoreSystem->playSound(source->Sound, nullptr, false, &source->Channel);
        }
    }

    void FMODBackend::StopSoundSource(void* sourceData)
    {
        if (!sourceData)
            return;

        FMODAudioSource* source = static_cast<FMODAudioSource*>(sourceData);

        if (source->IsStudioEvent && source->EventInstance)
            source->EventInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
        else if (source->Channel)
            source->Channel->stop();
    }

    void FMODBackend::DestroySoundSource(void* sourceData)
    {
        if (!sourceData)
            return;

        FMODAudioSource* source = static_cast<FMODAudioSource*>(sourceData);

        if (source->IsStudioEvent && source->EventInstance)
            source->EventInstance->release();
        else if (source->Sound)
            source->Sound->release();

        delete source;
    }

    void FMODBackend::UnloadBank(const std::string& bankFilePath)
    {
        if (m_BankCache.find(bankFilePath) != m_BankCache.end())
        {
            m_BankCache[bankFilePath]->unload();
            m_BankCache.erase(bankFilePath);
            RXN_CORE_INFO("FMOD Bank Unloaded: {0}", bankFilePath);
        }
    }

    void FMODBackend::SetEventParameter(void* sourceData, const std::string& paramName, float value)
    {
        if (!sourceData)
            return;

        FMODAudioSource* source = static_cast<FMODAudioSource*>(sourceData);

        if (source->IsStudioEvent && source->EventInstance)
            source->EventInstance->setParameterByName(paramName.c_str(), value);
    }
}