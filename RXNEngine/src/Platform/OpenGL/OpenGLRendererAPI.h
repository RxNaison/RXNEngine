#pragma once

#include "RXNEngine/Renderer/RendererAPI.h"

namespace RXNEngine {

	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		virtual void Init() override;

		virtual void BindDefaultRenderTarget() override;

		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void Clear() override;

		virtual void SetDepthTest(bool enabled);
		virtual void SetDepthFunc(DepthFunc func);
		virtual void SetCullFace(CullFace face);

		virtual void SetBlend(bool enabled) override;
		virtual void SetBlendFunc(BlendFactor source, BlendFactor destination) override;
		virtual void SetBlendEquation(BlendEquation equation) override;

		virtual void BindTextureID(uint32_t slot, uint32_t textureID);

		virtual void Draw(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) override;
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) override;

		virtual void DrawIndexedInstanced(const Ref<VertexArray>& vertexArray, const Ref<VertexBuffer>& transformBuffer,
			uint32_t instanceCount, uint32_t indexCount, uint32_t baseIndex) override;

		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) override;

		void SetLineWidth(float width) override;
	};

}