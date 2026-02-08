#include "rxnpch.h"
#include "OpenGLRendererAPI.h"

#include "glad/glad.h"

namespace RXNEngine {

	static GLenum GLDepthFunc(RendererAPI::DepthFunc func)
	{
		switch (func)
		{
			case RendererAPI::DepthFunc::Less:			return GL_LESS;
			case RendererAPI::DepthFunc::LessEqual:		return GL_LEQUAL;
			case RendererAPI::DepthFunc::Equal:			return GL_EQUAL;
			case RendererAPI::DepthFunc::Greater:		return GL_GREATER;
			case RendererAPI::DepthFunc::GreaterEqual:	return GL_GEQUAL;
			case RendererAPI::DepthFunc::NotEqual:		return GL_NOTEQUAL;
			case RendererAPI::DepthFunc::Always:		return GL_ALWAYS;
			case RendererAPI::DepthFunc::Never:			return GL_NEVER;
		}
		RXN_CORE_ASSERT(false, "Unknown DepthFunc!");
		return GL_LESS;
	}

	static GLenum GLCullFace(RendererAPI::CullFace face)
	{
		switch (face)
		{
			case RendererAPI::CullFace::Back:			return GL_BACK;
			case RendererAPI::CullFace::Front:			return GL_FRONT;
			case RendererAPI::CullFace::FrontAndBack:	return GL_FRONT_AND_BACK;
		}
		RXN_CORE_ASSERT(false, "Unknown CullFace!");
		return GL_BACK;
	}

	void OpenGLRendererAPI::Init()
	{
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_STENCIL_TEST);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		glEnable(GL_CULL_FACE);
	}

	void OpenGLRendererAPI::BindDefaultRenderTarget()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		glViewport(x, y, width, height);
	}

	void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
	{
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLRendererAPI::Clear()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLRendererAPI::SetDepthTest(bool enabled)
	{
		if(enabled)
			glEnable(GL_DEPTH_TEST);
		else 
			glDisable(GL_DEPTH_TEST);
	}

	void OpenGLRendererAPI::SetDepthFunc(DepthFunc func)
	{
		glDepthFunc(GLDepthFunc(func));
	}

	void OpenGLRendererAPI::SetCullFace(CullFace face)
	{
		if (face == CullFace::None)
		{
			glDisable(GL_CULL_FACE);
		}
		else
		{
			glEnable(GL_CULL_FACE);
			glCullFace(GLCullFace(face));
		}
	}

	void OpenGLRendererAPI::BindTextureID(uint32_t slot, uint32_t textureID)
	{
		glBindTextureUnit(slot, textureID);
	}

	void OpenGLRendererAPI::Draw(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
	{
		vertexArray->Bind();
		glDrawArrays(GL_TRIANGLES, 0, vertexCount);
	}

	void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount)
	{
		vertexArray->Bind();
		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
	}

    void OpenGLRendererAPI::DrawIndexedInstanced(const Ref<VertexArray>& vertexArray, const Ref<VertexBuffer>& instanceData, uint32_t instanceCount)
    {
        vertexArray->Bind();
        instanceData->Bind();

        const auto& layout = instanceData->GetLayout();

        // calculate this offset automatically. 
        uint32_t attribIndex = 4;

        for (const auto& element : layout)
        {
            switch (element.Type)
            {
				case ShaderDataType::Float4:
				case ShaderDataType::Float3:
				case ShaderDataType::Float2:
				case ShaderDataType::Float:
				{
					glEnableVertexAttribArray(attribIndex);
					glVertexAttribPointer(attribIndex, element.GetComponentCount(), GL_FLOAT,
						element.Normalized ? GL_TRUE : GL_FALSE, layout.GetStride(), (const void*)element.Offset);

					glVertexAttribDivisor(attribIndex, 1);
					attribIndex++;
					break;
				}
            // Handle Mat4/Mat3 if your layout used them directly (splitting into columns)
            // ...
            }
        }

        uint32_t count = vertexArray->GetIndexBuffer()->GetCount();
        glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr, instanceCount);
    }

	void OpenGLRendererAPI::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
	{
		vertexArray->Bind();
		glDrawArrays(GL_LINES, 0, vertexCount);
	}


	void OpenGLRendererAPI::SetLineWidth(float width)
	{
		glLineWidth(width);
	}

}


