#pragma once
#include "RXNEngine/Core/Base.h"
#include <glm/glm.hpp>
#include <string>

namespace RXNEngine {

    class AudioBackend
    {
    public:
        virtual ~AudioBackend() = default;

        virtual void Init() = 0;
        virtual void Shutdown() = 0;

        virtual void UpdateListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) = 0;
        virtual void PlayOneShot(const std::string& filepath, float volume) = 0;

        virtual void* CreateSoundSource(const std::string& filepath, bool looping, float minDistance, float maxDistance) = 0;
        virtual void UpdateSoundSource(void* sourceData, const glm::vec3& position, float volume, float pitch) = 0;
        virtual void PlaySoundSource(void* sourceData) = 0;
        virtual void StopSoundSource(void* sourceData) = 0;
        virtual void DestroySoundSource(void* sourceData) = 0;

        virtual void UnloadBank(const std::string& bankFilePath) {}
        virtual void SetEventParameter(void* sourceData, const std::string& paramName, float value) {}
    };
}