#include "rxnpch.h"
#include "JobSystem.h"

#include <optick.h>

namespace RXNEngine {

    namespace {
        uint32_t s_NumThreads = 0;
        std::thread::id s_MainThreadID;

        std::vector<std::thread> s_Workers;

        std::queue<std::function<void()>> s_JobQueue;
        std::mutex s_QueueMutex;
        std::condition_variable s_WakeCondition;

        std::atomic<uint64_t> s_CurrentLabel = 0;
        std::atomic<uint64_t> s_FinishedLabel = 0;
        bool s_IsRunning = false;
    }

    void JobSystem::Init()
    {
        s_IsRunning = true;
        s_MainThreadID = std::this_thread::get_id();

        uint32_t coreCount = std::thread::hardware_concurrency();
        s_NumThreads = (coreCount > 1) ? coreCount - 1 : 1;

        RXN_CORE_INFO("JobSystem: Initializing {0} Worker Threads...", s_NumThreads);

        for (uint32_t i = 0; i < s_NumThreads; ++i)
        {
            s_Workers.emplace_back(std::thread(&JobSystem::WorkerThread, i));
        }
    }

    void JobSystem::Shutdown()
    {
        if (!s_IsRunning) return;

        s_IsRunning = false;

        s_WakeCondition.notify_all();

        for (auto& thread : s_Workers)
        {
            if (thread.joinable())
                thread.join();
        }

        s_Workers.clear();
        RXN_CORE_INFO("JobSystem: Shutdown successful.");
    }

    void JobSystem::Execute(const std::function<void()>& job)
    {
        s_CurrentLabel.fetch_add(1);

        {
            std::lock_guard<std::mutex> lock(s_QueueMutex);
            s_JobQueue.push(job);
        }

        s_WakeCondition.notify_one();
    }

    void JobSystem::Dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(JobDispatchArgs)>& job)
    {
        if (jobCount == 0 || groupSize == 0) return;

        uint32_t groupCount = (jobCount + groupSize - 1) / groupSize;

        s_CurrentLabel.fetch_add(groupCount);

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
                std::lock_guard<std::mutex> lock(s_QueueMutex);
                s_JobQueue.push(jobGroup);
            }

            s_WakeCondition.notify_one();
        }
    }

    void JobSystem::Wait()
    {
        while (s_FinishedLabel.load() < s_CurrentLabel.load())
        {
            std::function<void()> job;
            bool hasJob = false;

            {
                std::lock_guard<std::mutex> lock(s_QueueMutex);
                if (!s_JobQueue.empty())
                {
                    job = s_JobQueue.front();
                    s_JobQueue.pop();
                    hasJob = true;
                }
            }

            if (hasJob)
            {
                job();
                s_FinishedLabel.fetch_add(1);
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    bool JobSystem::IsMainThread()
    {
        return std::this_thread::get_id() == s_MainThreadID;
    }

    uint32_t JobSystem::GetThreadCount()
    {
        return s_NumThreads + 1;
    }

    void JobSystem::WorkerThread(uint32_t threadID)
    {
        std::string threadName = "Worker Thread " + std::to_string(threadID);
        OPTICK_THREAD(threadName.c_str());

        while (s_IsRunning)
        {
            std::function<void()> job;

            {
                std::unique_lock<std::mutex> lock(s_QueueMutex);

                s_WakeCondition.wait(lock, [] { return !s_JobQueue.empty() || !s_IsRunning; });

                if (!s_IsRunning) break;

                job = s_JobQueue.front();
                s_JobQueue.pop();
            }

            job();

            s_FinishedLabel.fetch_add(1);
        }
    }

}