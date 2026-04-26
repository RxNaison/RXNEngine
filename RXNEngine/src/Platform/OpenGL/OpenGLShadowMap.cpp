#include "rxnpch.h"
#include "OpenGLShadowMap.h"

#include <glad/glad.h>

namespace RXNEngine {
	OpenGLShadowMap::~OpenGLShadowMap()
	{
		glDeleteFramebuffers(1, &m_FBO);
		glDeleteTextures(1, &m_DepthMapTexture);
	}

    void OpenGLShadowMap::Init(uint32_t size, uint32_t layers, bool isCubemap)
    {
        m_Size = size;
        m_IsCubemap = isCubemap;

        if (m_IsCubemap)
        {
            glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &m_DepthMapTexture);
            glTextureStorage3D(m_DepthMapTexture, 1, GL_DEPTH_COMPONENT16, m_Size, m_Size, layers * 6);
            glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
        else
        {
            glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_DepthMapTexture);
            glTextureStorage3D(m_DepthMapTexture, 1, GL_DEPTH_COMPONENT16, m_Size, m_Size, layers);
            glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTextureParameteri(m_DepthMapTexture, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            constexpr float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            glTextureParameterfv(m_DepthMapTexture, GL_TEXTURE_BORDER_COLOR, bordercolor);
        }

        glCreateFramebuffers(1, &m_FBO);
        glNamedFramebufferDrawBuffer(m_FBO, GL_NONE);
        glNamedFramebufferReadBuffer(m_FBO, GL_NONE);
    }

    void OpenGLShadowMap::BindWriteLayer(uint32_t layer, uint32_t face)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        uint32_t targetLayer = m_IsCubemap ? (layer * 6 + face) : layer;

        glNamedFramebufferTextureLayer(m_FBO, GL_DEPTH_ATTACHMENT, m_DepthMapTexture, 0, targetLayer);
        glViewport(0, 0, m_Size, m_Size);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

	void OpenGLShadowMap::BindRead(uint32_t slot)
	{
		glBindTextureUnit(slot, m_DepthMapTexture);
	}
}
