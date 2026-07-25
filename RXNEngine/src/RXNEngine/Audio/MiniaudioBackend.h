#pragma once
#include "AudioBackend.h"
#include <miniaudio.h>

namespace RXNEngine {

    class MiniaudioBackend : public AudioBackend
    {
    public:
        virtual void Init() override;
        virtual void Shutdown() override;

        virtual void UpdateListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up, const glm::vec3& velocity) override;
        virtual void PlayOneShot(const std::string& filepath, float volume) override;

        virtual void* CreateSoundSource(const std::string& filepath, bool looping, float minDistance, float maxDistance) override;
        virtual void UpdateSoundSource(void* sourceData, const glm::vec3& position, const glm::vec3& velocity, float volume, float pitch, float minDistance, float maxDistance, float occlusion, float spread = 0.0f) override;
        virtual void PlaySoundSource(void* sourceData) override;
        virtual void StopSoundSource(void* sourceData) override;
        virtual void DestroySoundSource(void* sourceData) override;
        virtual void SetReverbPreset(int preset, float volume) override;

    private:
        ma_engine m_Engine;
    };
}