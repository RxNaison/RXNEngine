#pragma once

#include "RXNEngine/Core/UUID.h"
#include "RXNEngine/Scene/Camera.h"
#include "RXNEngine/Asset/StaticMesh.h"
#include "RXNEngine/Renderer/Light.h"
#include "SceneCamera.h"
#include "RXNEngine/Renderer/GraphicsAPI/VideoTexture.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace RXNEngine {

	struct IDComponent
	{
		UUID ID;

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;

		IDComponent(const UUID& id)
			: ID(id) {
		}
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag) {
		}
	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f }; // in radians
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		glm::mat4 WorldTransform = glm::mat4(1.0f);

		bool IsDirty = false;

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation)
			: Translation(translation) {
		}

		glm::mat4 GetTransform() const
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

			return glm::translate(glm::mat4(1.0f), Translation) * rotation * glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	struct RelationshipComponent
	{
		UUID ParentHandle = 0;
		std::vector<UUID> Children;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent&) = default;
	};

	struct StaticMeshComponent
	{
		std::string AssetPath;
		Ref<StaticMesh> Mesh;
		uint32_t SubmeshIndex = 0;

		Ref<Material> MaterialTableOverride = nullptr;
		std::string MaterialAssetPath = "";

		StaticMeshComponent() = default;
		StaticMeshComponent(const StaticMeshComponent&) = default;
	};

	struct CameraComponent
	{
		SceneCamera Camera;
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	struct DirectionalLightComponent
	{
		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
	};

	struct PointLightComponent
	{
		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float Radius = 10.0f;
		float Falloff = 1.0f;
	};

	struct SpotLightComponent
	{
		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float Radius = 10.0f;
		float Falloff = 1.0f;
		float InnerAngle = 12.5f; // In degrees
		float OuterAngle = 17.5f; // In degrees

		Ref<Texture2D> CookieTexture = nullptr;
		Ref<VideoTexture> CookieVideo = nullptr;
		std::string CookieAssetPath = "";
		bool IsVideo = false;
		float CookieSize = 1.0f;
	};

	class ScriptableEntity;
	struct NativeScriptComponent
	{
		ScriptableEntity* Instance = nullptr;

		ScriptableEntity* (*InstantiateScript)();
		void (*DestroyScript)(NativeScriptComponent*);

		template<typename T>
		void Bind()
		{
			InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}
	};

	struct ScriptComponent
	{
		std::string ClassName;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent&) = default;
		ScriptComponent(const std::string& className) : ClassName(className) {}
	};

	struct RigidbodyComponent
	{
		enum class BodyType { Static = 0, Dynamic, Kinematic };
		BodyType Type = BodyType::Dynamic;

		float Mass = 1.0f;
		float LinearDrag = 0.05f;
		float AngularDrag = 0.05f;

		bool FixedRotation = false;

		bool UseCCD = false;
		float CCDVelocityThreshold = 50.0f;

		void* RuntimeActor = nullptr;

		RigidbodyComponent() = default;
		RigidbodyComponent(const RigidbodyComponent&) = default;
	};

	struct BoxColliderComponent
	{
		glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f };
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };

		float StaticFriction = 0.5f;
		float DynamicFriction = 0.5f;
		float Restitution = 0.1f;

		bool IsTrigger = false;

		void* RuntimeShape = nullptr;
		void* RuntimeMaterial = nullptr;

		BoxColliderComponent() = default;
		BoxColliderComponent(const BoxColliderComponent&) = default;
	};

	struct SphereColliderComponent
	{
		float Radius = 0.5f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };

		float StaticFriction = 0.5f;
		float DynamicFriction = 0.5f;
		float Restitution = 0.1f;

		bool IsTrigger = false;

		void* RuntimeShape = nullptr;
		void* RuntimeMaterial = nullptr;

		SphereColliderComponent() = default;
		SphereColliderComponent(const SphereColliderComponent&) = default;
	};

	struct CapsuleColliderComponent
	{
		float Radius = 0.5f;
		float Height = 1.0f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };

		float StaticFriction = 0.5f;
		float DynamicFriction = 0.5f;
		float Restitution = 0.1f;

		bool IsTrigger = false;

		void* RuntimeShape = nullptr;
		void* RuntimeMaterial = nullptr;

		CapsuleColliderComponent() = default;
		CapsuleColliderComponent(const CapsuleColliderComponent&) = default;
	};

	struct MeshColliderComponent
	{
		bool IsConvex = false;

		std::string OverrideAssetPath = "";

		float StaticFriction = 0.5f;
		float DynamicFriction = 0.5f;
		float Restitution = 0.1f;
		bool IsTrigger = false;

		void* RuntimeShape = nullptr;
		void* RuntimeMaterial = nullptr;

		MeshColliderComponent() = default;
		MeshColliderComponent(const MeshColliderComponent&) = default;
	};

	struct CharacterControllerComponent
	{
		float SlopeLimitDegrees = 45.0f;
		float StepOffset = 0.3f;
		float Radius = 0.5f;
		float Height = 1.0f;

		void* RuntimeController = nullptr;

		CharacterControllerComponent() = default;
		CharacterControllerComponent(const CharacterControllerComponent&) = default;
	};

	template<typename... Component>
	struct ComponentGroup {};

	using AllComponents = ComponentGroup<
		TransformComponent,
		RelationshipComponent,
		StaticMeshComponent,
		CameraComponent,
		DirectionalLightComponent,
		PointLightComponent,
		SpotLightComponent,
		NativeScriptComponent,
		ScriptComponent,
		RigidbodyComponent,
		BoxColliderComponent,
		SphereColliderComponent,
		CapsuleColliderComponent,
		MeshColliderComponent,
		CharacterControllerComponent
	>;
}