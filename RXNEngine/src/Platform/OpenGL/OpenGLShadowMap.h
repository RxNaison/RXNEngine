#pragma once
#include "RXNEngine/Renderer/ShadowMap.h"

namespace RXNEngine {

	class OpenGLShadowMap : public ShadowMap
	{
	public:
		virtual ~OpenGLShadowMap();

		virtual void Init(uint32_t size, uint32_t layers = 4, bool isCubemap = false) override;
		virtual void BindWriteLayer(uint32_t layer, uint32_t face = 0) override;
		virtual void BindRead(uint32_t slot) override;

		virtual uint32_t GetSize() const override { return m_Size; }
		virtual uint32_t GetRendererID() const override { return m_DepthMapTexture; }

	private:
		uint32_t m_FBO = 0;
		uint32_t m_DepthMapTexture = 0;
		uint32_t m_Size = 0;
		bool m_IsCubemap = false;
	};
}