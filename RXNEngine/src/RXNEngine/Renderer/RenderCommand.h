#pragma once

#include "RendererAPI.h"

namespace RXNEngine {

	class RenderCommand
	{
	public:

		inline static void Init()
		{
			s_RendererAPI->Init();
		}

		inline static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewport(x, y, width, height);
		}

		inline static void SetClearColor(const glm::vec4& color)
		{
			s_RendererAPI->SetClearColor(color);
		}

		inline static void Clear()
		{
			s_RendererAPI->Clear();
		}

		inline static void Draw(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
		{
			s_RendererAPI->Draw(vertexArray, vertexCount);
		}

		inline static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0)
		{
			s_RendererAPI->DrawIndexed(vertexArray, indexCount);
		}

		inline static void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
		{
			s_RendererAPI->DrawLines(vertexArray, vertexCount);
		}

		inline static void SetLineWidth(float width)
		{
			s_RendererAPI->SetLineWidth(width);
		}

		inline static void DrawIndexedInstanced(const Ref<VertexArray>& vertexArray, const Ref<VertexBuffer>& instanceData, uint32_t instanceCount)
		{
			s_RendererAPI->DrawIndexedInstanced(vertexArray, instanceData, instanceCount);
		}

		inline static void BindTextureID(uint32_t slot, uint32_t textureID)
		{
			s_RendererAPI->BindTextureID(slot, textureID);
		}

		inline static void SetDepthTest(bool enabled)
		{
			s_RendererAPI->SetDepthTest(enabled);
		}

		inline static void SetDepthFunc(RendererAPI::DepthFunc func)
		{
			s_RendererAPI->SetDepthFunc(func);
		}

		inline static void SetCullFace(RendererAPI::CullFace face)
		{
			s_RendererAPI->SetCullFace(face);
		}

		inline static void BindDefaultFramebuffer()
		{
			s_RendererAPI->BindDefaultFramebuffer();
		}
	private:
		static Scope<RendererAPI> s_RendererAPI;
	};

}