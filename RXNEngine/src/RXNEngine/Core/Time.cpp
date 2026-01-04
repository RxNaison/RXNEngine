#include "rxnpch.h"
#include "Time.h"

#include <GLFW/glfw3.h>

Time::Time()
{
    m_LastFrameTime = glfwGetTime();
}

void Time::OnFrameStart()
{
    float currentTime = glfwGetTime();
    m_DeltaTime = currentTime - m_LastFrameTime;
    m_LastFrameTime = currentTime;

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