#include "rxnpch.h"
#include "OpenGLVertexArray.h"

#include "glad/glad.h"

namespace RXNEngine {

	OpenGLVertexArray::OpenGLVertexArray()
	{
		glCreateVertexArrays(1, &m_RendererID);
	}

	OpenGLVertexArray::~OpenGLVertexArray()
	{
		glDeleteVertexArrays(1, &m_RendererID);
	}

	void OpenGLVertexArray::Bind() const
	{
		glBindVertexArray(m_RendererID);
	}

	void OpenGLVertexArray::Unbind() const
	{
		glBindVertexArray(0);
	}

	void OpenGLVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer)
	{
		RXN_CORE_ASSERT(vertexBuffer->GetLayout().GetElements().size(), "VertexBuffer has no layout!");

		glBindVertexArray(m_RendererID);
		vertexBuffer->Bind();

		const auto& layout = vertexBuffer->GetLayout();

		bool isInstancedBuffer = layout.GetElements().size() > 0 && layout.GetElements()[0].Instanced;
		uint32_t bindingIndex = isInstancedBuffer ? 1 : 0;

		if (isInstancedBuffer)
		{
			m_VertexBufferIndex = 4;
		}

		GLint bufferID;
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bufferID);
		glBindVertexBuffer(bindingIndex, bufferID, 0, layout.GetStride());

		for (const auto& element : layout)
		{
			switch (element.Type)
			{
				case ShaderDataType::Float:
				case ShaderDataType::Float2:
				case ShaderDataType::Float3:
				case ShaderDataType::Float4:
				{
					glEnableVertexAttribArray(m_VertexBufferIndex);

					glVertexAttribFormat(m_VertexBufferIndex,
						element.GetComponentCount(),
						GL_FLOAT,
						element.Normalized ? GL_TRUE : GL_FALSE,
						element.Offset);

					glVertexAttribBinding(m_VertexBufferIndex, bindingIndex);

					if (element.Instanced)
						glVertexBindingDivisor(bindingIndex, 1);

					m_VertexBufferIndex++;
					break;
				}
				case ShaderDataType::Mat3:
				case ShaderDataType::Mat4:
				{
					uint8_t count = element.GetComponentCount();
					for (uint8_t i = 0; i < count; i++)
					{
						glEnableVertexAttribArray(m_VertexBufferIndex);

						glVertexAttribFormat(m_VertexBufferIndex,
							count,
							GL_FLOAT,
							element.Normalized ? GL_TRUE : GL_FALSE,
							element.Offset + sizeof(float) * count * i);

						glVertexAttribBinding(m_VertexBufferIndex, bindingIndex);

						if (element.Instanced)
							glVertexBindingDivisor(bindingIndex, 1);

						m_VertexBufferIndex++;
					}
					break;
				}
				case ShaderDataType::Int:
				case ShaderDataType::Int2:
				case ShaderDataType::Int3:
				case ShaderDataType::Int4:
				case ShaderDataType::Bool:
				{
					glEnableVertexAttribArray(m_VertexBufferIndex);

					glVertexAttribIFormat(m_VertexBufferIndex,
						element.GetComponentCount(),
						GL_INT,
						element.Offset);

					glVertexAttribBinding(m_VertexBufferIndex, bindingIndex);

					if (element.Instanced)
						glVertexBindingDivisor(bindingIndex, 1);

					m_VertexBufferIndex++;
					break;
				}
			}
		}

		m_VertexBuffers.push_back(vertexBuffer);
	}
	void OpenGLVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
	{
		glBindVertexArray(m_RendererID);
		indexBuffer->Bind();

		m_IndexBuffer = indexBuffer;
	}
}