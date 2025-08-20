#include "rxnpch.h"
#include "Texture.h"

#include "RXNEngine/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace RXNEngine {

	Ref<Texture2D> Texture2D::Create(const TextureSpecification& specification)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    RXN_CORE_ASSERT(false, "RendererAPI::None is not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return CreateRef<OpenGLTexture2D>(specification);
		}

		RXN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}


	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    RXN_CORE_ASSERT(false, "RendererAPI::None is not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return CreateRef<OpenGLTexture2D>(path);
		}

		RXN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}