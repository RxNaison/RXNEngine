#include "rxnpch.h"
#include "FMODBackend.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Core/VFSSystem.h"

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
        std::vector<uint8_t> SoundData;
    };

    void FMODBackend::Init()
    {
        FMOD::Studio::System::create(&m_StudioSystem);
        m_StudioSystem->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL | FMOD_INIT_CHANNEL_LOWPASS | FMOD_INIT_CHANNEL_DISTANCEFILTER, nullptr);
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

    void FMODBackend::UpdateListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up, const glm::vec3& velocity)
    {
        FMOD_3D_ATTRIBUTES attributes = { {0} };
        attributes.position = GLMToFMOD(position);
        attributes.forward = GLMToFMOD(forward);
        attributes.up = GLMToFMOD(up);
        attributes.velocity = GLMToFMOD(velocity);

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
                auto vfs = Application::Get().GetSubsystem<VFSSystem>();
                if (vfs && vfs->FileExists(path))
                {
                    std::vector<uint8_t> soundData = vfs->ReadFile(path);
                    if (!soundData.empty())
                    {
                        m_MemorySoundCache[path] = std::move(soundData);
                        FMOD_CREATESOUNDEXINFO exinfo;
                        memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
                        exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
                        exinfo.length = m_MemorySoundCache[path].size();

                        m_CoreSystem->createSound((const char*)m_MemorySoundCache[path].data(), FMOD_DEFAULT | FMOD_OPENMEMORY, &exinfo, &sound);
                    }
                }
                else
                {
                    m_CoreSystem->createSound(path.c_str(), FMOD_DEFAULT, nullptr, &sound);
                }
                
                if (sound)
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
            FMOD_MODE mode = FMOD_3D | FMOD_3D_LINEARROLLOFF | (looping ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
            
            auto vfs = Application::Get().GetSubsystem<VFSSystem>();
            if (vfs && vfs->FileExists(filepath))
            {
                source->SoundData = vfs->ReadFile(filepath);
                if (!source->SoundData.empty())
                {
                    FMOD_CREATESOUNDEXINFO exinfo;
                    memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
                    exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
                    exinfo.length = source->SoundData.size();

                    m_CoreSystem->createSound((const char*)source->SoundData.data(), mode | FMOD_OPENMEMORY, &exinfo, &source->Sound);
                }
            }
            else
            {
                m_CoreSystem->createSound(filepath.c_str(), mode, nullptr, &source->Sound);
            }

            if (source->Sound)
                source->Sound->set3DMinMaxDistance(minDistance, maxDistance);
        }

        return source;
    }

    void FMODBackend::UpdateSoundSource(void* sourceData, const glm::vec3& position, const glm::vec3& velocity, float volume, float pitch, float minDistance, float maxDistance, float occlusion, float spread)
    {
        if (!sourceData)
            return;

        FMODAudioSource* source = static_cast<FMODAudioSource*>(sourceData);

        float reverbOcclusion = (spread > 0.0f) ? (occlusion * 0.20f) : (occlusion * 0.50f);

        if (source->IsStudioEvent && source->EventInstance)
        {
            FMOD_3D_ATTRIBUTES attributes = { {0} };
            attributes.position = GLMToFMOD(position);
            attributes.velocity = GLMToFMOD(velocity);
            attributes.forward = { 0,0,1 };
            attributes.up = { 0,1,0 };
            source->EventInstance->set3DAttributes(&attributes);
            source->EventInstance->setVolume(volume);
            source->EventInstance->setPitch(pitch);

            FMOD::ChannelGroup* cg = nullptr;
            if (source->EventInstance->getChannelGroup(&cg) == FMOD_OK && cg)
            {
                cg->set3DOcclusion(occlusion, reverbOcclusion);
                cg->set3DSpread(spread);
            }
            source->EventInstance->setParameterByName("Occlusion", occlusion);
        }
        else if (source->Channel)
        {
            FMOD_VECTOR pos = GLMToFMOD(position);
            FMOD_VECTOR vel = GLMToFMOD(velocity);
            source->Channel->set3DAttributes(&pos, &vel);
            source->Channel->setVolume(volume);
            source->Channel->setPitch(pitch);
            source->Channel->set3DMinMaxDistance(minDistance, maxDistance);
            source->Channel->set3DOcclusion(occlusion, reverbOcclusion);
            source->Channel->set3DSpread(spread);
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

    void FMODBackend::SetReverbPreset(int preset, float volume)
    {
        FMOD_REVERB_PROPERTIES prop;
        switch (preset)
        {
            case 0: prop = FMOD_PRESET_OFF; break;
            case 1: prop = FMOD_PRESET_GENERIC; break;
            case 2: prop = FMOD_PRESET_PADDEDCELL; break;
            case 3: prop = FMOD_PRESET_ROOM; break;
            case 4: prop = FMOD_PRESET_BATHROOM; break;
            case 5: prop = FMOD_PRESET_LIVINGROOM; break;
            case 6: prop = FMOD_PRESET_STONEROOM; break;
            case 7: prop = FMOD_PRESET_AUDITORIUM; break;
            case 8: prop = FMOD_PRESET_CONCERTHALL; break;
            case 9: prop = FMOD_PRESET_CAVE; break;
            case 10: prop = FMOD_PRESET_HANGAR; break;
            case 11: prop = FMOD_PRESET_CARPETTEDHALLWAY; break;
            case 12: prop = FMOD_PRESET_HALLWAY; break;
            case 13: prop = FMOD_PRESET_STONECORRIDOR; break;
            case 14: prop = FMOD_PRESET_ALLEY; break;
            case 15: prop = FMOD_PRESET_FOREST; break;
            case 16: prop = FMOD_PRESET_CITY; break;
            case 17: prop = FMOD_PRESET_MOUNTAINS; break;
            case 18: prop = FMOD_PRESET_QUARRY; break;
            default: prop = FMOD_PRESET_GENERIC; break;
        }

        m_CoreSystem->setReverbProperties(0, &prop);
    }
}