#pragma once
#include "RXNEngine/Renderer/GraphicsAPI/VertexArray.h"
#include "RXNEngine/Renderer/GraphicsAPI/Shader.h"
#include "RXNEngine/Renderer/GraphicsAPI/Texture.h"
#include "RXNEngine/Renderer/Font.h"
#include "RXNEngine/Scene/Components.h"


namespace RXNEngine {

	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const glm::mat4& viewProjection);
		static void EndScene();
		static void Flush();

		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const glm::vec4& tintColor = glm::vec4(1.0f));

		struct TextParams
		{
			glm::vec4 Color{ 1.0f };
			float Kerning = 0.0f;
			float LineSpacing = 0.0f;
		};
		static void DrawString(const std::string& string, const glm::mat4& transform, const Ref<Font>& font, const TextParams& textParams = TextParams());

		static void BuildTextGeometry(const std::string& string, const Ref<Font>& font, const TextParams& textParams, std::vector<TextVertexLocal>& outVertices);
		static void DrawTextCached(const std::vector<TextVertexLocal>& vertices, const glm::mat4& transform, const Ref<Font>& font, const TextParams& textParams);

		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;

			uint32_t GetTotalVertexCount() const { return QuadCount * 4; }
			uint32_t GetTotalIndexCount() const { return QuadCount * 6; }
		};
		static void ResetStats();
		static Statistics GetStats();
	private:
		static void StartBatch();
		static void NextBatch();
	};

}
