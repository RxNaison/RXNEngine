#pragma once

#include "RXNEngine/Core/UUID.h"
#include "RXNEngine/Scene/Camera.h"
#include "RXNEngine/Asset/StaticMesh.h"
#include "RXNEngine/Renderer/Light.h"
#include "SceneCamera.h"
#include "RXNEngine/Renderer/GraphicsAPI/VideoTexture.h"
#include "RXNEngine/Asset/PhysicsMaterial.h"
#include "RXNEngine/Renderer/Font.h"
#include "RXNEngine/Asset/SkeletalMesh.h"
#include "RXNEngine/Asset/AnimationClip.h"
#include "RXNEngine/Asset/Inertialization.h"
#include "RXNEngine/Animation/AnimGraph.h"
#include "RXNEngine/Renderer/GraphicsAPI/Buffer.h"
#include "RXNEngine/Renderer/GraphicsAPI/VertexArray.h"
#include "RXNEngine/Renderer/GraphicsAPI/UniformBuffer.h"

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

		bool CastsShadows = true;

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
		bool CastsShadows = true;
		uint32_t ShadowResolution = 2048;
	};

	struct PointLightComponent
	{
		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float Radius = 10.0f;
		float Falloff = 1.0f;

		bool CastsShadows = false;
		uint32_t ShadowResolution = 1024;

		bool IsShadowCacheValid = false;
		glm::vec3 LastCachedPosition = { 0.0f, 0.0f, 0.0f };
		int LastShadowLayer = -1;
		float LastCachedRadius = -1.0f; // WP20: cube-map far plane of the last bake
	};

	struct SpotLightComponent
	{
		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float Radius = 10.0f;
		float Falloff = 1.0f;
		float InnerAngle = 12.5f; // in degrees
		float OuterAngle = 17.5f; // in degrees

		Ref<Texture2D> CookieTexture = nullptr;
		Ref<VideoTexture> CookieVideo = nullptr;
		std::string CookieAssetPath = "";
		bool IsVideo = false;
		float CookieSize = 1.0f;

		bool CastsShadows = false;
		uint32_t ShadowResolution = 1024;

		bool IsShadowCacheValid = false;
		glm::vec3 LastCachedPosition = { 0.0f, 0.0f, 0.0f };
		int LastShadowLayer = -1;
		// WP20: full light-space matrix of the last bake. It encodes position,
		// direction, cone angle AND range, so ANY change invalidates the cache
		// (a flashlight mostly rotates, which position tracking alone misses).
		glm::mat4 LastCachedMatrix = glm::mat4(0.0f);
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

	struct ScriptFieldInstance
	{
		uint32_t Type = 0; // Maps to ScriptFieldType enum
		std::array<uint8_t, 16> Data = { 0 };
		std::string StringValue;
	};

	struct ScriptComponent
	{
		std::string ClassName;
		std::unordered_map<std::string, ScriptFieldInstance> FieldInstances;

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
		glm::vec3 HalfExtents = { 1.0f, 1.0f, 1.0f };
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		bool IsTrigger = false;
		bool IsAmbientZone = false;
		float AmbientIntensity = 1.0f;
		glm::vec3 TransitionMin = { 0.0f, 0.0f, 0.0f };
		glm::vec3 TransitionMax = { 0.0f, 0.0f, 0.0f };

		Ref<PhysicsMaterial> PhysicsMaterialAsset;
		std::string PhysicsMaterialPath = "";

		void* RuntimeShape = nullptr;
		void* RuntimeMaterial = nullptr;

		BoxColliderComponent() = default;
		BoxColliderComponent(const BoxColliderComponent&) = default;
	};

	struct SphereColliderComponent
	{
		float Radius = 0.5f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		bool IsTrigger = false;

		Ref<PhysicsMaterial> PhysicsMaterialAsset;
		std::string PhysicsMaterialPath = "";

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
		bool IsTrigger = false;

		Ref<PhysicsMaterial> PhysicsMaterialAsset;
		std::string PhysicsMaterialPath = "";

		void* RuntimeShape = nullptr;
		void* RuntimeMaterial = nullptr;

		CapsuleColliderComponent() = default;
		CapsuleColliderComponent(const CapsuleColliderComponent&) = default;
	};

	struct MeshColliderComponent
	{
		bool IsConvex = false;

		std::string OverrideAssetPath = "";

		Ref<PhysicsMaterial> PhysicsMaterialAsset;
		std::string PhysicsMaterialPath = "";

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

	struct AudioSourceComponent
	{
		std::string AudioClipPath = "";
		bool PlayOnAwake = true;
		bool Looping = false;
		float Volume = 1.0f;
		float MinDistance = 1.0f;
		float MaxDistance = 50.0f;

		void* RuntimeSound = nullptr;
		bool IsPlaying = false;

		// AAA Upgrades: Velocity & Occlusion Tracking
		glm::vec3 LastPosition = { 0.0f, 0.0f, 0.0f };
		bool HasLastPosition = false;
		float CurrentOcclusion = 0.0f;

		AudioSourceComponent() = default;
		AudioSourceComponent(const AudioSourceComponent&) = default;
	};

	enum class ReverbPreset {
		Off = 0, Generic, PaddedCell, Room, Bathroom, LivingRoom, StoneRoom, 
		Auditorium, ConcertHall, Cave, Hangar, CarpettedHallway, Hallway, 
		StoneCorridor, Alley, Forest, City, Mountains, Quarry
	};

	struct AudioReverbZoneComponent
	{
		ReverbPreset Preset = ReverbPreset::Generic;
		float MinDistance = 1.0f;
		float MaxDistance = 10.0f;
		bool IsBox = false;
		glm::vec3 BoxDimensions = { 10.0f, 10.0f, 10.0f };

		AudioReverbZoneComponent() = default;
		AudioReverbZoneComponent(const AudioReverbZoneComponent&) = default;
	};

	struct AudioPortalComponent
	{
		bool Active = true;
		glm::vec2 Dimensions = { 1.5f, 2.2f };

		AudioPortalComponent() = default;
		AudioPortalComponent(const AudioPortalComponent&) = default;
	};

	enum class CanvasRenderMode { ScreenSpaceOverlay = 0, WorldSpace = 1 };

	struct UICanvasComponent
	{
		bool Active = true;
		CanvasRenderMode RenderMode = CanvasRenderMode::ScreenSpaceOverlay;
		glm::vec2 ReferenceResolution = { 1920.0f, 1080.0f };
		
		UICanvasComponent() = default;
		UICanvasComponent(const UICanvasComponent&) = default;
	};

	struct UITransformComponent
	{
		glm::vec2 AnchorMin = { 0.5f, 0.5f };
		glm::vec2 AnchorMax = { 0.5f, 0.5f };
		glm::vec2 OffsetMin = { -50.0f, -50.0f };
		glm::vec2 OffsetMax = { 50.0f, 50.0f };

		int ZIndex = 0;
		bool IsDirty = true;

		glm::vec2 ComputedSize = { 0.0f, 0.0f };
		glm::vec2 ComputedBoundsMin = { 0.0f, 0.0f };
		glm::vec2 ComputedBoundsMax = { 0.0f, 0.0f };
		glm::mat4 ComputedTransform = glm::mat4(1.0f);

		UITransformComponent() = default;
		UITransformComponent(const UITransformComponent&) = default;
	};

	struct UIImageComponent
	{
		glm::vec4 TintColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		Ref<Texture2D> Texture;
		std::string TextureAssetPath = "";

		UIImageComponent() = default;
		UIImageComponent(const UIImageComponent&) = default;
	};

	struct TextVertexLocal
	{ 
		glm::vec2 Position;
		glm::vec2 TexCoord; 
	};

	struct UITextComponent
	{
		std::string Text = "Text";
		Ref<Font> FontAsset;
		glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		
		float FontSize = 48.0f;
		float LineSpacing = 0.0f;
		float Kerning = 0.0f;

		size_t TextHash = 0;
		std::vector<TextVertexLocal> CachedGeometry;

		UITextComponent() = default;
		UITextComponent(const UITextComponent&) = default;
	};

	enum class ButtonState { Normal = 0, Hovered, Pressed };

	struct UIButtonComponent
	{
		glm::vec4 NormalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec4 HoverColor = { 0.8f, 0.8f, 0.8f, 1.0f };
		glm::vec4 PressedColor = { 0.6f, 0.6f, 0.6f, 1.0f };

		ButtonState State = ButtonState::Normal;

		UIButtonComponent() = default;
		UIButtonComponent(const UIButtonComponent&) = default;
	};

	struct AnimatorComponent
	{
		Ref<SkeletalMesh> MeshAsset = nullptr;
		std::string MeshAssetPath = "";
		std::string AnimationPath = "";
		bool ApplyRootMotion = false;
		float PrevPlaybackTime = -1.0f;
		bool RootMotionInitialized = false;

		SkeletalPose CurrentPose;
		Ref<AnimationClip> CurrentClip = nullptr;
		std::string CurrentClipName = "";
		std::string CurrentClipPath = "";
		float PlaybackTime = 0.0f;
		float PlaybackSpeed = 1.0f;
		bool Looping = true;

		InertializationDelta DeltaSolver;
		std::vector<glm::vec3> JointVelocities;

		Ref<VertexBuffer> SkinnedVBOs[2] = { nullptr, nullptr };
		Ref<VertexArray> SkinnedVAO = nullptr;
		Ref<UniformBuffer> BoneUniformBuffer = nullptr;
		uint32_t CurrentBufferIdx = 0;

		AnimGraphInstance GraphInstance;
		std::vector<AnimInstruction> CompiledInstructions;
		AnimGraphContext GraphContext;
		FeatureVector TargetQueryFeatures;
		FeatureWeights SearchWeights;
		uint64_t ActiveTagBitmask = 0;

		AnimatorComponent() = default;
		AnimatorComponent(const AnimatorComponent&) = default;

		void PlayAnimation(Ref<AnimationClip> clip, float halfLife = 0.15f)
		{
			if (!clip)
				return;

			if (!MeshAsset)
			{
				CurrentClip = clip;
				CurrentClipName = clip->GetName();
				PlaybackTime = 0.0f;
				RootMotionInitialized = false;
				PrevPlaybackTime = -1.0f;
				return;
			}

			if (CurrentClip && CurrentPose.JointCount > 0)
			{
				if (JointVelocities.size() != CurrentPose.JointCount)
				{
					JointVelocities.resize(CurrentPose.JointCount, glm::vec3(0.0f));
				}

				std::vector<glm::vec3> newVelocities(CurrentPose.JointCount, glm::vec3(0.0f));
				SkeletalPose newPose;
				newPose.Resize(CurrentPose.JointCount);
				for (uint32_t j = 0; j < CurrentPose.JointCount; ++j)
				{
					clip->SampleTrackForSkeleton(MeshAsset->GetSkeleton().get(), j, 0.0f, newPose.LocalTranslations[j], newPose.LocalRotations[j], newPose.LocalScales[j]);
				}

				DeltaSolver.HalfLife = halfLife;
				DeltaSolver.Initialize(CurrentPose, newPose, JointVelocities, newVelocities);
			}

			CurrentClip = clip;
			CurrentClipName = clip->GetName();
			PlaybackTime = 0.0f;
			RootMotionInitialized = false;
			PrevPlaybackTime = -1.0f;
		}
	};

	struct FoliageComponent //TODO
	{
		std::string ImpostorAssetPath = "";
		float ImpostorSize = 1.0f;

		FoliageComponent() = default;
		FoliageComponent(const FoliageComponent&) = default;
	};

	struct ReflectionProbeComponent
	{
		bool Active = true;
		glm::vec3 BoxHalfExtents = { 5.0f, 5.0f, 5.0f };
		float BlendDistance = 1.0f;
		float Intensity = 1.0f;

		// Capture (R1 Step 2 - hybrid baked + on-demand).
		int Resolution = 256;
		bool Baked = true;
		bool Dirty = true;
		bool Captured = false; // runtime: set once this probe's cubemap has been captured
		glm::vec3 LastCapturePosition = glm::vec3(0.0f); // runtime: world position used for the last capture (auto re-capture when the probe moves)
		uint32_t CaptureFaceIndex = 0; // runtime (WP13): time-sliced capture cursor, one cubemap face per frame

		ReflectionProbeComponent() = default;
		ReflectionProbeComponent(const ReflectionProbeComponent&) = default;
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
		CharacterControllerComponent,
		AudioSourceComponent,
		AudioReverbZoneComponent,
		AudioPortalComponent,
		UICanvasComponent,
		UITransformComponent,
		UIImageComponent,
		UITextComponent,
		UIButtonComponent,
		AnimatorComponent,
		FoliageComponent,
		ReflectionProbeComponent
	>;
}