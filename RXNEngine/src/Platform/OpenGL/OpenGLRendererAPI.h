#pragma once

#include "RXNEngine/Renderer/RendererAPI.h"

namespace RXNEngine {

	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		virtual void Init() override;

		virtual void BindDefaultFramebuffer() override;

		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void Clear() override;

		virtual void SetDepthTest(bool enabled);
		virtual void SetDepthFunc(DepthFunc func);
		virtual void SetCullFace(CullFace face);

		virtual void BindTextureID(uint32_t slot, uint32_t textureID);

		virtual void Draw(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) override;
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) override;
		virtual void DrawIndexedInstanced(const Ref<VertexArray>& vertexArray, const Ref<VertexBuffer>& instanceData, uint32_t instanceCount) override;
		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) override;

		void SetLineWidth(float width) override;
	};

}