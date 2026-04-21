#include "rxnpch.h"
#include "Time.h"

#include <SDL3/SDL.h>

Time::Time()
{
    m_LastFrameTime = SDL_GetPerformanceCounter();
}

void Time::OnFrameStart()
{
    uint64_t currentCounter = SDL_GetPerformanceCounter();

    uint64_t counterDelta = currentCounter - m_LastFrameTime;
    m_LastFrameTime = currentCounter;

    m_DeltaTime = (float)((double)counterDelta / (double)SDL_GetPerformanceFrequency());

    if (m_DeltaTime > m_MaxDeltaTime)
    {
        m_DeltaTime = m_MaxDeltaTime;
    }

    m_Accumulator += m_DeltaTime;
}

bool Time::ShouldRunFixedUpdate()
{
    if (m_Accumulator >= m_FixedDeltaTime)
    {
        m_Accumulator -= m_FixedDeltaTime;
        return true;
    }
    return false;
}

float Time::GetSystemTime()
{
    using namespace std::chrono;
    return duration_cast<duration<float>>(high_resolution_clock::now().time_since_epoch()).count();
}