#pragma once

#include "RXNEngine/Renderer/RenderTarget.h"

namespace RXNEngine {

	class OpenGLRenderTarget : public RenderTarget
	{
	public:
		OpenGLRenderTarget(const RenderTargetSpecification& spec);
		virtual ~OpenGLRenderTarget();

		virtual void Bind() override;
		virtual void Unbind() override;

		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) override;

		virtual void ClearAttachment(uint32_t attachmentIndex, int value) override;

		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override { RXN_CORE_ASSERT(index < m_ColorAttachments.size()); return m_ColorAttachments[index]; }

		virtual const RenderTargetSpecification& GetSpecification() const override { return m_Specification; }
	private:
		void UpdateState();
	private:
		uint32_t m_RendererID = 0;
		RenderTargetSpecification m_Specification;

		std::vector<RenderTargetTextureSpecification> m_ColorAttachmentSpecifications;
		RenderTargetTextureSpecification m_DepthAttachmentSpecification = RenderTargetTextureFormat::None;

		std::vector<uint32_t> m_ColorAttachments;
		uint32_t m_DepthAttachment = 0;
	};

}