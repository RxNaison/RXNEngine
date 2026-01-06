#include "rxnpch.h"
#include "OpenGLTextureCube.h"
#include "RXNEngine/Renderer/VertexArray.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

namespace RXNEngine {

	static Ref<VertexArray> s_CubeVAO;
	static Ref<VertexArray> s_QuadVAO;

	static void RenderCube()
	{
		if (s_CubeVAO == nullptr)
		{
			float vertices[] = {
				-1.0f,  1.0f, -1.0f,
				-1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,
				 1.0f,  1.0f, -1.0f,
				-1.0f,  1.0f, -1.0f,

				-1.0f, -1.0f,  1.0f,
				-1.0f, -1.0f, -1.0f,
				-1.0f,  1.0f, -1.0f,
				-1.0f,  1.0f, -1.0f,
				-1.0f,  1.0f,  1.0f,
				-1.0f, -1.0f,  1.0f,

				 1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,

				-1.0f, -1.0f,  1.0f,
				-1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f, -1.0f,  1.0f,
				-1.0f, -1.0f,  1.0f,

				-1.0f,  1.0f, -1.0f,
				 1.0f,  1.0f, -1.0f,
				 1.0f,  1.0f,  1.0f,
				 1.0f,  1.0f,  1.0f,
				-1.0f,  1.0f,  1.0f,
				-1.0f,  1.0f, -1.0f,

				-1.0f, -1.0f, -1.0f,
				-1.0f, -1.0f,  1.0f,
				 1.0f, -1.0f, -1.0f,
				 1.0f, -1.0f, -1.0f,
				-1.0f, -1.0f,  1.0f,
				 1.0f, -1.0f,  1.0f
			};

			s_CubeVAO = VertexArray::Create();
			Ref<VertexBuffer> vbo = VertexBuffer::Create(vertices, sizeof(vertices));
			vbo->SetLayout({ { ShaderDataType::Float3, "a_Position" } });
			s_CubeVAO->AddVertexBuffer(vbo);
		}
		s_CubeVAO->Bind();
		glDrawArrays(GL_TRIANGLES, 0, 36);
		s_CubeVAO->Unbind();
	}

	static void RenderQuad()
	{
		if (s_QuadVAO == nullptr)
		{
			float quadVertices[] = {
				-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
				-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
				 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
				 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
			};
			s_QuadVAO = VertexArray::Create();
			Ref<VertexBuffer> vbo = VertexBuffer::Create(quadVertices, sizeof(quadVertices));
			vbo->SetLayout({
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
				});
			s_QuadVAO->AddVertexBuffer(vbo);
		}
		s_QuadVAO->Bind();
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		s_QuadVAO->Unbind();
	}


	OpenGLTextureCube::OpenGLTextureCube(const std::vector<std::string>& paths)
	{
		glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

		int width, height, channels;
		stbi_set_flip_vertically_on_load(0);

		for (uint32_t i = 0; i < paths.size(); i++)
		{
			stbi_uc* data = stbi_load(paths[i].c_str(), &width, &height, &channels, 0);
			if (data)
			{
				m_Specification.Width = width;
				m_Specification.Height = height;

				GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
					0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

				stbi_image_free(data);
			}
			else
			{
				RXN_CORE_ERROR("Failed to load cube map face: {0}", paths[i]);
			}
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		CreateIrradianceMap();
		CreatePrefilterMap();
		CreateBRDFLUT();
	}

	OpenGLTextureCube::OpenGLTextureCube(const std::string& path)
	{
		Ref<Texture2D> hdrTexture = Texture2D::Create(path);

		uint32_t captureFBO, captureRBO;
		glGenFramebuffers(1, &captureFBO);
		glGenRenderbuffers(1, &captureRBO);

		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);

		int res = 1024;
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, res, res);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
		for (unsigned int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, res, res, 0, GL_RGB, GL_FLOAT, nullptr);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		glm::mat4 captureViews[] =
		{
		   glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		   glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		   glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};

		m_EquirectShader = Shader::Create("assets/shaders/equirectangular_to_cubemap.glsl");
		m_EquirectShader->Bind();
		m_EquirectShader->SetInt("u_EquirectangularMap", 0);
		m_EquirectShader->SetMat4("u_Projection", captureProjection);

		hdrTexture->Bind(0);

		glViewport(0, 0, res, res);
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		for (unsigned int i = 0; i < 6; ++i)
		{
			m_EquirectShader->SetMat4("u_View", captureViews[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_RendererID, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			RenderCube();
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &captureFBO);
		glDeleteRenderbuffers(1, &captureRBO);

		CreateIrradianceMap();
		CreatePrefilterMap();
		CreateBRDFLUT();
	}

	OpenGLTextureCube::~OpenGLTextureCube()
	{
		glDeleteTextures(1, &m_RendererID);
		glDeleteTextures(1, &m_IrradianceMapID);
		glDeleteTextures(1, &m_PrefilterMapID);
		glDeleteTextures(1, &m_BRDFLUTMapID);
	}

	void OpenGLTextureCube::Bind(uint32_t slot) const
	{
		glBindTextureUnit(slot, m_RendererID);
	}


	void OpenGLTextureCube::CreateIrradianceMap()
	{
		int res = 32;
		uint32_t captureFBO, captureRBO;
		glGenFramebuffers(1, &captureFBO);
		glGenRenderbuffers(1, &captureRBO);
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, res, res);

		glGenTextures(1, &m_IrradianceMapID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_IrradianceMapID);
		for (unsigned int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, res, res, 0, GL_RGB, GL_FLOAT, nullptr);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		glm::mat4 captureViews[] = {
			glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};

		Ref<Shader> irradianceShader = Shader::Create("assets/shaders/ibl_irradiance.glsl");
		irradianceShader->Bind();
		irradianceShader->SetInt("u_EnvironmentMap", 0);
		irradianceShader->SetMat4("u_Projection", captureProjection);

		glBindTextureUnit(0, m_RendererID);

		glViewport(0, 0, res, res);
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		for (unsigned int i = 0; i < 6; ++i)
		{
			irradianceShader->SetMat4("u_View", captureViews[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_IrradianceMapID, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			RenderCube();
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &captureFBO);
		glDeleteRenderbuffers(1, &captureRBO);
	}

	void OpenGLTextureCube::CreatePrefilterMap()
	{
		int res = 128;
		uint32_t captureFBO, captureRBO;
		glGenFramebuffers(1, &captureFBO);
		glGenRenderbuffers(1, &captureRBO);
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);

		glGenTextures(1, &m_PrefilterMapID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_PrefilterMapID);
		for (unsigned int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, res, res, 0, GL_RGB, GL_FLOAT, nullptr);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 4);

		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

		glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		glm::mat4 captureViews[] = {
			glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};

		Ref<Shader> prefilterShader = Shader::Create("assets/shaders/ibl_prefilter.glsl");
		prefilterShader->Bind();
		prefilterShader->SetInt("u_EnvironmentMap", 0);
		prefilterShader->SetMat4("u_Projection", captureProjection);

		glBindTextureUnit(0, m_RendererID);

		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		unsigned int maxMipLevels = 5;
		for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
		{
			unsigned int mipWidth = res * std::pow(0.5, mip);
			unsigned int mipHeight = res * std::pow(0.5, mip);

			glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
			glViewport(0, 0, mipWidth, mipHeight);

			float roughness = (float)mip / (float)(maxMipLevels - 1);
			prefilterShader->SetFloat("u_Roughness", roughness);

			for (unsigned int i = 0; i < 6; ++i)
			{
				prefilterShader->SetMat4("u_View", captureViews[i]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_PrefilterMapID, mip);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				RenderCube();
			}
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &captureFBO);
		glDeleteRenderbuffers(1, &captureRBO);
	}

	void OpenGLTextureCube::CreateBRDFLUT()
	{
		glGenTextures(1, &m_BRDFLUTMapID);
		glBindTexture(GL_TEXTURE_2D, m_BRDFLUTMapID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		uint32_t captureFBO, captureRBO;
		glGenFramebuffers(1, &captureFBO);
		glGenRenderbuffers(1, &captureRBO);
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BRDFLUTMapID, 0);

		glDisable(GL_BLEND);
		glViewport(0, 0, 512, 512);
		Ref<Shader> brdfShader = Shader::Create("assets/shaders/ibl_brdf.glsl");
		brdfShader->Bind();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		RenderQuad();

		glEnable(GL_BLEND);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &captureFBO);
		glDeleteRenderbuffers(1, &captureRBO);
	}
}