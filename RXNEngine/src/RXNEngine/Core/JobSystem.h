#pragma once
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

    class JobSystem
    {
    public:
        static void Init();
        static void Shutdown();

        static void Execute(const std::function<void()>& job);

        static void Dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(JobDispatchArgs)>& job);

        static void Wait();

        static bool IsMainThread();

        static uint32_t GetThreadCount();

    private:
        static void WorkerThread(uint32_t threadID);
    };

}