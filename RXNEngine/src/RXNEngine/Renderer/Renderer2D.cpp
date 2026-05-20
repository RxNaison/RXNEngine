#include "rxnpch.h"
#include "Renderer2D.h"

#include "RXNEngine/Renderer/GraphicsAPI/VertexArray.h"
#include "RXNEngine/Renderer/GraphicsAPI/Shader.h"
#include "RXNEngine/Renderer/RenderCommand.h"
#include "RXNEngine/Renderer/Font.h"
#include <msdf-atlas-gen.h>
#include <FontGeometry.h>
#include <GlyphGeometry.h>

#include <glm/gtc/matrix_transform.hpp>

namespace RXNEngine {

	struct MSDFData
	{
		std::vector<msdf_atlas::GlyphGeometry> Glyphs;
		msdf_atlas::FontGeometry FontGeometry;
	};

	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		float TexIndex;
	};

	struct Renderer2DData
	{
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32;

		Ref<VertexArray> QuadVertexArray;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<Shader> TextureShader;
		Ref<Texture2D> WhiteTexture;

		uint32_t QuadIndexCount = 0;
		QuadVertex* QuadVertexBufferBase = nullptr;
		QuadVertex* QuadVertexBufferPtr = nullptr;

		Ref<VertexArray> TextVertexArray;
		Ref<VertexBuffer> TextVertexBuffer;
		Ref<Shader> TextShader;

		uint32_t TextIndexCount = 0;
		QuadVertex* TextVertexBufferBase = nullptr;
		QuadVertex* TextVertexBufferPtr = nullptr;

		std::array<Ref<Texture2D>, 32> TextureSlots;
		uint32_t TextureSlotIndex = 1; // 0 is white texture

		std::array<Ref<Texture2D>, 32> FontTextureSlots;
		uint32_t FontTextureSlotIndex = 0;

		glm::vec4 QuadVertexPositions[4];

		Renderer2D::Statistics Stats;
	};

	static Renderer2DData s_Data;

	void Renderer2D::Init()
	{
		s_Data.QuadVertexArray = VertexArray::Create();

		s_Data.QuadVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(QuadVertex));
		s_Data.QuadVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color" },
			{ ShaderDataType::Float2, "a_TexCoord" },
			{ ShaderDataType::Float,  "a_TexIndex" }
		});
		s_Data.QuadVertexArray->AddVertexBuffer(s_Data.QuadVertexBuffer);

		s_Data.QuadVertexBufferBase = new QuadVertex[s_Data.MaxVertices];

		uint32_t* quadIndices = new uint32_t[s_Data.MaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < s_Data.MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

		Ref<IndexBuffer> quadIB = IndexBuffer::Create(quadIndices, s_Data.MaxIndices);
		s_Data.QuadVertexArray->SetIndexBuffer(quadIB);
		delete[] quadIndices;

		TextureSpecification spec;
		spec.Width = 1;
		spec.Height = 1;
		s_Data.WhiteTexture = Texture2D::Create(spec);
		uint32_t whiteTextureData = 0xffffffff;
		s_Data.WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));

		int32_t samplers[s_Data.MaxTextureSlots];
		for (uint32_t i = 0; i < s_Data.MaxTextureSlots; i++)
			samplers[i] = i;

		s_Data.TextureShader = Shader::Create("res/shaders/Renderer2D_Quad.glsl");
		s_Data.TextureShader->Bind();
		s_Data.TextureShader->SetIntArray("u_Textures", samplers, s_Data.MaxTextureSlots);

		s_Data.TextureSlots[0] = s_Data.WhiteTexture;

		s_Data.TextVertexArray = VertexArray::Create();

		s_Data.TextVertexBuffer = VertexBuffer::Create(s_Data.MaxVertices * sizeof(QuadVertex));
		s_Data.TextVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color" },
			{ ShaderDataType::Float2, "a_TexCoord" },
			{ ShaderDataType::Float,  "a_TexIndex" }
		});
		s_Data.TextVertexArray->AddVertexBuffer(s_Data.TextVertexBuffer);
		s_Data.TextVertexArray->SetIndexBuffer(quadIB);

		s_Data.TextVertexBufferBase = new QuadVertex[s_Data.MaxVertices];
		
		s_Data.TextShader = Shader::Create("res/shaders/Renderer2D_Text.glsl");
		s_Data.TextShader->Bind();
		s_Data.TextShader->SetIntArray("u_Textures", samplers, s_Data.MaxTextureSlots);

		s_Data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		s_Data.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };
	}

	void Renderer2D::Shutdown()
	{
		delete[] s_Data.QuadVertexBufferBase;
		delete[] s_Data.TextVertexBufferBase;
	}

	void Renderer2D::BeginScene(const glm::mat4& viewProjection)
	{
		s_Data.TextureShader->Bind();
		s_Data.TextureShader->SetMat4("u_ViewProjection", viewProjection);
		
		s_Data.TextShader->Bind();
		s_Data.TextShader->SetMat4("u_ViewProjection", viewProjection);

		StartBatch();
	}

	void Renderer2D::EndScene()
	{
		Flush();
	}

	void Renderer2D::StartBatch()
	{
		s_Data.QuadIndexCount = 0;
		s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase;
		s_Data.TextureSlotIndex = 1;
		
		s_Data.TextIndexCount = 0;
		s_Data.TextVertexBufferPtr = s_Data.TextVertexBufferBase;
		s_Data.FontTextureSlotIndex = 0;
	}

	void Renderer2D::Flush()
	{
		if (s_Data.QuadIndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.QuadVertexBufferPtr - (uint8_t*)s_Data.QuadVertexBufferBase);
			s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBufferBase, dataSize);

			for (uint32_t i = 0; i < s_Data.TextureSlotIndex; i++)
				s_Data.TextureSlots[i]->Bind(i);

			s_Data.TextureShader->Bind();
			RenderCommand::DrawIndexed(s_Data.QuadVertexArray, s_Data.QuadIndexCount);
			s_Data.Stats.DrawCalls++;
		}
		
		if (s_Data.TextIndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.TextVertexBufferPtr - (uint8_t*)s_Data.TextVertexBufferBase);
			s_Data.TextVertexBuffer->SetData(s_Data.TextVertexBufferBase, dataSize);

			s_Data.TextShader->Bind();

			for (uint32_t i = 0; i < s_Data.FontTextureSlotIndex; i++)
			{
				s_Data.FontTextureSlots[i]->Bind(i);
				s_Data.TextShader->SetFloat2("u_TexSizes[" + std::to_string(i) + "]", { s_Data.FontTextureSlots[i]->GetWidth(), s_Data.FontTextureSlots[i]->GetHeight() });
			}

			RenderCommand::DrawIndexed(s_Data.TextVertexArray, s_Data.TextIndexCount);
			s_Data.Stats.DrawCalls++;
		}
	}

	void Renderer2D::NextBatch()
	{
		Flush();
		StartBatch();
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		DrawQuad(transform, color);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		constexpr size_t quadVertexCount = 4;
		const float textureIndex = 0.0f;
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

		if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices)
			NextBatch();

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = color;
			s_Data.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			s_Data.QuadVertexBufferPtr->TexIndex = textureIndex;
			s_Data.QuadVertexBufferPtr++;
		}

		s_Data.QuadIndexCount += 6;
		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const glm::vec4& tintColor)
	{
		constexpr size_t quadVertexCount = 4;
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

		if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices)
			NextBatch();

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++)
		{
			if (s_Data.TextureSlots[i]->GetRendererID() == texture->GetRendererID())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			if (s_Data.TextureSlotIndex >= Renderer2DData::MaxTextureSlots)
				NextBatch();

			textureIndex = (float)s_Data.TextureSlotIndex;
			s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
			s_Data.TextureSlotIndex++;
		}

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = tintColor;
			s_Data.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			s_Data.QuadVertexBufferPtr->TexIndex = textureIndex;
			s_Data.QuadVertexBufferPtr++;
		}

		s_Data.QuadIndexCount += 6;
		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::DrawString(const std::string& string, const glm::mat4& transform, const Ref<Font>& font, const TextParams& textParams)
	{
		// Fallback immediate-mode API
		std::vector<TextVertexLocal> tempGeom;
		BuildTextGeometry(string, font, textParams, tempGeom);
		DrawTextCached(tempGeom, transform, font, textParams);
	}

	void Renderer2D::BuildTextGeometry(const std::string& string, const Ref<Font>& font, const TextParams& textParams, std::vector<TextVertexLocal>& outVertices)
	{
		outVertices.clear();
		if (!font || !font->GetMSDFData() || !font->GetAtlasTexture()) return;

		const auto& fontGeometry = font->GetMSDFData()->FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();
		Ref<Texture2D> fontAtlas = font->GetAtlasTexture();

		double x = 0.0;
		double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
		double y = 0.0;

		float texelWidth = 1.0f / fontAtlas->GetWidth();
		float texelHeight = 1.0f / fontAtlas->GetHeight();

		outVertices.reserve(string.size() * 4);

		for (size_t i = 0; i < string.size(); i++)
		{
			char character = string[i];

			if (character == '\r')
				continue;

			if (character == '\n')
			{
				x = 0;
				y -= fsScale * metrics.lineHeight + textParams.LineSpacing;
				continue;
			}

			auto glyph = fontGeometry.getGlyph(character);

			if (!glyph)
				glyph = fontGeometry.getGlyph('?');

			if (!glyph)
				continue;

			double al, ab, ar, at;
			glyph->getQuadAtlasBounds(al, ab, ar, at);

			glm::vec2 texCoordMin((float)al * texelWidth, (float)ab * texelHeight);
			glm::vec2 texCoordMax((float)ar * texelWidth, (float)at * texelHeight);

			double pl, pb, pr, pt;
			glyph->getQuadPlaneBounds(pl, pb, pr, pt);

			glm::vec2 quadMin((float)pl * fsScale + x, (float)pb * fsScale + y);
			glm::vec2 quadMax((float)pr * fsScale + x, (float)pt * fsScale + y);

			outVertices.push_back({ glm::vec2(quadMin.x, quadMin.y), texCoordMin });
			outVertices.push_back({ glm::vec2(quadMax.x, quadMin.y), glm::vec2(texCoordMax.x, texCoordMin.y) });
			outVertices.push_back({ glm::vec2(quadMax.x, quadMax.y), texCoordMax });
			outVertices.push_back({ glm::vec2(quadMin.x, quadMax.y), glm::vec2(texCoordMin.x, texCoordMax.y) });

			if (i < string.size() - 1)
			{
				double advance = glyph->getAdvance();
				char nextCharacter = string[i + 1];
				fontGeometry.getAdvance(advance, character, nextCharacter);
				x += fsScale * advance + textParams.Kerning;
			}
		}
	}

	void Renderer2D::DrawTextCached(const std::vector<TextVertexLocal>& vertices, const glm::mat4& transform, const Ref<Font>& font, const TextParams& textParams)
	{
		if (vertices.empty() || !font || !font->GetAtlasTexture())
			return;

		Ref<Texture2D> fontAtlas = font->GetAtlasTexture();

		float textureIndex = 0.0f;
		for (uint32_t i = 0; i < s_Data.FontTextureSlotIndex; i++)
		{
			if (s_Data.FontTextureSlots[i]->GetRendererID() == fontAtlas->GetRendererID())
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f && s_Data.FontTextureSlotIndex == 0)
		{
			textureIndex = (float)s_Data.FontTextureSlotIndex;
			s_Data.FontTextureSlots[s_Data.FontTextureSlotIndex++] = fontAtlas;
		}
		else if (textureIndex == 0.0f && s_Data.FontTextureSlots[0]->GetRendererID() != fontAtlas->GetRendererID())
		{
			if (s_Data.FontTextureSlotIndex >= Renderer2DData::MaxTextureSlots)
				NextBatch();

			textureIndex = (float)s_Data.FontTextureSlotIndex;
			s_Data.FontTextureSlots[s_Data.FontTextureSlotIndex++] = fontAtlas;
		}

		for (size_t i = 0; i < vertices.size(); i += 4)
		{
			if (s_Data.TextIndexCount >= Renderer2DData::MaxIndices)
				NextBatch();

			for (size_t j = 0; j < 4; j++)
			{
				s_Data.TextVertexBufferPtr->Position = transform * glm::vec4(vertices[i + j].Position, 0.0f, 1.0f);
				s_Data.TextVertexBufferPtr->Color = textParams.Color;
				s_Data.TextVertexBufferPtr->TexCoord = vertices[i + j].TexCoord;
				s_Data.TextVertexBufferPtr->TexIndex = textureIndex;
				s_Data.TextVertexBufferPtr++;
			}

			s_Data.TextIndexCount += 6;
			s_Data.Stats.QuadCount++;
		}
	}

	void Renderer2D::ResetStats()
	{
		memset(&s_Data.Stats, 0, sizeof(Statistics));
	}

	Renderer2D::Statistics Renderer2D::GetStats()
	{
		return s_Data.Stats;
	}

}
