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

	Ref<Texture2D> Texture2D::WhiteTexture()
	{

		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    RXN_CORE_ASSERT(false, "RendererAPI::None is not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:
			{
				TextureSpecification spec;
				spec.GenerateMips = false;

				static Ref<Texture2D> s_WhiteTexture;
				if (!s_WhiteTexture)
				{
					s_WhiteTexture = CreateRef<OpenGLTexture2D>(spec);
					uint32_t white = 0xffffffff;
					s_WhiteTexture->SetData(&white, sizeof(uint32_t));
				}
				return s_WhiteTexture;
			}
		}

		RXN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::BlueTexture()
	{

		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    RXN_CORE_ASSERT(false, "RendererAPI::None is not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:
			{
				TextureSpecification spec;
				spec.GenerateMips = false;

				static Ref<Texture2D> s_WhiteTexture;
				if (!s_WhiteTexture)
				{
					s_WhiteTexture = CreateRef<OpenGLTexture2D>(spec);
					uint32_t white = 0xffff8080;
					s_WhiteTexture->SetData(&white, sizeof(uint32_t));
				}
				return s_WhiteTexture;
			}
		}

		RXN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}