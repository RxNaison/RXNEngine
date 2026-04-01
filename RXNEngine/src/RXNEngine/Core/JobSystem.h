#pragma once
#include "RXNEngine/Core/Subsystem.h"
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <queue>

namespace RXNEngine {

    struct JobDispatchArgs
    {
        uint32_t JobIndex;
        uint32_t GroupIndex;
    };

    class JobSystem : public Subsystem
    {
    public:
        JobSystem() = default;

        virtual void Init() override;
        virtual void Shutdown() override;

        void Execute(const std::function<void()>& job);
        void Dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(JobDispatchArgs)>& job);
        void Wait();

        bool IsMainThread() const;
        uint32_t GetThreadCount() const;

    private:
        void WorkerThread(uint32_t threadID);

    private:
        uint32_t m_NumThreads = 0;
        std::thread::id m_MainThreadID;

        std::vector<std::thread> m_Workers;

        std::queue<std::function<void()>> m_JobQueue;
        std::mutex m_QueueMutex;
        std::condition_variable m_WakeCondition;

        std::atomic<uint64_t> m_CurrentLabel = 0;
        std::atomic<uint64_t> m_FinishedLabel = 0;
        bool m_IsRunning = false;
    };

}