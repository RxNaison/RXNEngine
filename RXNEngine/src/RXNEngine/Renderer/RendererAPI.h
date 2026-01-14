#pragma once

#include <glm/glm.hpp>

#include "VertexArray.h"

namespace RXNEngine {

	class RendererAPI
	{
	public:
		enum class API
		{
			None = 0, OpenGL
		};

		enum class DepthFunc
		{
			Less = 0,
			LessEqual,
			Equal,
			Greater,
			GreaterEqual,
			NotEqual,
			Always,
			Never
		};

		enum class CullFace
		{
			Back = 0,
			Front,
			FrontAndBack
		};
	public:
		virtual void Init() = 0;

		virtual void BindDefaultFramebuffer() = 0;

		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		virtual void SetClearColor(const glm::vec4& color) = 0;
		virtual void Clear() = 0;

		virtual void SetDepthTest(bool enabled) = 0;
		virtual void SetDepthFunc(DepthFunc func) = 0;
		virtual void SetCullFace(CullFace face) = 0;

		virtual void BindTextureID(uint32_t slot, uint32_t textureID) = 0;

		virtual void Draw(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) = 0;
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;
		virtual void DrawIndexedInstanced(const Ref<VertexArray>& vertexArray, const Ref<VertexBuffer>& instanceData, uint32_t instanceCount) = 0;
		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) = 0;

		virtual void SetLineWidth(float width) = 0;

		inline static API GetAPI() { return s_API; }
		//static Scope<RendererAPI> Create();
	private:
		static API s_API;
	};

}