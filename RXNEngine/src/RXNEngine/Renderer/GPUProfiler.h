#pragma once

#include "RXNEngine/Core/Base.h"

#include <cstdint>
#include <string>
#include <vector>

namespace RXNEngine {

	class GPUProfiler
	{
	public:
		struct ScopeResult
		{
			std::string Name;
			uint32_t Depth = 0;
			float Milliseconds = 0.0f;
			float CPUMilliseconds = 0.0f;
		};

		virtual ~GPUProfiler() = default;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		virtual void BeginScope(const char* name) = 0;
		virtual void EndScope() = 0;

		virtual void SetEnabled(bool enabled) = 0;
		virtual bool IsEnabled() const = 0;

		virtual const std::vector<ScopeResult>& GetResults() const = 0;
		virtual float GetFrameGPUTime() const = 0;

		static GPUProfiler& Get();

	private:
		static Ref<GPUProfiler> Create();
	};

	class GPUProfilerScope
	{
	public:
		GPUProfilerScope(const char* name) { GPUProfiler::Get().BeginScope(name); }
		~GPUProfilerScope() { GPUProfiler::Get().EndScope(); }

		GPUProfilerScope(const GPUProfilerScope&) = delete;
		GPUProfilerScope& operator=(const GPUProfilerScope&) = delete;
	};

#define RXN_GPU_SCOPE_CONCAT_INNER(a, b) a##b
#define RXN_GPU_SCOPE_CONCAT(a, b) RXN_GPU_SCOPE_CONCAT_INNER(a, b)
#define RXN_GPU_SCOPE(name) ::RXNEngine::GPUProfilerScope RXN_GPU_SCOPE_CONCAT(gpuProfilerScope, __LINE__)(name)

}
