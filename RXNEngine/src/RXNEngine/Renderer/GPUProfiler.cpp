#include "rxnpch.h"
#include "RXNEngine/Renderer/Renderer.h"
#include "GPUProfiler.h"
#include "Platform/OpenGL/OpenGLGPUProfiler.h"

namespace RXNEngine {

	GPUProfiler& GPUProfiler::Get()
	{
		static Ref<GPUProfiler> s_Instance = Create();
		return *s_Instance;
	}

	Ref<GPUProfiler> GPUProfiler::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    RXN_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLGPUProfiler>();
		}

		RXN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
