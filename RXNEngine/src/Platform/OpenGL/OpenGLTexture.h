#pragma once

#include "RXNEngine/Renderer/Texture.h"
#include "glad/glad.h"

namespace RXNEngine {

	class OpenGLTexture2D : public Texture2D
	{
	public:
		OpenGLTexture2D(const std::string& path);
		virtual ~OpenGLTexture2D();

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }

		virtual void Bind(uint32_t slot = 0) const override;

	private:
		std::string m_Path;
		uint32_t m_Width, m_Height;
		uint32_t m_RendererID;
		bool m_IsLoaded;
		GLenum m_InternalFormat, m_DataFormat;
	};

}