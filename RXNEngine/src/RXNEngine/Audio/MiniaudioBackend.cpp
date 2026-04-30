#include "rxnpch.h"
#define MINIAUDIO_IMPLEMENTATION
#include "MiniaudioBackend.h"

namespace RXNEngine {

    void MiniaudioBackend::Init()
    {
        ma_engine_config engineConfig = ma_engine_config_init();
        ma_result result = ma_engine_init(&engineConfig, &m_Engine);
        RXN_CORE_ASSERT(result == MA_SUCCESS, "Failed to initialize Miniaudio!");
        RXN_CORE_INFO("Miniaudio Backend Initialized Successfully.");
    }

    void MiniaudioBackend::Shutdown()
    {
        ma_engine_uninit(&m_Engine);
    }

    void MiniaudioBackend::UpdateListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up)
    {
        ma_engine_listener_set_position(&m_Engine, 0, position.x, position.y, position.z);
        ma_engine_listener_set_direction(&m_Engine, 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(&m_Engine, 0, up.x, up.y, up.z);
    }

    void MiniaudioBackend::PlayOneShot(const std::string& filepath, float volume)
    {
        ma_engine_play_sound(&m_Engine, filepath.c_str(), nullptr);
    }

    void* MiniaudioBackend::CreateSoundSource(const std::string& filepath, bool looping, float minDistance, float maxDistance)
    {
        ma_sound* sound = new ma_sound();
        ma_result result = ma_sound_init_from_file(&m_Engine, filepath.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, nullptr, nullptr, sound);

        if (result != MA_SUCCESS)
        {
            RXN_CORE_ERROR("Failed to load audio file: {0}", filepath);
            delete sound;
            return nullptr;
        }

        ma_sound_set_looping(sound, looping ? MA_TRUE : MA_FALSE);
        ma_sound_set_min_distance(sound, minDistance);
        ma_sound_set_max_distance(sound, maxDistance);

        return sound;
    }

    void MiniaudioBackend::UpdateSoundSource(void* sourceData, const glm::vec3& position, float volume, float pitch)
    {
        if (!sourceData)
            return;

        ma_sound* sound = static_cast<ma_sound*>(sourceData);
        ma_sound_set_position(sound, position.x, position.y, position.z);
        ma_sound_set_volume(sound, volume);
        ma_sound_set_pitch(sound, pitch);
    }

    void MiniaudioBackend::PlaySoundSource(void* sourceData)
    {
        if (!sourceData)
            return;

        ma_sound_start(static_cast<ma_sound*>(sourceData));
    }

    void MiniaudioBackend::StopSoundSource(void* sourceData)
    {
        if (!sourceData)
            return;

        ma_sound_stop(static_cast<ma_sound*>(sourceData));
    }

    void MiniaudioBackend::DestroySoundSource(void* sourceData)
    {
        if (!sourceData)
            return;

        ma_sound* sound = static_cast<ma_sound*>(sourceData);
        ma_sound_uninit(sound);
        delete sound;
    }
}