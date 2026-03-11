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

	static GLenum GLStencilFunc(RendererAPI::StencilFunc func)
	{
		switch (func)
		{
			case RendererAPI::StencilFunc::Always:       return GL_ALWAYS;
			case RendererAPI::StencilFunc::NotEqual:     return GL_NOTEQUAL;
			case RendererAPI::StencilFunc::Equal:        return GL_EQUAL;
			case RendererAPI::StencilFunc::Less:         return GL_LESS;
			case RendererAPI::StencilFunc::LessEqual:    return GL_LEQUAL;
			case RendererAPI::StencilFunc::Greater:      return GL_GREATER;
			case RendererAPI::StencilFunc::GreaterEqual: return GL_GEQUAL;
		}
		return GL_ALWAYS;
	}

	static GLenum GLStencilOp(RendererAPI::StencilOp op)
	{
		switch (op)
		{
			case RendererAPI::StencilOp::Keep:           return GL_KEEP;
			case RendererAPI::StencilOp::Replace:        return GL_REPLACE;
			case RendererAPI::StencilOp::Zero:           return GL_ZERO;
			case RendererAPI::StencilOp::Increment:      return GL_INCR;
			case RendererAPI::StencilOp::IncrementWrap:  return GL_INCR_WRAP;
			case RendererAPI::StencilOp::Decrement:      return GL_DECR;
			case RendererAPI::StencilOp::DecrementWrap:  return GL_DECR_WRAP;
			case RendererAPI::StencilOp::Invert:         return GL_INVERT;
		}
		return GL_KEEP;
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
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
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

	void OpenGLRendererAPI::SetBlend(bool enabled)
	{
		if (enabled)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
	}

	void OpenGLRendererAPI::SetBlendFunc(BlendFactor source, BlendFactor destination)
	{
		auto BlendFactorToGL = [](BlendFactor factor) -> GLenum
			{
				switch (factor)
				{
					case BlendFactor::Zero:             return GL_ZERO;
					case BlendFactor::One:              return GL_ONE;
					case BlendFactor::SrcColor:         return GL_SRC_COLOR;
					case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
					case BlendFactor::DstColor:         return GL_DST_COLOR;
					case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
					case BlendFactor::SrcAlpha:         return GL_SRC_ALPHA;
					case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
					case BlendFactor::DstAlpha:         return GL_DST_ALPHA;
					case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
				}
				return GL_ZERO;
			};

		glBlendFunc(BlendFactorToGL(source), BlendFactorToGL(destination));
	}

	void OpenGLRendererAPI::SetBlendEquation(BlendEquation equation)
	{
		auto BlendEquationToGL = [](BlendEquation eq) -> GLenum
			{
				switch (eq)
				{
					case BlendEquation::Add:             return GL_FUNC_ADD;
					case BlendEquation::Subtract:        return GL_FUNC_SUBTRACT;
					case BlendEquation::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
					case BlendEquation::Min:             return GL_MIN;
					case BlendEquation::Max:             return GL_MAX;
				}
				return GL_FUNC_ADD;
			};

		glBlendEquation(BlendEquationToGL(equation));
	}

	void OpenGLRendererAPI::SetStencilTest(bool enabled)
	{
		if (enabled) glEnable(GL_STENCIL_TEST);
		else glDisable(GL_STENCIL_TEST);
	}

	void OpenGLRendererAPI::SetStencilMask(uint32_t mask)
	{
		glStencilMask(mask);
	}

	void OpenGLRendererAPI::SetStencilFunc(StencilFunc func, int ref, uint32_t mask)
	{
		glStencilFunc(GLStencilFunc(func), ref, mask);
	}

	void OpenGLRendererAPI::SetStencilOp(StencilOp fail, StencilOp zfail, StencilOp zpass)
	{
		glStencilOp(GLStencilOp(fail), GLStencilOp(zfail), GLStencilOp(zpass));
	}

	void OpenGLRendererAPI::SetDepthMask(bool writeEnabled)
	{
		glDepthMask(writeEnabled ? GL_TRUE : GL_FALSE);
	}

	void OpenGLRendererAPI::SetColorMask(bool r, bool g, bool b, bool a)
	{
		glColorMask(r ? GL_TRUE : GL_FALSE,
			g ? GL_TRUE : GL_FALSE,
			b ? GL_TRUE : GL_FALSE,
			a ? GL_TRUE : GL_FALSE);
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

	void OpenGLRendererAPI::DrawIndexedInstanced(const Ref<VertexArray>& vertexArray, const Ref<VertexBuffer>& instanceData, uint32_t instanceCount, uint32_t indexCount, uint32_t baseIndex)
	{
		vertexArray->Bind();
		instanceData->Bind();

		const auto& layout = instanceData->GetLayout();

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
				case ShaderDataType::Int:
				case ShaderDataType::Int2:
				case ShaderDataType::Int3:
				case ShaderDataType::Int4:
				case ShaderDataType::Bool:
				{
					glEnableVertexAttribArray(attribIndex);
					glVertexAttribIPointer(attribIndex, element.GetComponentCount(), GL_INT,
						layout.GetStride(), (const void*)element.Offset);

					glVertexAttribDivisor(attribIndex, 1);
					attribIndex++;
					break;
				}
			}
		}

		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		const void* offset = (const void*)(sizeof(uint32_t) * baseIndex);

		glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_INT, offset, instanceCount);
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


