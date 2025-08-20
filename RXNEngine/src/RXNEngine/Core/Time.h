#pragma once

#include <chrono>

class Time
{
public:
    static Time& Get()
    {
        static Time instance;
        return instance;
    }

    void OnFrameStart();

    bool ShouldRunFixedUpdate();

    inline float GetDeltaTime() const { return m_DeltaTime; }
    inline float GetFixedDeltaTime() const { return m_FixedDeltaTime; }
    float GetSystemTime();

private:
    Time();

    Time(const Time&) = delete;
    Time& operator=(const Time&) = delete;
    Time(Time&&) = delete;
    Time& operator=(Time&&) = delete;

private:
    float m_DeltaTime = 0.0f;
    float m_MaxDeltaTime = 0.25f;
    const float m_FixedDeltaTime = 0.02f;

    float m_LastFrameTime = 0.0f;
    float m_Accumulator = 0.0f;
};