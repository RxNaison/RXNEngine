#pragma once
#include "AudioBackend.h"
#include <fmod_studio.hpp>
#include <fmod.hpp>
#include <unordered_map>

namespace RXNEngine {

    class FMODBackend : public AudioBackend
    {
    public:
        virtual void Init() override;
        virtual void Shutdown() override;

        virtual void UpdateListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) override;
        virtual void PlayOneShot(const std::string& path, float volume) override;

        virtual void* CreateSoundSource(const std::string& filepath, bool looping, float minDistance, float maxDistance) override;
        virtual void UpdateSoundSource(void* sourceData, const glm::vec3& position, float volume, float pitch) override;
        virtual void PlaySoundSource(void* sourceData) override;
        virtual void StopSoundSource(void* sourceData) override;
        virtual void DestroySoundSource(void* sourceData) override;

        void LoadBank(const std::string& bankFilePath);
        virtual void UnloadBank(const std::string& bankFilePath) override;
        virtual void SetEventParameter(void* sourceData, const std::string& paramName, float value) override;

    private:
        FMOD::Studio::System* m_StudioSystem = nullptr;
        FMOD::System* m_CoreSystem = nullptr;

        std::unordered_map<std::string, FMOD::Sound*> m_SoundCache;
        std::unordered_map<std::string, FMOD::Studio::Bank*> m_BankCache;
    };
}