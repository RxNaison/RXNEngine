#pragma once

#include <glm/glm.hpp>

#include "RXNEngine/Renderer/GraphicsAPI/VertexArray.h"

namespace RXNEngine {

	class RenderTarget;

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
			None = 0,
			Back,
			Front,
			FrontAndBack
		};

		enum class BlendFactor
		{
			Zero = 0,
			One,
			SrcColor,
			OneMinusSrcColor,
			DstColor,
			OneMinusDstColor,
			SrcAlpha,
			OneMinusSrcAlpha,
			DstAlpha,
			OneMinusDstAlpha
		};

		enum class BlendEquation
		{
			Add = 0,
			Subtract,
			ReverseSubtract,
			Min,
			Max
		};

		enum class StencilFunc
		{
			Always = 0,
			NotEqual,
			Equal,
			Less,
			LessEqual,
			Greater,
			GreaterEqual
		};

		enum class StencilOp
		{
			Keep = 0,
			Replace,
			Zero,
			Increment,
			IncrementWrap,
			Decrement,
			DecrementWrap,
			Invert
		};

		enum class BarrierType
		{
			None = 0,
			ShaderStorage,
			VertexAttribArray,
			ShaderStorageAndAttribArray
		};
	public:
		virtual void Init() = 0;

		virtual void BindDefaultRenderTarget() = 0;

		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		virtual void SetClearColor(const glm::vec4& color) = 0;
		virtual void Clear() = 0;

		virtual void SetDepthTest(bool enabled) = 0;
		virtual void SetDepthFunc(DepthFunc func) = 0;
		virtual void SetCullFace(CullFace face) = 0;

		virtual void SetBlend(bool enabled) = 0;
		virtual void SetBlendFunc(BlendFactor source, BlendFactor destination) = 0;
		virtual void SetBlendEquation(BlendEquation equation) = 0;

		virtual void SetScissorTest(bool enabled) = 0;
		virtual bool IsScissorTestEnabled() = 0;

		virtual void BlitRenderTarget(const Ref<RenderTarget>& src, const Ref<RenderTarget>& dst) = 0;

		virtual void SetStencilTest(bool enabled) = 0;
		virtual void SetStencilMask(uint32_t mask) = 0;
		virtual void SetStencilFunc(StencilFunc func, int ref, uint32_t mask) = 0;
		virtual void SetStencilOp(StencilOp fail, StencilOp zfail, StencilOp zpass) = 0;

		virtual void SetDepthMask(bool writeEnabled) = 0;
		virtual void SetColorMask(bool r, bool g, bool b, bool a) = 0;
		virtual void SetColorMaskIndexed(uint32_t attachmentIndex, bool r, bool g, bool b, bool a) = 0;

		struct TextureUnitBinding
		{
			uint32_t Unit = 0;
			uint32_t Tex2D = 0;
			uint32_t TexCube = 0;
			uint32_t Tex2DArray = 0;
			uint32_t TexCubeArray = 0;
		};
		virtual void QueryTextureBindings(std::vector<TextureUnitBinding>& outBindings, uint32_t unitCount) = 0;

		virtual void BindTextureID(uint32_t slot, uint32_t textureID) = 0;

		virtual void Draw(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) = 0;
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;
		virtual void DrawIndexedInstanced(const Ref<VertexArray>& vertexArray, const Ref<VertexBuffer>& transformBuffer, uint32_t instanceCount, uint32_t indexCount = 0, uint32_t baseIndex = 0) = 0;
		virtual void DrawIndexedInstancedStream(const Ref<VertexArray>& vertexArray, uint32_t instanceBufferID, uint32_t instanceStride, uint32_t instanceOffsetBytes, uint32_t instanceCount, uint32_t indexCount = 0, uint32_t baseIndex = 0) = 0;
		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) = 0;

		virtual void SetLineWidth(float width) = 0;

		virtual void DispatchCompute(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ) = 0;
		virtual void MemoryBarrier(BarrierType type) = 0;

		inline static API GetAPI() { return s_API; }
	private:
		static API s_API;
	};

}