#pragma once

#include "RXNEngine/Core/Base.h"

namespace RXNEngine {

	enum class RenderTargetTextureFormat
	{
		None = 0,

		// Color
		RGBA8,
		RGBA16F,
		RGBA32F,
		RED_INTEGER,

		// Depth/stencil
		DEPTH24STENCIL8,

		// Defaults
		Depth = DEPTH24STENCIL8
	};

	struct RenderTargetTextureSpecification
	{
		RenderTargetTextureSpecification() = default;
		RenderTargetTextureSpecification(RenderTargetTextureFormat format)
			: TextureFormat(format) {
		}

		RenderTargetTextureFormat TextureFormat = RenderTargetTextureFormat::None;
		// TODO: filtering/wrap
	};

	struct RenderTargetAttachmentSpecification
	{
		RenderTargetAttachmentSpecification() = default;
		RenderTargetAttachmentSpecification(std::initializer_list<RenderTargetTextureSpecification> attachments)
			: Attachments(attachments) {
		}

		std::vector<RenderTargetTextureSpecification> Attachments;
	};

	struct RenderTargetSpecification
	{
		float Width = 0, Height = 0;
		RenderTargetAttachmentSpecification Attachments;
		uint32_t Samples = 1;

		bool SwapChainTarget = false;
	};

	class RenderTarget
	{
	public:
		virtual ~RenderTarget() = default;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) = 0;

		virtual void ClearAttachment(uint32_t attachmentIndex, int value) = 0;

		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const = 0;

		virtual const RenderTargetSpecification& GetSpecification() const = 0;

		static Ref<RenderTarget> Create(const RenderTargetSpecification& spec);
	};


}