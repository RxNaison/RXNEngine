#include "rxnpch.h"
#include "OpenGLHiZBuffer.h"
#include "RXNEngine/Renderer/GraphicsAPI/VertexArray.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace RXNEngine {

	static Ref<VertexArray> s_HiZTriVAO;

	static void RenderHiZTriangle()
	{
		if (s_HiZTriVAO == nullptr)
		{
			float vertices[] = {
				-1.0f, -1.0f,
				 3.0f, -1.0f,
				-1.0f,  3.0f
			};

			s_HiZTriVAO = VertexArray::Create();
			Ref<VertexBuffer> vbo = VertexBuffer::Create(vertices, sizeof(vertices));
			vbo->SetLayout({ { ShaderDataType::Float2, "a_Position" } });
			s_HiZTriVAO->AddVertexBuffer(vbo);
		}

		s_HiZTriVAO->Bind();
		glDrawArrays(GL_TRIANGLES, 0, 3);
		s_HiZTriVAO->Unbind();
	}

	OpenGLHiZBuffer::~OpenGLHiZBuffer()
	{
		Destroy();
	}

	void OpenGLHiZBuffer::Destroy()
	{
		if (m_TextureID)
			glDeleteTextures(1, &m_TextureID); m_TextureID = 0;

		if (m_FBO)
			glDeleteFramebuffers(1, &m_FBO); m_FBO = 0;

		if (m_PBOs[0])
			glDeleteBuffers(2, m_PBOs); m_PBOs[0] = m_PBOs[1] = 0;
	}

	void OpenGLHiZBuffer::Allocate()
	{
		uint32_t maxDim = std::max(m_Width, m_Height);
		m_MipLevels = 1u + (uint32_t)std::floor(std::log2((double)std::max(maxDim, 1u)));

		glGenTextures(1, &m_TextureID);
		glBindTexture(GL_TEXTURE_2D, m_TextureID);
		glTexStorage2D(GL_TEXTURE_2D, (GLsizei)m_MipLevels, GL_RG32F, (GLsizei)m_Width, (GLsizei)m_Height);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)(m_MipLevels - 1));
		glBindTexture(GL_TEXTURE_2D, 0);

		m_OcclusionMip = 0;

		while (std::max(m_Width >> m_OcclusionMip, 1u) > 64 && m_OcclusionMip + 1 < m_MipLevels)
			m_OcclusionMip++;

		m_OcclusionW = std::max(m_Width >> m_OcclusionMip, 1u);
		m_OcclusionH = std::max(m_Height >> m_OcclusionMip, 1u);

		m_Grid.Width = m_OcclusionW;
		m_Grid.Height = m_OcclusionH;
		m_Grid.Texels.assign((size_t)m_OcclusionW * m_OcclusionH, glm::vec2(0.0f));
		m_Grid.Valid = false;

		if (m_PBOs[0])
			glDeleteBuffers(2, m_PBOs); m_PBOs[0] = m_PBOs[1] = 0;

		glCreateBuffers(2, m_PBOs);
		const uint32_t pboSize = m_OcclusionW * m_OcclusionH * 2u * (uint32_t)sizeof(float);
		glNamedBufferData(m_PBOs[0], pboSize, nullptr, GL_STREAM_READ);
		glNamedBufferData(m_PBOs[1], pboSize, nullptr, GL_STREAM_READ);
		m_PBOPending[0] = m_PBOPending[1] = false;
	}

	void OpenGLHiZBuffer::Init(uint32_t width, uint32_t height)
	{
		m_Width = std::max(width, 1u);
		m_Height = std::max(height, 1u);

		glGenFramebuffers(1, &m_FBO);
		Allocate();

		m_BuildShader = Shader::Create("res/shaders/hiz_build.glsl");
	}

	void OpenGLHiZBuffer::Resize(uint32_t width, uint32_t height)
	{
		width = std::max(width, 1u);
		height = std::max(height, 1u);
		if (width == m_Width && height == m_Height && m_TextureID != 0)
			return;

		m_Width = width;
		m_Height = height;

		if (m_TextureID)
			glDeleteTextures(1, &m_TextureID); m_TextureID = 0;

		Allocate();
	}

	void OpenGLHiZBuffer::Build(uint32_t srcDepthTextureId, const glm::mat4& invViewProjection)
	{
		if (srcDepthTextureId == 0 || m_TextureID == 0 || !m_BuildShader)
			return;

		GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
		GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);

		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		m_BuildShader->Bind();
		m_BuildShader->SetInt("u_SrcTexture", 0);

		m_BuildShader->SetInt("u_Mode", 0);
		m_BuildShader->SetMat4("u_InvViewProjection", invViewProjection);
		glViewport(0, 0, (GLsizei)m_Width, (GLsizei)m_Height);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TextureID, 0);
		glBindTextureUnit(0, srcDepthTextureId);
		RenderHiZTriangle();

		m_BuildShader->SetInt("u_Mode", 1);
		glBindTextureUnit(0, m_TextureID);
		for (uint32_t mip = 1; mip < m_MipLevels; ++mip)
		{
			uint32_t srcMip = mip - 1;
			uint32_t dstW = std::max(m_Width >> mip, 1u);
			uint32_t dstH = std::max(m_Height >> mip, 1u);
			uint32_t srcW = std::max(m_Width >> srcMip, 1u);
			uint32_t srcH = std::max(m_Height >> srcMip, 1u);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, (GLint)srcMip);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)srcMip);

			m_BuildShader->SetFloat2("u_SrcTexelSize", glm::vec2(1.0f / (float)srcW, 1.0f / (float)srcH));
			glViewport(0, 0, (GLsizei)dstW, (GLsizei)dstH);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TextureID, (GLint)mip);
			RenderHiZTriangle();
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)(m_MipLevels - 1));


		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		if (depthWasEnabled)
			glEnable(GL_DEPTH_TEST);

		if (blendWasEnabled)
			glEnable(GL_BLEND);
	}

}
