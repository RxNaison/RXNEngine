#include "rxnpch.h"
#include "OpenGLGPUProfiler.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Core/Log.h"

#include <glad/glad.h>

namespace RXNEngine {

	OpenGLGPUProfiler::~OpenGLGPUProfiler()
	{
		if (!m_Initialized)
			return;

		for (uint32_t i = 0; i < FramesInFlight; i++)
			glDeleteQueries(MaxScopesPerFrame * 2, m_Slots[i].Queries);
	}

	void OpenGLGPUProfiler::LazyInit()
	{
		if (m_Initialized)
			return;

		for (uint32_t i = 0; i < FramesInFlight; i++)
			glGenQueries(MaxScopesPerFrame * 2, m_Slots[i].Queries);

		m_Initialized = true;
	}

	void OpenGLGPUProfiler::BeginFrame()
	{
		if (!m_Enabled)
			return;

		if (m_InFrame)
			EndFrame();

		LazyInit();

		{
			auto now = std::chrono::steady_clock::now();
			if (m_HasLastBeginFrame)
			{
				float frameMs = (float)std::chrono::duration<double, std::milli>(now - m_LastBeginFrame).count();
				constexpr float kHitchThresholdMs = 120.0f;
				if (frameMs > kHitchThresholdMs)
				{
					RXN_CORE_WARN("[Hitch] frame took {0:.1f} ms (last GPU frame {1:.2f} ms, dropped resolves {2})", frameMs, m_FrameGPUTime, m_DroppedResolves);
					for (const ScopeResult& r : m_Results)
						RXN_CORE_WARN("[Hitch]   {0}{1}: GPU {2:.2f} ms | CPU {3:.2f} ms", std::string((size_t)r.Depth * 2, ' '), r.Name, r.Milliseconds, r.CPUMilliseconds);
				}
			}
			m_LastBeginFrame = now;
			m_HasLastBeginFrame = true;
		}

		m_CurrentSlot = (m_CurrentSlot + 1) % FramesInFlight;
		FrameSlot& slot = m_Slots[m_CurrentSlot];

		if (slot.Pending)
			ResolveSlot(slot);

		slot.Scopes.clear();
		slot.UsedQueries = 0;
		m_ScopeStack.clear();
		m_CurrentDepth = 0;
		m_DroppedDepth = 0;
		m_InFrame = true;
	}

	void OpenGLGPUProfiler::EndFrame()
	{
		if (!m_InFrame)
			return;

		while (!m_ScopeStack.empty())
			EndScope();

		FrameSlot& slot = m_Slots[m_CurrentSlot];
		slot.Pending = !slot.Scopes.empty();
		m_InFrame = false;
	}

	void OpenGLGPUProfiler::BeginScope(const char* name)
	{
		if (!m_Enabled || !m_InFrame)
			return;

		FrameSlot& slot = m_Slots[m_CurrentSlot];
		if (slot.UsedQueries + 2 > MaxScopesPerFrame * 2)
		{
			m_DroppedDepth++;
			return;
		}

		m_ScopeStack.push_back((uint32_t)slot.Scopes.size());

		Scope& scope = slot.Scopes.emplace_back();
		scope.Name = name;
		scope.Depth = m_CurrentDepth++;
		scope.BeginQuery = slot.UsedQueries++;
		scope.CPUBegin = std::chrono::steady_clock::now();

		glQueryCounter(slot.Queries[scope.BeginQuery], GL_TIMESTAMP);
	}

	void OpenGLGPUProfiler::EndScope()
	{
		if (!m_Enabled || !m_InFrame)
			return;

		if (m_DroppedDepth > 0)
		{
			m_DroppedDepth--;
			return;
		}

		if (m_ScopeStack.empty())
			return;

		FrameSlot& slot = m_Slots[m_CurrentSlot];
		Scope& scope = slot.Scopes[m_ScopeStack.back()];
		m_ScopeStack.pop_back();

		scope.CPUEnd = std::chrono::steady_clock::now();
		scope.EndQuery = slot.UsedQueries++;
		glQueryCounter(slot.Queries[scope.EndQuery], GL_TIMESTAMP);

		if (m_CurrentDepth > 0)
			m_CurrentDepth--;
	}

	void OpenGLGPUProfiler::ResolveSlot(FrameSlot& slot)
	{
		slot.Pending = false;

		if (slot.Scopes.empty() || slot.UsedQueries == 0)
			return;

		{
			GLint available = 0;
			glGetQueryObjectiv(slot.Queries[slot.UsedQueries - 1], GL_QUERY_RESULT_AVAILABLE, &available);
			if (!available)
			{
				m_DroppedResolves++;
				return;
			}
		}

		m_Results.clear();
		m_Results.reserve(slot.Scopes.size());

		uint64_t frameMin = UINT64_MAX;
		uint64_t frameMax = 0;

		for (const Scope& scope : slot.Scopes)
		{
			if (scope.BeginQuery == InvalidQuery || scope.EndQuery == InvalidQuery)
				continue;

			uint64_t t0 = 0, t1 = 0;
			glGetQueryObjectui64v(slot.Queries[scope.BeginQuery], GL_QUERY_RESULT, &t0);
			glGetQueryObjectui64v(slot.Queries[scope.EndQuery], GL_QUERY_RESULT, &t1);

			ScopeResult& result = m_Results.emplace_back();
			result.Name = scope.Name;
			result.Depth = scope.Depth;
			result.Milliseconds = (float)((double)(t1 - t0) / 1000000.0);
			result.CPUMilliseconds = (float)std::chrono::duration<double, std::milli>(scope.CPUEnd - scope.CPUBegin).count();

			if (t0 < frameMin) frameMin = t0;
			if (t1 > frameMax) frameMax = t1;
		}

		m_FrameGPUTime = (frameMax > frameMin) ? (float)((double)(frameMax - frameMin) / 1000000.0) : 0.0f;
	}

}
