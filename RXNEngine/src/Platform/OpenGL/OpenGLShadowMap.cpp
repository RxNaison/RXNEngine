#include "rxnpch.h"
#include "OpenGLShadowMap.h"

#include <glad/glad.h>

namespace RXNEngine {
	OpenGLShadowMap::~OpenGLShadowMap()
	{
		glDeleteFramebuffers(1, &m_FBO);
		glDeleteTextures(1, &m_DepthMapTexture);
	}
	void OpenGLShadowMap::Init(uint32_t size)
	{
		m_Size = size;

		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_DepthMapTexture);
		glTextureStorage3D(m_DepthMapTexture, 1, GL_DEPTH_COMPONENT32F, m_Size, m_Size, 4);

		glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

		glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

		constexpr float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTextureParameterfv(m_DepthMapTexture, GL_TEXTURE_BORDER_COLOR, bordercolor);

		glCreateFramebuffers(1, &m_FBO);
		glNamedFramebufferTexture(m_FBO, GL_DEPTH_ATTACHMENT, m_DepthMapTexture, 0);

		glNamedFramebufferDrawBuffer(m_FBO, GL_NONE);
		glNamedFramebufferReadBuffer(m_FBO, GL_NONE);

		int status = glCheckNamedFramebufferStatus(m_FBO, GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			RXN_CORE_ERROR("Shadow Framebuffer is incomplete!");
		}
	}

	void OpenGLShadowMap::BindWriteLayer(uint32_t layer)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

		glNamedFramebufferTextureLayer(m_FBO, GL_DEPTH_ATTACHMENT, m_DepthMapTexture, 0, layer);

		glViewport(0, 0, m_Size, m_Size);
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLShadowMap::BindRead(uint32_t slot)
	{
		glBindTextureUnit(slot, m_DepthMapTexture);
	}
}
