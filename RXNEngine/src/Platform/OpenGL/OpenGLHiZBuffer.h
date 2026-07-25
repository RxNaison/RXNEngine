#pragma once

#include "RXNEngine/Renderer/HiZBuffer.h"
#include "RXNEngine/Renderer/GraphicsAPI/Shader.h"

#include <cstdint>

namespace RXNEngine {

	class OpenGLHiZBuffer : public HiZBuffer
	{
	public:
		OpenGLHiZBuffer() = default;
		virtual ~OpenGLHiZBuffer();

		virtual void Init(uint32_t width, uint32_t height) override;
		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual void Build(uint32_t srcDepthTextureId, const glm::mat4& invViewProjection) override;

		virtual uint32_t GetTextureID() const override { return m_TextureID; }
		virtual uint32_t GetMipLevels() const override { return m_MipLevels; }

		virtual const OcclusionGrid& GetOcclusionGrid() const override { return m_Grid; }

	private:
		void Allocate();
		void Destroy();

	private:
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint32_t m_MipLevels = 1;
		uint32_t m_TextureID = 0;
		uint32_t m_FBO = 0;
		Ref<Shader> m_BuildShader;

		uint32_t m_OcclusionMip = 0;
		uint32_t m_OcclusionW = 0;
		uint32_t m_OcclusionH = 0;
		uint32_t m_PBOs[2] = { 0, 0 };
		glm::mat4 m_PBOInvVP[2] = { glm::mat4(1.0f), glm::mat4(1.0f) };
		bool m_PBOPending[2] = { false, false };
		uint32_t m_PBOFrame = 0;
		OcclusionGrid m_Grid;
	};

}
