#include "rxnpch.h"
#include "JobSystem.h"

#include <optick.h>

namespace RXNEngine {

    void JobSystem::Init()
    {
        m_IsRunning = true;
        m_MainThreadID = std::this_thread::get_id();

        uint32_t coreCount = std::thread::hardware_concurrency();
        m_NumThreads = (coreCount > 1) ? coreCount - 1 : 1;

        RXN_CORE_INFO("JobSystem: Initializing {0} Worker Threads...", m_NumThreads);

        for (uint32_t i = 0; i < m_NumThreads; ++i)
            m_Workers.emplace_back(std::thread(&JobSystem::WorkerThread, this, i));
    }

    void JobSystem::Shutdown()
    {
        if (!m_IsRunning) return;

        m_IsRunning = false;
        m_WakeCondition.notify_all();

        for (auto& thread : m_Workers)
        {
            if (thread.joinable())
                thread.join();
        }

        m_Workers.clear();
        RXN_CORE_INFO("JobSystem: Shutdown successful.");
    }

    void JobSystem::Execute(const std::function<void()>& job)
    {
        m_CurrentLabel.fetch_add(1);

        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            m_JobQueue.push(job);
        }

        m_WakeCondition.notify_one();
    }

    void JobSystem::Dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(JobDispatchArgs)>& job)
    {
        if (jobCount == 0 || groupSize == 0)
            return;

        uint32_t groupCount = (jobCount + groupSize - 1) / groupSize;

        m_CurrentLabel.fetch_add(groupCount);

        for (uint32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
        {
            auto jobGroup = [groupIndex, groupSize, jobCount, job]()
                {
                    uint32_t groupJobOffset = groupIndex * groupSize;
                    uint32_t groupJobEnd = std::min(groupJobOffset + groupSize, jobCount);

                    for (uint32_t i = groupJobOffset; i < groupJobEnd; ++i)
                    {
                        JobDispatchArgs args;
                        args.JobIndex = i;
                        args.GroupIndex = groupIndex;

                        job(args);
                    }
                };

            {
                std::lock_guard<std::mutex> lock(m_QueueMutex);
                m_JobQueue.push(jobGroup);
            }

            m_WakeCondition.notify_one();
        }
    }

    void JobSystem::Wait()
    {
        while (m_FinishedLabel.load() < m_CurrentLabel.load())
        {
            std::function<void()> job;
            bool hasJob = false;

            {
                std::lock_guard<std::mutex> lock(m_QueueMutex);
                if (!m_JobQueue.empty())
                {
                    job = m_JobQueue.front();
                    m_JobQueue.pop();
                    hasJob = true;
                }
            }

            if (hasJob)
            {
                job();
                m_FinishedLabel.fetch_add(1);
            }
            else
            {
                std::unique_lock<std::mutex> lock(m_WaitMutex);
                m_WaitCondition.wait(lock, [this]() { return m_FinishedLabel.load() >= m_CurrentLabel.load(); });
            }
        }
    }

    bool JobSystem::IsMainThread() const
    {
        return std::this_thread::get_id() == m_MainThreadID;
    }

    uint32_t JobSystem::GetThreadCount() const
    {
        return m_NumThreads + 1;
    }

    void JobSystem::WorkerThread(uint32_t threadID)
    {
        std::string threadName = "Worker Thread " + std::to_string(threadID);
        OPTICK_THREAD(threadName.c_str());

        while (m_IsRunning)
        {
            std::function<void()> job;

            {
                std::unique_lock<std::mutex> lock(m_QueueMutex);

                m_WakeCondition.wait(lock, [this] { return !m_JobQueue.empty() || !m_IsRunning; });

                if (!m_IsRunning) break;

                job = m_JobQueue.front();
                m_JobQueue.pop();
            }

            if (job)
            {
                job();
                m_FinishedLabel.fetch_add(1);

                {
                    std::lock_guard<std::mutex> waitLock(m_WaitMutex);
                }

                m_WaitCondition.notify_all();
            }
        }
    }

}