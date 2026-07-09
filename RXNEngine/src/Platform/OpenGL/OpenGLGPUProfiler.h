#pragma once

#include "RXNEngine/Renderer/GPUProfiler.h"

#include <chrono>
#include <cstdint>
#include <vector>

namespace RXNEngine {

	class OpenGLGPUProfiler : public GPUProfiler
	{
	public:
		OpenGLGPUProfiler() = default;
		virtual ~OpenGLGPUProfiler();

		virtual void BeginFrame() override;
		virtual void EndFrame() override;

		virtual void BeginScope(const char* name) override;
		virtual void EndScope() override;

		virtual void SetEnabled(bool enabled) override { m_Enabled = enabled; }
		virtual bool IsEnabled() const override { return m_Enabled; }

		virtual const std::vector<ScopeResult>& GetResults() const override { return m_Results; }
		virtual float GetFrameGPUTime() const override { return m_FrameGPUTime; }

	private:
		static constexpr uint32_t FramesInFlight = 8;
		static constexpr uint32_t MaxScopesPerFrame = 64;
		static constexpr uint32_t InvalidQuery = 0xFFFFFFFFu;

		struct Scope
		{
			std::string Name;
			uint32_t Depth = 0;
			uint32_t BeginQuery = InvalidQuery;
			uint32_t EndQuery = InvalidQuery;
			std::chrono::steady_clock::time_point CPUBegin;
			std::chrono::steady_clock::time_point CPUEnd;
		};

		struct FrameSlot
		{
			std::vector<Scope> Scopes;
			uint32_t UsedQueries = 0;
			bool Pending = false;
			uint32_t Queries[MaxScopesPerFrame * 2] = {};
		};

		void LazyInit();
		void ResolveSlot(FrameSlot& slot);

		bool m_Initialized = false;
		bool m_Enabled = true;
		bool m_InFrame = false;

		FrameSlot m_Slots[FramesInFlight];
		uint32_t m_CurrentSlot = 0;
		uint32_t m_CurrentDepth = 0;
		uint32_t m_DroppedDepth = 0;
		std::vector<uint32_t> m_ScopeStack;

		std::vector<ScopeResult> m_Results;
		float m_FrameGPUTime = 0.0f;

		std::chrono::steady_clock::time_point m_LastBeginFrame;
		bool m_HasLastBeginFrame = false;
		uint32_t m_DroppedResolves = 0;
	};

}
