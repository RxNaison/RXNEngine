#include "rxnpch.h"
#include "SceneSerializer.h"

#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Scene/Components.h"
#include "RXNEngine/Core/UUID.h"
#include "RXNEngine/Asset/AssetManager.h"
#include "RXNEngine/Serialization/YamlHelpers.h"
#include "RXNEngine/Scripting/ScriptEngine.h"
#include "RXNEngine/Utils/PlatformUtils.h"
#include "RXNEngine/Core/VFSSystem.h"

#include <fstream>

namespace RXNEngine {

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_Scene(scene)
	{
	}

	void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		RXN_CORE_ASSERT(entity.HasComponent<IDComponent>());

		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

		if (entity.HasComponent<TagComponent>())
		{
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap; // TagComponent

			auto& tag = entity.GetComponent<TagComponent>().Tag;
			out << YAML::Key << "Tag" << YAML::Value << tag;

			out << YAML::EndMap; // TagComponent
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent

			auto& tc = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Translation" << YAML::Value << tc.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << tc.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << tc.Scale;

			out << YAML::EndMap; // TransformComponent
		}

		if (entity.HasComponent<RelationshipComponent>())
		{
			out << YAML::Key << "RelationshipComponent";
			out << YAML::BeginMap; // RelationshipComponent

			auto& rc = entity.GetComponent<RelationshipComponent>();
			out << YAML::Key << "ParentHandle" << YAML::Value << rc.ParentHandle;

			out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
			for (auto child : rc.Children)
			{
				out << child;
			}
			out << YAML::EndSeq;

			out << YAML::EndMap; // RelationshipComponent
		}

		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap; // CameraComponent

			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			auto& camera = cameraComponent.Camera;

			out << YAML::Key << "Camera" << YAML::Value;
			out << YAML::BeginMap; // Camera
			out << YAML::Key << "ProjectionType" << YAML::Value << (int)camera.GetProjectionType();
			out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.GetPerspectiveFOV();
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::EndMap; // Camera

			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cameraComponent.FixedAspectRatio;

			out << YAML::EndMap; // CameraComponent
		}

		if (entity.HasComponent<StaticMeshComponent>())
		{
			out << YAML::Key << "StaticMeshComponent";
			out << YAML::BeginMap;

			auto& mc = entity.GetComponent<StaticMeshComponent>();

			out << YAML::Key << "AssetPath" << YAML::Value << FileSystem::GetRelativePath(mc.AssetPath);
			out << YAML::Key << "SubmeshIndex" << YAML::Value << mc.SubmeshIndex;
			out << YAML::Key << "MaterialAssetPath" << YAML::Value << FileSystem::GetRelativePath(mc.MaterialAssetPath);
			out << YAML::Key << "CastsShadows" << YAML::Value << mc.CastsShadows;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap; // DirectionalLightComponent

			auto& dl = entity.GetComponent<DirectionalLightComponent>();
			out << YAML::Key << "Color" << YAML::Value << dl.Color;
			out << YAML::Key << "Intensity" << YAML::Value << dl.Intensity;

			out << YAML::EndMap; // DirectionalLightComponent
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap; // PointLightComponent

			auto& pl = entity.GetComponent<PointLightComponent>();
			out << YAML::Key << "Color" << YAML::Value << pl.Color;
			out << YAML::Key << "Intensity" << YAML::Value << pl.Intensity;
			out << YAML::Key << "Radius" << YAML::Value << pl.Radius;
			out << YAML::Key << "Falloff" << YAML::Value << pl.Falloff;
			out << YAML::Key << "CastsShadows" << YAML::Value << pl.CastsShadows;

			out << YAML::EndMap; // PointLightComponent
		}

		if (entity.HasComponent<SpotLightComponent>())
		{
			out << YAML::Key << "SpotLightComponent";
			out << YAML::BeginMap;

			auto& sl = entity.GetComponent<SpotLightComponent>();
			out << YAML::Key << "Color" << YAML::Value << sl.Color;
			out << YAML::Key << "Intensity" << YAML::Value << sl.Intensity;
			out << YAML::Key << "Radius" << YAML::Value << sl.Radius;
			out << YAML::Key << "Falloff" << YAML::Value << sl.Falloff;
			out << YAML::Key << "InnerAngle" << YAML::Value << sl.InnerAngle;
			out << YAML::Key << "OuterAngle" << YAML::Value << sl.OuterAngle;
			out << YAML::Key << "CastsShadows" << YAML::Value << sl.CastsShadows;

			out << YAML::Key << "CookieAssetPath" << YAML::Value << sl.CookieAssetPath;
			out << YAML::Key << "IsVideo" << YAML::Value << sl.IsVideo;
			out << YAML::Key << "CookieSize" << YAML::Value << sl.CookieSize;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<RigidbodyComponent>())
		{
			out << YAML::Key << "RigidbodyComponent";
			out << YAML::BeginMap; // RigidbodyComponent

			auto& rb = entity.GetComponent<RigidbodyComponent>();
			out << YAML::Key << "Type" << YAML::Value << (int)rb.Type;
			out << YAML::Key << "Mass" << YAML::Value << rb.Mass;
			out << YAML::Key << "LinearDrag" << YAML::Value << rb.LinearDrag;
			out << YAML::Key << "AngularDrag" << YAML::Value << rb.AngularDrag;

			out << YAML::Key << "FixedRotation" << YAML::Value << rb.FixedRotation;

			out << YAML::Key << "UseCCD" << YAML::Value << rb.UseCCD;
			out << YAML::Key << "CCDVelocityThreshold" << YAML::Value << rb.CCDVelocityThreshold;

			out << YAML::EndMap; // RigidbodyComponent
		}

		if (entity.HasComponent<BoxColliderComponent>())
		{
			out << YAML::Key << "BoxColliderComponent";
			out << YAML::BeginMap; // BoxColliderComponent

			auto& bc = entity.GetComponent<BoxColliderComponent>();
			out << YAML::Key << "HalfExtents" << YAML::Value << bc.HalfExtents;
			out << YAML::Key << "Offset" << YAML::Value << bc.Offset;
			out << YAML::Key << "IsTrigger" << YAML::Value << bc.IsTrigger;
			out << YAML::Key << "IsAmbientZone" << YAML::Value << bc.IsAmbientZone;
			out << YAML::Key << "AmbientIntensity" << YAML::Value << bc.AmbientIntensity;
			out << YAML::Key << "TransitionMin" << YAML::Value << bc.TransitionMin;
			out << YAML::Key << "TransitionMax" << YAML::Value << bc.TransitionMax;
			out << YAML::Key << "PhysicsMaterialPath" << YAML::Value << FileSystem::GetRelativePath(bc.PhysicsMaterialPath);

			out << YAML::EndMap; // BoxColliderComponent
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			out << YAML::Key << "SphereColliderComponent";
			out << YAML::BeginMap; // SphereColliderComponent

			auto& sc = entity.GetComponent<SphereColliderComponent>();
			out << YAML::Key << "Radius" << YAML::Value << sc.Radius;
			out << YAML::Key << "Offset" << YAML::Value << sc.Offset;
			out << YAML::Key << "IsTrigger" << YAML::Value << sc.IsTrigger;
			out << YAML::Key << "PhysicsMaterialPath" << YAML::Value << FileSystem::GetRelativePath(sc.PhysicsMaterialPath);

			out << YAML::EndMap; // SphereColliderComponent
		}

		if (entity.HasComponent<CapsuleColliderComponent>())
		{
			out << YAML::Key << "CapsuleColliderComponent";
			out << YAML::BeginMap; // CapsuleColliderComponent

			auto& cc = entity.GetComponent<CapsuleColliderComponent>();
			out << YAML::Key << "Radius" << YAML::Value << cc.Radius;
			out << YAML::Key << "Height" << YAML::Value << cc.Height;
			out << YAML::Key << "Offset" << YAML::Value << cc.Offset;
			out << YAML::Key << "IsTrigger" << YAML::Value << cc.IsTrigger;
			out << YAML::Key << "PhysicsMaterialPath" << YAML::Value << FileSystem::GetRelativePath(cc.PhysicsMaterialPath);

			out << YAML::EndMap; // CapsuleColliderComponent
		}

		if (entity.HasComponent<MeshColliderComponent>())
		{
			out << YAML::Key << "MeshColliderComponent";
			out << YAML::BeginMap; // MeshColliderComponent

			auto& mc = entity.GetComponent<MeshColliderComponent>();
			out << YAML::Key << "IsConvex" << YAML::Value << mc.IsConvex;
			out << YAML::Key << "OverrideAssetPath" << YAML::Value << FileSystem::GetRelativePath(mc.OverrideAssetPath);
			out << YAML::Key << "IsTrigger" << YAML::Value << mc.IsTrigger;
			out << YAML::Key << "PhysicsMaterialPath" << YAML::Value << FileSystem::GetRelativePath(mc.PhysicsMaterialPath);

			out << YAML::EndMap; // MeshColliderComponent
		}

		if (entity.HasComponent<CharacterControllerComponent>())
		{
			out << YAML::Key << "CharacterControllerComponent";
			out << YAML::BeginMap;

			auto& cct = entity.GetComponent<CharacterControllerComponent>();
			out << YAML::Key << "SlopeLimitDegrees" << YAML::Value << cct.SlopeLimitDegrees;
			out << YAML::Key << "StepOffset" << YAML::Value << cct.StepOffset;
			out << YAML::Key << "Radius" << YAML::Value << cct.Radius;
			out << YAML::Key << "Height" << YAML::Value << cct.Height;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap; // ScriptComponent

			auto& sc = entity.GetComponent<ScriptComponent>();
			out << YAML::Key << "ClassName" << YAML::Value << sc.ClassName;

			out << YAML::Key << "ScriptFields" << YAML::Value;
			out << YAML::BeginMap; // ScriptFields
			for (const auto& [name, field] : sc.FieldInstances)
			{
				out << YAML::Key << name;
				out << YAML::BeginMap;
				out << YAML::Key << "Type" << YAML::Value << field.Type;

				out << YAML::Key << "Data" << YAML::Value;
				switch ((ScriptFieldType)field.Type)
				{
					case ScriptFieldType::Float: out << *(float*)field.Data.data(); break;
					case ScriptFieldType::Bool: out << *(bool*)field.Data.data(); break;
					case ScriptFieldType::Int: out << *(int*)field.Data.data(); break;
					case ScriptFieldType::Vector2: out << *(glm::vec2*)field.Data.data(); break;
					case ScriptFieldType::Vector3: out << *(glm::vec3*)field.Data.data(); break;
					case ScriptFieldType::Vector4: out << *(glm::vec4*)field.Data.data(); break;
					case ScriptFieldType::Entity: out << *(uint64_t*)field.Data.data(); break;
				}
				out << YAML::EndMap;
			}
			out << YAML::EndMap; // ScriptFields

			out << YAML::EndMap; // ScriptComponent
		}

		if (entity.HasComponent<AudioSourceComponent>())
		{
			out << YAML::Key << "AudioSourceComponent";
			out << YAML::BeginMap;

			auto& ac = entity.GetComponent<AudioSourceComponent>();
			out << YAML::Key << "AudioClipPath" << YAML::Value << FileSystem::GetRelativePath(ac.AudioClipPath);
			out << YAML::Key << "PlayOnAwake" << YAML::Value << ac.PlayOnAwake;
			out << YAML::Key << "Looping" << YAML::Value << ac.Looping;
			out << YAML::Key << "Volume" << YAML::Value << ac.Volume;
			out << YAML::Key << "MinDistance" << YAML::Value << ac.MinDistance;
			out << YAML::Key << "MaxDistance" << YAML::Value << ac.MaxDistance;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<UICanvasComponent>())
		{
			out << YAML::Key << "UICanvasComponent";
			out << YAML::BeginMap;

			auto& uc = entity.GetComponent<UICanvasComponent>();
			out << YAML::Key << "Active" << YAML::Value << uc.Active;
			out << YAML::Key << "RenderMode" << YAML::Value << (int)uc.RenderMode;
			out << YAML::Key << "ReferenceResolution" << YAML::Value << uc.ReferenceResolution;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<UITransformComponent>())
		{
			out << YAML::Key << "UITransformComponent";
			out << YAML::BeginMap;

			auto& ut = entity.GetComponent<UITransformComponent>();
			out << YAML::Key << "AnchorMin" << YAML::Value << ut.AnchorMin;
			out << YAML::Key << "AnchorMax" << YAML::Value << ut.AnchorMax;
			out << YAML::Key << "OffsetMin" << YAML::Value << ut.OffsetMin;
			out << YAML::Key << "OffsetMax" << YAML::Value << ut.OffsetMax;
			out << YAML::Key << "ZIndex" << YAML::Value << ut.ZIndex;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<UIImageComponent>())
		{
			out << YAML::Key << "UIImageComponent";
			out << YAML::BeginMap;

			auto& ui = entity.GetComponent<UIImageComponent>();
			out << YAML::Key << "TintColor" << YAML::Value << ui.TintColor;
			out << YAML::Key << "TextureAssetPath" << YAML::Value << FileSystem::GetRelativePath(ui.TextureAssetPath);

			out << YAML::EndMap;
		}

		if (entity.HasComponent<UITextComponent>())
		{
			out << YAML::Key << "UITextComponent";
			out << YAML::BeginMap;

			auto& ut = entity.GetComponent<UITextComponent>();
			out << YAML::Key << "Text" << YAML::Value << ut.Text;
			out << YAML::Key << "FontAsset" << YAML::Value << FileSystem::GetRelativePath(ut.FontAsset->GetPath().string());
			out << YAML::Key << "Color" << YAML::Value << ut.Color;
			out << YAML::Key << "FontSize" << YAML::Value << ut.FontSize;
			out << YAML::Key << "LineSpacing" << YAML::Value << ut.LineSpacing;
			out << YAML::Key << "Kerning" << YAML::Value << ut.Kerning;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<UIButtonComponent>())
		{
			out << YAML::Key << "UIButtonComponent";
			out << YAML::BeginMap;

			auto& ub = entity.GetComponent<UIButtonComponent>();
			out << YAML::Key << "NormalColor" << YAML::Value << ub.NormalColor;
			out << YAML::Key << "HoverColor" << YAML::Value << ub.HoverColor;
			out << YAML::Key << "PressedColor" << YAML::Value << ub.PressedColor;

			out << YAML::EndMap;
		}

		out << YAML::EndMap; // Entity
	}

	void SceneSerializer::DeserializeComponentsToEntity(YAML::Node& entity, Entity deserializedEntity)
	{
		auto transformComponent = entity["TransformComponent"];
		if (transformComponent)
		{
			auto& tc = deserializedEntity.GetComponent<TransformComponent>();

			if (transformComponent["Translation"])
				tc.Translation = transformComponent["Translation"].as<glm::vec3>();

			if (transformComponent["Rotation"])
				tc.Rotation = transformComponent["Rotation"].as<glm::vec3>();

			if (transformComponent["Scale"])
				tc.Scale = transformComponent["Scale"].as<glm::vec3>();
		}

		auto relationshipComponent = entity["RelationshipComponent"];
		if (relationshipComponent)
		{
			auto& rc = deserializedEntity.GetComponent<RelationshipComponent>();

			if (relationshipComponent["ParentHandle"])
				rc.ParentHandle = relationshipComponent["ParentHandle"].as<uint64_t>();

			if (auto children = relationshipComponent["Children"])
			{
				for (auto child : children)
					rc.Children.push_back(child.as<uint64_t>());
			}
		}

		auto cameraComponent = entity["CameraComponent"];
		if (cameraComponent)
		{
			auto& cc = deserializedEntity.AddComponent<CameraComponent>();

			if (auto cameraProps = cameraComponent["Camera"])
			{
				if (cameraProps["ProjectionType"])
					cc.Camera.SetProjectionType((SceneCamera::ProjectionMode)cameraProps["ProjectionType"].as<int>());
				if (cameraProps["PerspectiveFOV"])
					cc.Camera.SetPerspectiveFOV(cameraProps["PerspectiveFOV"].as<float>());
				if (cameraProps["PerspectiveNear"])
					cc.Camera.SetPerspectiveNearClip(cameraProps["PerspectiveNear"].as<float>());
				if (cameraProps["PerspectiveFar"])
					cc.Camera.SetPerspectiveFarClip(cameraProps["PerspectiveFar"].as<float>());

				if (cameraProps["OrthographicSize"])
					cc.Camera.SetOrthographicSize(cameraProps["OrthographicSize"].as<float>());
				if (cameraProps["OrthographicNear"])
					cc.Camera.SetOrthographicNearClip(cameraProps["OrthographicNear"].as<float>());
				if (cameraProps["OrthographicFar"])
					cc.Camera.SetOrthographicFarClip(cameraProps["OrthographicFar"].as<float>());
			}

			if (cameraComponent["FixedAspectRatio"]) cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();
		}

		auto staticMeshComponent = entity["StaticMeshComponent"];
		if (staticMeshComponent)
		{
			auto& mc = deserializedEntity.AddComponent<StaticMeshComponent>();

			if (staticMeshComponent["AssetPath"])
			{
				std::string assetPath = staticMeshComponent["AssetPath"].as<std::string>();
				mc.AssetPath = assetPath;
				mc.Mesh = Application::Get().GetSubsystem<AssetManager>()->GetMesh(assetPath);
			}

			if (staticMeshComponent["SubmeshIndex"])
				mc.SubmeshIndex = staticMeshComponent["SubmeshIndex"].as<uint32_t>();

			if (staticMeshComponent["MaterialAssetPath"])
			{
				std::string matPath = staticMeshComponent["MaterialAssetPath"].as<std::string>();
				if (!matPath.empty())
				{
					mc.MaterialAssetPath = matPath;
					mc.MaterialTableOverride = Application::Get().GetSubsystem<AssetManager>()->GetMaterial(matPath);
				}
			}

			if (staticMeshComponent["CastsShadows"])
				mc.CastsShadows = staticMeshComponent["CastsShadows"].as<bool>();
		}

		auto directionalLightComponent = entity["DirectionalLightComponent"];
		if (directionalLightComponent)
		{
			auto& dlc = deserializedEntity.AddComponent<DirectionalLightComponent>();

			if (directionalLightComponent["Color"])
				dlc.Color = directionalLightComponent["Color"].as<glm::vec3>();
			if (directionalLightComponent["Intensity"])
				dlc.Intensity = directionalLightComponent["Intensity"].as<float>();
		}

		auto pointLightComponent = entity["PointLightComponent"];
		if (pointLightComponent)
		{
			auto& plc = deserializedEntity.AddComponent<PointLightComponent>();

			if (pointLightComponent["Color"])
				plc.Color = pointLightComponent["Color"].as<glm::vec3>();
			if (pointLightComponent["Intensity"])
				plc.Intensity = pointLightComponent["Intensity"].as<float>();
			if (pointLightComponent["Radius"])
				plc.Radius = pointLightComponent["Radius"].as<float>();
			if (pointLightComponent["Falloff"])
				plc.Falloff = pointLightComponent["Falloff"].as<float>();
			if (pointLightComponent["CastsShadows"])
				plc.CastsShadows = pointLightComponent["CastsShadows"].as<bool>();
		}

		auto spotLightComponent = entity["SpotLightComponent"];
		if (spotLightComponent)
		{
			auto& sl = deserializedEntity.AddComponent<SpotLightComponent>();

			if (spotLightComponent["Color"])
				sl.Color = spotLightComponent["Color"].as<glm::vec3>();
			if (spotLightComponent["Intensity"])
				sl.Intensity = spotLightComponent["Intensity"].as<float>();
			if (spotLightComponent["Radius"])
				sl.Radius = spotLightComponent["Radius"].as<float>();
			if (spotLightComponent["Falloff"])
				sl.Falloff = spotLightComponent["Falloff"].as<float>();
			if (spotLightComponent["InnerAngle"])
				sl.InnerAngle = spotLightComponent["InnerAngle"].as<float>();
			if (spotLightComponent["OuterAngle"])
				sl.OuterAngle = spotLightComponent["OuterAngle"].as<float>();
			if (spotLightComponent["CastsShadows"])
				sl.CastsShadows = spotLightComponent["CastsShadows"].as<bool>();

			if (spotLightComponent["IsVideo"])
				sl.IsVideo = spotLightComponent["IsVideo"].as<bool>();

			if (spotLightComponent["CookieAssetPath"])
			{
				std::string path = spotLightComponent["CookieAssetPath"].as<std::string>();
				if (!path.empty())
				{
					sl.CookieAssetPath = path;

					if (sl.IsVideo)
						sl.CookieVideo = CreateRef<VideoTexture>(path);
					else
						sl.CookieTexture = Application::Get().GetSubsystem<AssetManager>()->GetTexture(path);
				}
			}

			if (spotLightComponent["CookieSize"])
				sl.CookieSize = spotLightComponent["CookieSize"].as<float>();
		}

		auto rigidbodyComponent = entity["RigidbodyComponent"];
		if (rigidbodyComponent)
		{
			auto& rb = deserializedEntity.AddComponent<RigidbodyComponent>();

			if (rigidbodyComponent["Type"])
				rb.Type = (RigidbodyComponent::BodyType)rigidbodyComponent["Type"].as<int>();
			if (rigidbodyComponent["Mass"])
				rb.Mass = rigidbodyComponent["Mass"].as<float>();
			if (rigidbodyComponent["LinearDrag"])
				rb.LinearDrag = rigidbodyComponent["LinearDrag"].as<float>();
			if (rigidbodyComponent["AngularDrag"])
				rb.AngularDrag = rigidbodyComponent["AngularDrag"].as<float>();
			if (rigidbodyComponent["FixedRotation"])
				rb.FixedRotation = rigidbodyComponent["FixedRotation"].as<bool>();
			if (rigidbodyComponent["UseCCD"])
				rb.UseCCD = rigidbodyComponent["UseCCD"].as<bool>();
			if (rigidbodyComponent["CCDVelocityThreshold"])
				rb.CCDVelocityThreshold = rigidbodyComponent["CCDVelocityThreshold"].as<float>();
		}

		auto boxColliderComponent = entity["BoxColliderComponent"];
		if (boxColliderComponent)
		{
			auto& bc = deserializedEntity.AddComponent<BoxColliderComponent>();

			if (boxColliderComponent["HalfExtents"])
				bc.HalfExtents = boxColliderComponent["HalfExtents"].as<glm::vec3>();
			if (boxColliderComponent["Offset"])
				bc.Offset = boxColliderComponent["Offset"].as<glm::vec3>();
			if (boxColliderComponent["IsTrigger"])
				bc.IsTrigger = boxColliderComponent["IsTrigger"].as<bool>();
			if (boxColliderComponent["IsAmbientZone"])
				bc.IsAmbientZone = boxColliderComponent["IsAmbientZone"].as<bool>();
			if (boxColliderComponent["AmbientIntensity"])
				bc.AmbientIntensity = boxColliderComponent["AmbientIntensity"].as<float>();
			if (boxColliderComponent["TransitionMin"])
				bc.TransitionMin = boxColliderComponent["TransitionMin"].as<glm::vec3>();
			if (boxColliderComponent["TransitionMax"])
				bc.TransitionMax = boxColliderComponent["TransitionMax"].as<glm::vec3>();

			if (boxColliderComponent["PhysicsMaterialPath"])
			{
				std::string path = boxColliderComponent["PhysicsMaterialPath"].as<std::string>();
				if (!path.empty())
				{
					bc.PhysicsMaterialPath = path;
					bc.PhysicsMaterialAsset = Application::Get().GetSubsystem<AssetManager>()->GetPhysicsMaterial(path);
				}
			}
		}

		auto sphereColliderComponent = entity["SphereColliderComponent"];
		if (sphereColliderComponent)
		{
			auto& sc = deserializedEntity.AddComponent<SphereColliderComponent>();

			if (sphereColliderComponent["Radius"])
				sc.Radius = sphereColliderComponent["Radius"].as<float>();
			if (sphereColliderComponent["Offset"])
				sc.Offset = sphereColliderComponent["Offset"].as<glm::vec3>();
			if (sphereColliderComponent["IsTrigger"])
				sc.IsTrigger = sphereColliderComponent["IsTrigger"].as<bool>();

			if (boxColliderComponent["PhysicsMaterialPath"])
			{
				std::string path = boxColliderComponent["PhysicsMaterialPath"].as<std::string>();
				if (!path.empty())
				{
					sc.PhysicsMaterialPath = path;
					sc.PhysicsMaterialAsset = Application::Get().GetSubsystem<AssetManager>()->GetPhysicsMaterial(path);
				}
			}
		}

		auto capsuleColliderComponent = entity["CapsuleColliderComponent"];
		if (capsuleColliderComponent)
		{
			auto& cc = deserializedEntity.AddComponent<CapsuleColliderComponent>();

			if (capsuleColliderComponent["Radius"])
				cc.Radius = capsuleColliderComponent["Radius"].as<float>();
			if (capsuleColliderComponent["Height"])
				cc.Height = capsuleColliderComponent["Height"].as<float>();
			if (capsuleColliderComponent["Offset"])
				cc.Offset = capsuleColliderComponent["Offset"].as<glm::vec3>();
			if (capsuleColliderComponent["IsTrigger"])
				cc.IsTrigger = capsuleColliderComponent["IsTrigger"].as<bool>();

			if (boxColliderComponent["PhysicsMaterialPath"])
			{
				std::string path = boxColliderComponent["PhysicsMaterialPath"].as<std::string>();
				if (!path.empty())
				{
					cc.PhysicsMaterialPath = path;
					cc.PhysicsMaterialAsset = Application::Get().GetSubsystem<AssetManager>()->GetPhysicsMaterial(path);
				}
			}
		}

		auto meshColliderComponent = entity["MeshColliderComponent"];
		if (meshColliderComponent)
		{
			auto& mc = deserializedEntity.AddComponent<MeshColliderComponent>();
			if (meshColliderComponent["IsConvex"])
				mc.IsConvex = meshColliderComponent["IsConvex"].as<bool>();
			if (meshColliderComponent["OverrideAssetPath"])
				mc.OverrideAssetPath = meshColliderComponent["OverrideAssetPath"].as<std::string>();
			if (meshColliderComponent["IsTrigger"])
				mc.IsTrigger = meshColliderComponent["IsTrigger"].as<bool>();

			if (boxColliderComponent["PhysicsMaterialPath"])
			{
				std::string path = boxColliderComponent["PhysicsMaterialPath"].as<std::string>();
				if (!path.empty())
				{
					mc.PhysicsMaterialPath = path;
					mc.PhysicsMaterialAsset = Application::Get().GetSubsystem<AssetManager>()->GetPhysicsMaterial(path);
				}
			}
		}

		auto characterControllerComponent = entity["CharacterControllerComponent"];
		if (characterControllerComponent)
		{
			auto& cct = deserializedEntity.AddComponent<CharacterControllerComponent>();
			if (characterControllerComponent["SlopeLimitDegrees"])
				cct.SlopeLimitDegrees = characterControllerComponent["SlopeLimitDegrees"].as<float>();
			if (characterControllerComponent["StepOffset"])
				cct.StepOffset = characterControllerComponent["StepOffset"].as<float>();
			if (characterControllerComponent["Radius"])
				cct.Radius = characterControllerComponent["Radius"].as<float>();
			if (characterControllerComponent["Height"])
				cct.Height = characterControllerComponent["Height"].as<float>();
		}

		auto scriptComponent = entity["ScriptComponent"];
		if (scriptComponent)
		{
			auto& sc = deserializedEntity.AddComponent<ScriptComponent>();
			if (scriptComponent["ClassName"]) sc.ClassName = scriptComponent["ClassName"].as<std::string>();

			auto scriptFields = scriptComponent["ScriptFields"];
			if (scriptFields)
			{
				for (auto field : scriptFields)
				{
					std::string name = field.first.as<std::string>();
					uint32_t type = field.second["Type"].as<uint32_t>();

					ScriptFieldInstance fieldInstance;
					fieldInstance.Type = type;

					switch ((ScriptFieldType)type)
					{
						case ScriptFieldType::Float:
						{
							float data = field.second["Data"].as<float>();
							memcpy(fieldInstance.Data.data(), &data, sizeof(float));
							break;
						}
						case ScriptFieldType::Bool:
						{
							bool data = field.second["Data"].as<bool>();
							memcpy(fieldInstance.Data.data(), &data, sizeof(bool));
							break;
						}
						case ScriptFieldType::Int:
						{
							int data = field.second["Data"].as<int>();
							memcpy(fieldInstance.Data.data(), &data, sizeof(int));
							break;
						}
						case ScriptFieldType::Vector2:
						{
							glm::vec2 data = field.second["Data"].as<glm::vec2>();
							memcpy(fieldInstance.Data.data(), &data, sizeof(glm::vec2));
							break;
						}
						case ScriptFieldType::Vector3:
						{
							glm::vec3 data = field.second["Data"].as<glm::vec3>();
							memcpy(fieldInstance.Data.data(), &data, sizeof(glm::vec3));
							break;
						}
						case ScriptFieldType::Vector4:
						{
							glm::vec4 data = field.second["Data"].as<glm::vec4>();
							memcpy(fieldInstance.Data.data(), &data, sizeof(glm::vec4));
							break;
						}
						case ScriptFieldType::Entity:
						{
							uint64_t data = field.second["Data"].as<uint64_t>();
							memcpy(fieldInstance.Data.data(), &data, sizeof(uint64_t));
							break;
						}
					}
					sc.FieldInstances[name] = fieldInstance;
				}
			}
		}

		auto audioSourceComponent = entity["AudioSourceComponent"];
		if (audioSourceComponent)
		{
			auto& ac = deserializedEntity.AddComponent<AudioSourceComponent>();
			if (audioSourceComponent["AudioClipPath"])
				ac.AudioClipPath = audioSourceComponent["AudioClipPath"].as<std::string>();
			if (audioSourceComponent["PlayOnAwake"])
				ac.PlayOnAwake = audioSourceComponent["PlayOnAwake"].as<bool>();
			if (audioSourceComponent["Looping"])
				ac.Looping = audioSourceComponent["Looping"].as<bool>();
			if (audioSourceComponent["Volume"])
				ac.Volume = audioSourceComponent["Volume"].as<float>();
			if (audioSourceComponent["MinDistance"])
				ac.MinDistance = audioSourceComponent["MinDistance"].as<float>();
			if (audioSourceComponent["MaxDistance"])
				ac.MaxDistance = audioSourceComponent["MaxDistance"].as<float>();
		}

		auto uiCanvasComponent = entity["UICanvasComponent"];
		if (uiCanvasComponent)
		{
			auto& uc = deserializedEntity.AddComponent<UICanvasComponent>();
			if (uiCanvasComponent["Active"])
				uc.Active = uiCanvasComponent["Active"].as<bool>();
			if (uiCanvasComponent["RenderMode"])
				uc.RenderMode = (CanvasRenderMode)uiCanvasComponent["RenderMode"].as<int>();
			if (uiCanvasComponent["ReferenceResolution"])
				uc.ReferenceResolution = uiCanvasComponent["ReferenceResolution"].as<glm::vec2>();
		}

		auto uiTransformComponent = entity["UITransformComponent"];
		if (uiTransformComponent)
		{
			auto& ut = deserializedEntity.AddComponent<UITransformComponent>();
			if (uiTransformComponent["AnchorMin"])
				ut.AnchorMin = uiTransformComponent["AnchorMin"].as<glm::vec2>();
			if (uiTransformComponent["AnchorMax"])
				ut.AnchorMax = uiTransformComponent["AnchorMax"].as<glm::vec2>();
			if (uiTransformComponent["OffsetMin"])
				ut.OffsetMin = uiTransformComponent["OffsetMin"].as<glm::vec2>();
			if (uiTransformComponent["OffsetMax"])
				ut.OffsetMax = uiTransformComponent["OffsetMax"].as<glm::vec2>();
			if (uiTransformComponent["ZIndex"])
				ut.ZIndex = uiTransformComponent["ZIndex"].as<int>();
		}

		auto uiImageComponent = entity["UIImageComponent"];
		if (uiImageComponent)
		{
			auto& ui = deserializedEntity.AddComponent<UIImageComponent>();
			if (uiImageComponent["TintColor"])
				ui.TintColor = uiImageComponent["TintColor"].as<glm::vec4>();
			if (uiImageComponent["TextureAssetPath"])
			{
				ui.TextureAssetPath = uiImageComponent["TextureAssetPath"].as<std::string>();
				if (!ui.TextureAssetPath.empty())
					ui.Texture = Application::Get().GetSubsystem<AssetManager>()->GetTexture(ui.TextureAssetPath);
			}
		}

		auto uiTextComponent = entity["UITextComponent"];
		if (uiTextComponent)
		{
			auto& ut = deserializedEntity.AddComponent<UITextComponent>();
			if (uiTextComponent["Text"])
				ut.Text = uiTextComponent["Text"].as<std::string>();
			if (uiTextComponent["FontAsset"])
			{
				std::string fontPath = uiTextComponent["FontAsset"].as<std::string>();
				if (!fontPath.empty())
					ut.FontAsset = CreateRef<Font>(fontPath);
			}
			if (uiTextComponent["Color"])
				ut.Color = uiTextComponent["Color"].as<glm::vec4>();
			if (uiTextComponent["FontSize"])
				ut.FontSize = uiTextComponent["FontSize"].as<float>();
			if (uiTextComponent["LineSpacing"])
				ut.LineSpacing = uiTextComponent["LineSpacing"].as<float>();
			if (uiTextComponent["Kerning"])
				ut.Kerning = uiTextComponent["Kerning"].as<float>();
		}

		auto uiButtonComponent = entity["UIButtonComponent"];
		if (uiButtonComponent)
		{
			auto& ub = deserializedEntity.AddComponent<UIButtonComponent>();
			if (uiButtonComponent["NormalColor"])
				ub.NormalColor = uiButtonComponent["NormalColor"].as<glm::vec4>();
			if (uiButtonComponent["HoverColor"])
				ub.HoverColor = uiButtonComponent["HoverColor"].as<glm::vec4>();
			if (uiButtonComponent["PressedColor"])
				ub.PressedColor = uiButtonComponent["PressedColor"].as<glm::vec4>();
		}

	}

	void SceneSerializer::Serialize(const std::string& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << "Untitled";

		Entity primaryCam = m_Scene->GetPrimaryCameraEntity();
		if (primaryCam)
			out << YAML::Key << "PrimaryCameraID" << YAML::Value << primaryCam.GetUUID();

		if (m_Scene->m_Skybox)
		{
			out << YAML::Key << "Skybox";
			out << YAML::BeginMap;

			out << YAML::Key << "TexturePath" << YAML::Value << FileSystem::GetRelativePath(m_Scene->m_Skybox->GetPath());

			out << YAML::Key << "Intensity" << YAML::Value << m_Scene->m_SkyboxIntensity;

			out << YAML::EndMap;
		}

		out << YAML::Key << "RendererSettings";
		out << YAML::BeginMap;
		out << YAML::Key << "Exposure" << YAML::Value << m_Scene->m_RendererSettings.Exposure;
		out << YAML::Key << "Gamma" << YAML::Value << m_Scene->m_RendererSettings.Gamma;
		out << YAML::Key << "BloomThreshold" << YAML::Value << m_Scene->m_RendererSettings.BloomThreshold;
		out << YAML::Key << "BloomKnee" << YAML::Value << m_Scene->m_RendererSettings.BloomKnee;
		out << YAML::Key << "BloomIntensity" << YAML::Value << m_Scene->m_RendererSettings.BloomIntensity;
		out << YAML::Key << "BloomFilterRadius" << YAML::Value << m_Scene->m_RendererSettings.BloomFilterRadius;
		out << YAML::Key << "ShowColliders" << YAML::Value << m_Scene->m_RendererSettings.ShowColliders;
		out << YAML::Key << "LightSize" << YAML::Value << m_Scene->m_RendererSettings.LightSize;
		out << YAML::Key << "ContactThreshold" << YAML::Value << m_Scene->m_RendererSettings.ContactThreshold;
		out << YAML::Key << "ContactSharpness" << YAML::Value << m_Scene->m_RendererSettings.ContactSharpness;
		out << YAML::Key << "ContactSharpeningBias" << YAML::Value << m_Scene->m_RendererSettings.ContactSharpeningBias;
		out << YAML::EndMap;

		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		m_Scene->GetRaw().view<entt::entity>().each([&](auto entityID)
			{
				Entity entity = { entityID, m_Scene.get() };
				if (!entity)
					return;

				SerializeEntity(out, entity);
			});
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
	}

	enum class BinaryComponentType : uint32_t
	{
		None = 0,
		Transform,
		Relationship,
		StaticMesh,
		Camera,
		DirectionalLight,
		PointLight,
		SpotLight,
		NativeScript,
		Script,
		Rigidbody,
		BoxCollider,
		SphereCollider,
		CapsuleCollider,
		MeshCollider,
		CharacterController,
		AudioSource,
		UICanvas,
		UITransform,
		UIImage,
		UIText,
		UIButton
	};

	struct BinaryWriter
	{
		std::vector<uint8_t> Buffer;

		template<typename T>
		void Write(const T& value)
		{
			size_t offset = Buffer.size();
			Buffer.resize(offset + sizeof(T));
			memcpy(Buffer.data() + offset, &value, sizeof(T));
		}

		void WriteString(const std::string& str)
		{
			uint32_t len = (uint32_t)str.size();
			Write(len);
			if (len > 0)
			{
				size_t offset = Buffer.size();
				Buffer.resize(offset + len);
				memcpy(Buffer.data() + offset, str.data(), len);
			}
		}

		void WriteBytes(const void* src, size_t size)
		{
			size_t offset = Buffer.size();
			Buffer.resize(offset + size);
			memcpy(Buffer.data() + offset, src, size);
		}
	};

	struct BinaryReader
	{
		const uint8_t* Buffer = nullptr;
		size_t Size = 0;
		size_t Offset = 0;

		BinaryReader(const uint8_t* buffer, size_t size)
			: Buffer(buffer), Size(size), Offset(0) {}

		template<typename T>
		void Read(T& value)
		{
			RXN_CORE_ASSERT(Offset + sizeof(T) <= Size);
			memcpy(&value, Buffer + Offset, sizeof(T));
			Offset += sizeof(T);
		}

		std::string ReadString()
		{
			uint32_t len = 0;
			Read(len);
			if (len == 0) return "";
			RXN_CORE_ASSERT(Offset + len <= Size);
			std::string str((const char*)(Buffer + Offset), len);
			Offset += len;
			return str;
		}

		void ReadBytes(void* dest, size_t size)
		{
			RXN_CORE_ASSERT(Offset + size <= Size);
			memcpy(dest, Buffer + Offset, size);
			Offset += size;
		}
	};

	void SceneSerializer::SerializeRuntime(const std::string& filepath)
	{
		BinaryWriter writer;

		// Magic and header
		writer.WriteBytes("RXNBIN\0\0", 8);
		uint32_t version = 2;
		writer.Write(version);
		// Scene properties
		Entity primaryCam = m_Scene->GetPrimaryCameraEntity();
		uint64_t primaryCameraID = primaryCam ? (uint64_t)primaryCam.GetUUID() : 0;
		writer.Write(primaryCameraID);

		std::string skyboxPath = "";
		float skyboxIntensity = 1.0f;
		if (m_Scene->m_Skybox)
		{
			skyboxPath = m_Scene->m_Skybox->GetPath();
			skyboxIntensity = m_Scene->m_SkyboxIntensity;
		}
		writer.WriteString(skyboxPath);
		writer.Write(skyboxIntensity);

		// Renderer settings
		writer.Write(m_Scene->m_RendererSettings.Exposure);
		writer.Write(m_Scene->m_RendererSettings.Gamma);
		writer.Write(m_Scene->m_RendererSettings.BloomThreshold);
		writer.Write(m_Scene->m_RendererSettings.BloomKnee);
		writer.Write(m_Scene->m_RendererSettings.BloomIntensity);
		writer.Write(m_Scene->m_RendererSettings.BloomFilterRadius);
		writer.Write(m_Scene->m_RendererSettings.ShowColliders);
		writer.Write(m_Scene->m_RendererSettings.LightSize);
		writer.Write(m_Scene->m_RendererSettings.ContactThreshold);
		writer.Write(m_Scene->m_RendererSettings.ContactSharpness);
		writer.Write(m_Scene->m_RendererSettings.ContactSharpeningBias);

		// Gather entities
		std::vector<entt::entity> entities;
		m_Scene->GetRaw().view<entt::entity>().each([&](auto entityID)
		{
			entities.push_back(entityID);
		});

		uint32_t entityCount = (uint32_t)entities.size();
		writer.Write(entityCount);

		for (auto entityID : entities)
		{
			Entity entity = { entityID, m_Scene.get() };
			if (!entity)
				continue;

			// Write UUID and Tag
			writer.Write((uint64_t)entity.GetUUID());
			writer.WriteString(entity.GetName());

			// Count components
			uint32_t componentCount = 0;
			if (entity.HasComponent<TransformComponent>()) componentCount++;
			if (entity.HasComponent<RelationshipComponent>()) componentCount++;
			if (entity.HasComponent<StaticMeshComponent>()) componentCount++;
			if (entity.HasComponent<CameraComponent>()) componentCount++;
			if (entity.HasComponent<DirectionalLightComponent>()) componentCount++;
			if (entity.HasComponent<PointLightComponent>()) componentCount++;
			if (entity.HasComponent<SpotLightComponent>()) componentCount++;
			if (entity.HasComponent<RigidbodyComponent>()) componentCount++;
			if (entity.HasComponent<BoxColliderComponent>()) componentCount++;
			if (entity.HasComponent<SphereColliderComponent>()) componentCount++;
			if (entity.HasComponent<CapsuleColliderComponent>()) componentCount++;
			if (entity.HasComponent<MeshColliderComponent>()) componentCount++;
			if (entity.HasComponent<CharacterControllerComponent>()) componentCount++;
			if (entity.HasComponent<ScriptComponent>()) componentCount++;
			if (entity.HasComponent<AudioSourceComponent>()) componentCount++;
			if (entity.HasComponent<UICanvasComponent>()) componentCount++;
			if (entity.HasComponent<UITransformComponent>()) componentCount++;
			if (entity.HasComponent<UIImageComponent>()) componentCount++;
			if (entity.HasComponent<UITextComponent>()) componentCount++;
			if (entity.HasComponent<UIButtonComponent>()) componentCount++;

			writer.Write(componentCount);

			if (entity.HasComponent<TransformComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::Transform);
				auto& tc = entity.GetComponent<TransformComponent>();
				writer.Write(tc.Translation);
				writer.Write(tc.Rotation);
				writer.Write(tc.Scale);
			}

			if (entity.HasComponent<RelationshipComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::Relationship);
				auto& rc = entity.GetComponent<RelationshipComponent>();
				writer.Write(rc.ParentHandle);
				uint32_t childrenCount = (uint32_t)rc.Children.size();
				writer.Write(childrenCount);
				for (auto child : rc.Children)
					writer.Write((uint64_t)child);
			}

			if (entity.HasComponent<StaticMeshComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::StaticMesh);
				auto& mc = entity.GetComponent<StaticMeshComponent>();
				writer.WriteString(mc.AssetPath);
				writer.Write(mc.SubmeshIndex);
				writer.WriteString(mc.MaterialAssetPath);
			}

			if (entity.HasComponent<CameraComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::Camera);
				auto& cc = entity.GetComponent<CameraComponent>();
				auto& camera = cc.Camera;
				writer.Write((uint32_t)camera.GetProjectionType());
				writer.Write(camera.GetPerspectiveFOV());
				writer.Write(camera.GetPerspectiveNearClip());
				writer.Write(camera.GetPerspectiveFarClip());
				writer.Write(camera.GetOrthographicSize());
				writer.Write(camera.GetOrthographicNearClip());
				writer.Write(camera.GetOrthographicFarClip());
				writer.Write(cc.FixedAspectRatio);
			}

			if (entity.HasComponent<DirectionalLightComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::DirectionalLight);
				auto& dl = entity.GetComponent<DirectionalLightComponent>();
				writer.Write(dl.Color);
				writer.Write(dl.Intensity);
			}

			if (entity.HasComponent<PointLightComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::PointLight);
				auto& pl = entity.GetComponent<PointLightComponent>();
				writer.Write(pl.Color);
				writer.Write(pl.Intensity);
				writer.Write(pl.Radius);
				writer.Write(pl.Falloff);
				writer.Write(pl.CastsShadows);
			}

			if (entity.HasComponent<SpotLightComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::SpotLight);
				auto& sl = entity.GetComponent<SpotLightComponent>();
				writer.Write(sl.Color);
				writer.Write(sl.Intensity);
				writer.Write(sl.Radius);
				writer.Write(sl.Falloff);
				writer.Write(sl.InnerAngle);
				writer.Write(sl.OuterAngle);
				writer.Write(sl.CastsShadows);
				writer.Write(sl.IsVideo);
				writer.WriteString(sl.CookieAssetPath);
				writer.Write(sl.CookieSize);
			}

			if (entity.HasComponent<RigidbodyComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::Rigidbody);
				auto& rb = entity.GetComponent<RigidbodyComponent>();
				writer.Write((uint32_t)rb.Type);
				writer.Write(rb.Mass);
				writer.Write(rb.LinearDrag);
				writer.Write(rb.AngularDrag);
				writer.Write(rb.FixedRotation);
				writer.Write(rb.UseCCD);
				writer.Write(rb.CCDVelocityThreshold);
			}

			if (entity.HasComponent<BoxColliderComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::BoxCollider);
				auto& bc = entity.GetComponent<BoxColliderComponent>();
				writer.Write(bc.HalfExtents);
				writer.Write(bc.Offset);
				writer.Write(bc.IsTrigger);
				writer.Write(bc.IsAmbientZone);
				writer.Write(bc.AmbientIntensity);
				writer.Write(bc.TransitionMin);
				writer.Write(bc.TransitionMax);
				writer.WriteString(bc.PhysicsMaterialPath);
			}

			if (entity.HasComponent<SphereColliderComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::SphereCollider);
				auto& sc = entity.GetComponent<SphereColliderComponent>();
				writer.Write(sc.Radius);
				writer.Write(sc.Offset);
				writer.Write(sc.IsTrigger);
				writer.WriteString(sc.PhysicsMaterialPath);
			}

			if (entity.HasComponent<CapsuleColliderComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::CapsuleCollider);
				auto& cc = entity.GetComponent<CapsuleColliderComponent>();
				writer.Write(cc.Radius);
				writer.Write(cc.Height);
				writer.Write(cc.Offset);
				writer.Write(cc.IsTrigger);
				writer.WriteString(cc.PhysicsMaterialPath);
			}

			if (entity.HasComponent<MeshColliderComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::MeshCollider);
				auto& mc = entity.GetComponent<MeshColliderComponent>();
				writer.Write(mc.IsConvex);
				writer.WriteString(mc.OverrideAssetPath);
				writer.Write(mc.IsTrigger);
				writer.WriteString(mc.PhysicsMaterialPath);
			}

			if (entity.HasComponent<CharacterControllerComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::CharacterController);
				auto& cct = entity.GetComponent<CharacterControllerComponent>();
				writer.Write(cct.SlopeLimitDegrees);
				writer.Write(cct.StepOffset);
				writer.Write(cct.Radius);
				writer.Write(cct.Height);
			}

			if (entity.HasComponent<ScriptComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::Script);
				auto& sc = entity.GetComponent<ScriptComponent>();
				writer.WriteString(sc.ClassName);
				uint32_t fieldsCount = (uint32_t)sc.FieldInstances.size();
				writer.Write(fieldsCount);
				for (const auto& [name, field] : sc.FieldInstances)
				{
					writer.WriteString(name);
					writer.Write(field.Type);
					writer.WriteBytes(field.Data.data(), field.Data.size());
				}
			}

			if (entity.HasComponent<AudioSourceComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::AudioSource);
				auto& ac = entity.GetComponent<AudioSourceComponent>();
				writer.WriteString(ac.AudioClipPath);
				writer.Write(ac.PlayOnAwake);
				writer.Write(ac.Looping);
				writer.Write(ac.Volume);
				writer.Write(ac.MinDistance);
				writer.Write(ac.MaxDistance);
			}

			if (entity.HasComponent<UICanvasComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::UICanvas);
				auto& uc = entity.GetComponent<UICanvasComponent>();
				writer.Write(uc.Active);
				writer.Write((uint32_t)uc.RenderMode);
				writer.Write(uc.ReferenceResolution);
			}

			if (entity.HasComponent<UITransformComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::UITransform);
				auto& ut = entity.GetComponent<UITransformComponent>();
				writer.Write(ut.AnchorMin);
				writer.Write(ut.AnchorMax);
				writer.Write(ut.OffsetMin);
				writer.Write(ut.OffsetMax);
				writer.Write(ut.ZIndex);
			}

			if (entity.HasComponent<UIImageComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::UIImage);
				auto& ui = entity.GetComponent<UIImageComponent>();
				writer.Write(ui.TintColor);
				writer.WriteString(ui.TextureAssetPath);
			}

			if (entity.HasComponent<UITextComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::UIText);
				auto& ut = entity.GetComponent<UITextComponent>();
				writer.WriteString(ut.Text);
				std::string fontPath = ut.FontAsset ? ut.FontAsset->GetPath().string() : "";
				writer.WriteString(fontPath);
				writer.Write(ut.Color);
				writer.Write(ut.FontSize);
				writer.Write(ut.LineSpacing);
				writer.Write(ut.Kerning);
			}

			if (entity.HasComponent<UIButtonComponent>())
			{
				writer.Write((uint32_t)BinaryComponentType::UIButton);
				auto& ub = entity.GetComponent<UIButtonComponent>();
				writer.Write(ub.NormalColor);
				writer.Write(ub.HoverColor);
				writer.Write(ub.PressedColor);
			}
		}

		std::ofstream fout(filepath, std::ios::binary);
		if (fout.is_open())
		{
			fout.write((const char*)writer.Buffer.data(), writer.Buffer.size());
		}
	}

	bool SceneSerializer::Deserialize(const std::string& filepath)
	{
		YAML::Node data;
		try
		{
			data = YAML::LoadFile(filepath);
		}
		catch (const std::exception& e)
		{
			RXN_CORE_ERROR("Failed to load .rxn file '{0}'\n     {1}", filepath, e.what());
			return false;
		}

		if (!data["Scene"])
			return false;

		std::string sceneName = data["Scene"].as<std::string>();
		RXN_CORE_TRACE("Deserializing scene '{0}'", sceneName);

		UUID primaryCameraID = UUID::Null;
		if (data["PrimaryCameraID"])
			primaryCameraID = data["PrimaryCameraID"].as<UUID>();

		auto skyboxData = data["Skybox"];
		if (skyboxData)
		{
			if (skyboxData["TexturePath"])
				m_Scene->m_Skybox = Cubemap::Create(skyboxData["TexturePath"].as<std::string>());

			if (skyboxData["Intensity"])
				m_Scene->m_SkyboxIntensity = skyboxData["Intensity"].as<float>();
		}

		auto rendererSettings = data["RendererSettings"];
		if (rendererSettings)
		{
			if (rendererSettings["Exposure"]) m_Scene->m_RendererSettings.Exposure = rendererSettings["Exposure"].as<float>();
			if (rendererSettings["Gamma"]) m_Scene->m_RendererSettings.Gamma = rendererSettings["Gamma"].as<float>();
			if (rendererSettings["BloomThreshold"]) m_Scene->m_RendererSettings.BloomThreshold = rendererSettings["BloomThreshold"].as<float>();
			if (rendererSettings["BloomKnee"]) m_Scene->m_RendererSettings.BloomKnee = rendererSettings["BloomKnee"].as<float>();
			if (rendererSettings["BloomIntensity"]) m_Scene->m_RendererSettings.BloomIntensity = rendererSettings["BloomIntensity"].as<float>();
			if (rendererSettings["BloomFilterRadius"]) m_Scene->m_RendererSettings.BloomFilterRadius = rendererSettings["BloomFilterRadius"].as<float>();
			if (rendererSettings["ShowColliders"]) m_Scene->m_RendererSettings.ShowColliders = rendererSettings["ShowColliders"].as<bool>();
			if (rendererSettings["LightSize"]) m_Scene->m_RendererSettings.LightSize = rendererSettings["LightSize"].as<float>();
			if (rendererSettings["ContactThreshold"]) m_Scene->m_RendererSettings.ContactThreshold = rendererSettings["ContactThreshold"].as<float>();
			if (rendererSettings["ContactSharpness"]) m_Scene->m_RendererSettings.ContactSharpness = rendererSettings["ContactSharpness"].as<float>();
			if (rendererSettings["ContactSharpeningBias"]) m_Scene->m_RendererSettings.ContactSharpeningBias = rendererSettings["ContactSharpeningBias"].as<float>();
		}

		auto entities = data["Entities"];
		if (entities)
		{
			for (auto entity : entities)
			{
				if (!entity["Entity"])
					continue;

				uint64_t uuid = entity["Entity"].as<uint64_t>();

				std::string name;
				auto tagComponent = entity["TagComponent"];
				if (tagComponent && tagComponent["Tag"])
					name = tagComponent["Tag"].as<std::string>();

				RXN_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name);
				Entity deserializedEntity = m_Scene->CreateEntityWithUUID(uuid, name);

				DeserializeComponentsToEntity(entity, deserializedEntity);

				if (primaryCameraID != UUID::Null && uuid == (uint64_t)primaryCameraID)
				{
					m_Scene->SetPrimaryCameraEntity(deserializedEntity);
				}
			}
		}

		return true;
	}

	bool SceneSerializer::DeserializeRuntime(const std::string& filepath)
	{
		std::vector<uint8_t> buffer;
		auto vfs = Application::Get().GetSubsystem<VFSSystem>();

		if (vfs && vfs->FileExists(filepath))
		{
			buffer = vfs->ReadFile(filepath);
			if (buffer.empty())
			{
				RXN_CORE_ERROR("Failed to read binary scene file '{0}' from VFS", filepath);
				return false;
			}
		}
		else
		{
			std::ifstream file(filepath, std::ios::binary | std::ios::ate);
			if (!file.is_open())
			{
				RXN_CORE_ERROR("Failed to open binary scene file '{0}' on disk", filepath);
				return false;
			}

			size_t size = (size_t)file.tellg();
			file.seekg(0, std::ios::beg);

			buffer.resize(size);
			file.read((char*)buffer.data(), size);
			file.close();
		}

		size_t size = buffer.size();
		BinaryReader reader(buffer.data(), size);

		RXN_CORE_INFO("DeserializeRuntime: Starting deserialization of file {0} (size: {1} bytes)", filepath, size);

		char magic[8];
		reader.ReadBytes(magic, 8);
		if (memcmp(magic, "RXNBIN\0\0", 8) != 0)
		{
			RXN_CORE_ERROR("Invalid binary scene file '{0}'", filepath);
			return false;
		}

		uint32_t version = 0;
		reader.Read(version);
		if (version != 1 && version != 2)
		{
			RXN_CORE_ERROR("Unsupported binary scene version {0} in file '{1}'", version, filepath);
			return false;
		}

		uint64_t primaryCameraID = 0;
		reader.Read(primaryCameraID);

		std::string skyboxPath = reader.ReadString();
		float skyboxIntensity = 1.0f;
		reader.Read(skyboxIntensity);

		if (version == 2)
		{
			reader.Read(m_Scene->m_RendererSettings.Exposure);
			reader.Read(m_Scene->m_RendererSettings.Gamma);
			reader.Read(m_Scene->m_RendererSettings.BloomThreshold);
			reader.Read(m_Scene->m_RendererSettings.BloomKnee);
			reader.Read(m_Scene->m_RendererSettings.BloomIntensity);
			reader.Read(m_Scene->m_RendererSettings.BloomFilterRadius);
			reader.Read(m_Scene->m_RendererSettings.ShowColliders);
			reader.Read(m_Scene->m_RendererSettings.LightSize);
			reader.Read(m_Scene->m_RendererSettings.ContactThreshold);
			reader.Read(m_Scene->m_RendererSettings.ContactSharpness);
			reader.Read(m_Scene->m_RendererSettings.ContactSharpeningBias);
		}

		if (!skyboxPath.empty())
		{
			m_Scene->m_Skybox = Cubemap::Create(skyboxPath);
			m_Scene->m_SkyboxIntensity = skyboxIntensity;
		}

		uint32_t entityCount = 0;
		reader.Read(entityCount);

		for (uint32_t i = 0; i < entityCount; i++)
		{
			uint64_t uuid = 0;
			reader.Read(uuid);
			std::string name = reader.ReadString();

			Entity deserializedEntity = m_Scene->CreateEntityWithUUID(uuid, name);

			uint32_t componentCount = 0;
			reader.Read(componentCount);

			for (uint32_t c = 0; c < componentCount; c++)
			{
				uint32_t typeVal = 0;
				reader.Read(typeVal);
				BinaryComponentType type = (BinaryComponentType)typeVal;

				switch (type)
				{
					case BinaryComponentType::Transform:
					{
						auto& tc = deserializedEntity.GetComponent<TransformComponent>();
						reader.Read(tc.Translation);
						reader.Read(tc.Rotation);
						reader.Read(tc.Scale);
						break;
					}
					case BinaryComponentType::Relationship:
					{
						auto& rc = deserializedEntity.GetComponent<RelationshipComponent>();
						reader.Read(rc.ParentHandle);
						uint32_t childrenCount = 0;
						reader.Read(childrenCount);
						rc.Children.resize(childrenCount);
						for (uint32_t ch = 0; ch < childrenCount; ch++)
						{
							uint64_t childUUID = 0;
							reader.Read(childUUID);
							rc.Children[ch] = childUUID;
						}
						break;
					}
					case BinaryComponentType::StaticMesh:
					{
						auto& mc = deserializedEntity.AddComponent<StaticMeshComponent>();
						mc.AssetPath = reader.ReadString();
						reader.Read(mc.SubmeshIndex);
						mc.MaterialAssetPath = reader.ReadString();

						if (!mc.AssetPath.empty())
							mc.Mesh = Application::Get().GetSubsystem<AssetManager>()->GetMesh(mc.AssetPath);

						if (!mc.MaterialAssetPath.empty())
							mc.MaterialTableOverride = Application::Get().GetSubsystem<AssetManager>()->GetMaterial(mc.MaterialAssetPath);
						break;
					}
					case BinaryComponentType::Camera:
					{
						auto& cc = deserializedEntity.AddComponent<CameraComponent>();
						uint32_t projType = 0;
						reader.Read(projType);
						cc.Camera.SetProjectionType((SceneCamera::ProjectionMode)projType);
						float perspectiveFOV = 0.0f, perspectiveNear = 0.0f, perspectiveFar = 0.0f;
						reader.Read(perspectiveFOV);
						reader.Read(perspectiveNear);
						reader.Read(perspectiveFar);
						cc.Camera.SetPerspectiveFOV(perspectiveFOV);
						cc.Camera.SetPerspectiveNearClip(perspectiveNear);
						cc.Camera.SetPerspectiveFarClip(perspectiveFar);

						float orthographicSize = 0.0f, orthographicNear = 0.0f, orthographicFar = 0.0f;
						reader.Read(orthographicSize);
						reader.Read(orthographicNear);
						reader.Read(orthographicFar);
						cc.Camera.SetOrthographicSize(orthographicSize);
						cc.Camera.SetOrthographicNearClip(orthographicNear);
						cc.Camera.SetOrthographicFarClip(orthographicFar);

						reader.Read(cc.FixedAspectRatio);
						break;
					}
					case BinaryComponentType::DirectionalLight:
					{
						auto& dl = deserializedEntity.AddComponent<DirectionalLightComponent>();
						reader.Read(dl.Color);
						reader.Read(dl.Intensity);
						break;
					}
					case BinaryComponentType::PointLight:
					{
						auto& pl = deserializedEntity.AddComponent<PointLightComponent>();
						reader.Read(pl.Color);
						reader.Read(pl.Intensity);
						reader.Read(pl.Radius);
						reader.Read(pl.Falloff);
						reader.Read(pl.CastsShadows);
						break;
					}
					case BinaryComponentType::SpotLight:
					{
						auto& sl = deserializedEntity.AddComponent<SpotLightComponent>();
						reader.Read(sl.Color);
						reader.Read(sl.Intensity);
						reader.Read(sl.Radius);
						reader.Read(sl.Falloff);
						reader.Read(sl.InnerAngle);
						reader.Read(sl.OuterAngle);
						reader.Read(sl.CastsShadows);
						reader.Read(sl.IsVideo);
						sl.CookieAssetPath = reader.ReadString();
						reader.Read(sl.CookieSize);

						if (!sl.CookieAssetPath.empty())
						{
							if (sl.IsVideo)
								sl.CookieVideo = CreateRef<VideoTexture>(sl.CookieAssetPath);
							else
								sl.CookieTexture = Application::Get().GetSubsystem<AssetManager>()->GetTexture(sl.CookieAssetPath);
						}
						break;
					}
					case BinaryComponentType::Rigidbody:
					{
						auto& rb = deserializedEntity.AddComponent<RigidbodyComponent>();
						uint32_t bodyType = 0;
						reader.Read(bodyType);
						rb.Type = (RigidbodyComponent::BodyType)bodyType;
						reader.Read(rb.Mass);
						reader.Read(rb.LinearDrag);
						reader.Read(rb.AngularDrag);
						reader.Read(rb.FixedRotation);
						reader.Read(rb.UseCCD);
						reader.Read(rb.CCDVelocityThreshold);
						break;
					}
					case BinaryComponentType::BoxCollider:
					{
						auto& bc = deserializedEntity.AddComponent<BoxColliderComponent>();
						reader.Read(bc.HalfExtents);
						reader.Read(bc.Offset);
						reader.Read(bc.IsTrigger);
						reader.Read(bc.IsAmbientZone);
						reader.Read(bc.AmbientIntensity);
						reader.Read(bc.TransitionMin);
						reader.Read(bc.TransitionMax);
						bc.PhysicsMaterialPath = reader.ReadString();

						if (!bc.PhysicsMaterialPath.empty())
							bc.PhysicsMaterialAsset = Application::Get().GetSubsystem<AssetManager>()->GetPhysicsMaterial(bc.PhysicsMaterialPath);
						break;
					}
					case BinaryComponentType::SphereCollider:
					{
						auto& sc = deserializedEntity.AddComponent<SphereColliderComponent>();
						reader.Read(sc.Radius);
						reader.Read(sc.Offset);
						reader.Read(sc.IsTrigger);
						sc.PhysicsMaterialPath = reader.ReadString();

						if (!sc.PhysicsMaterialPath.empty())
							sc.PhysicsMaterialAsset = Application::Get().GetSubsystem<AssetManager>()->GetPhysicsMaterial(sc.PhysicsMaterialPath);
						break;
					}
					case BinaryComponentType::CapsuleCollider:
					{
						auto& cc = deserializedEntity.AddComponent<CapsuleColliderComponent>();
						reader.Read(cc.Radius);
						reader.Read(cc.Height);
						reader.Read(cc.Offset);
						reader.Read(cc.IsTrigger);
						cc.PhysicsMaterialPath = reader.ReadString();

						if (!cc.PhysicsMaterialPath.empty())
							cc.PhysicsMaterialAsset = Application::Get().GetSubsystem<AssetManager>()->GetPhysicsMaterial(cc.PhysicsMaterialPath);
						break;
					}
					case BinaryComponentType::MeshCollider:
					{
						auto& mc = deserializedEntity.AddComponent<MeshColliderComponent>();
						reader.Read(mc.IsConvex);
						mc.OverrideAssetPath = reader.ReadString();
						reader.Read(mc.IsTrigger);
						mc.PhysicsMaterialPath = reader.ReadString();

						if (!mc.PhysicsMaterialPath.empty())
							mc.PhysicsMaterialAsset = Application::Get().GetSubsystem<AssetManager>()->GetPhysicsMaterial(mc.PhysicsMaterialPath);
						break;
					}
					case BinaryComponentType::CharacterController:
					{
						auto& cct = deserializedEntity.AddComponent<CharacterControllerComponent>();
						reader.Read(cct.SlopeLimitDegrees);
						reader.Read(cct.StepOffset);
						reader.Read(cct.Radius);
						reader.Read(cct.Height);
						break;
					}
					case BinaryComponentType::Script:
					{
						auto& sc = deserializedEntity.AddComponent<ScriptComponent>();
						sc.ClassName = reader.ReadString();
						uint32_t fieldsCount = 0;
						reader.Read(fieldsCount);
						for (uint32_t f = 0; f < fieldsCount; f++)
						{
							std::string fieldName = reader.ReadString();
							ScriptFieldInstance field;
							reader.Read(field.Type);
							reader.ReadBytes(field.Data.data(), field.Data.size());
							sc.FieldInstances[fieldName] = field;
						}
						break;
					}
					case BinaryComponentType::AudioSource:
					{
						auto& ac = deserializedEntity.AddComponent<AudioSourceComponent>();
						ac.AudioClipPath = reader.ReadString();
						reader.Read(ac.PlayOnAwake);
						reader.Read(ac.Looping);
						reader.Read(ac.Volume);
						reader.Read(ac.MinDistance);
						reader.Read(ac.MaxDistance);
						break;
					}
					case BinaryComponentType::UICanvas:
					{
						auto& uc = deserializedEntity.AddComponent<UICanvasComponent>();
						reader.Read(uc.Active);
						uint32_t renderMode = 0;
						reader.Read(renderMode);
						uc.RenderMode = (CanvasRenderMode)renderMode;
						reader.Read(uc.ReferenceResolution);
						break;
					}
					case BinaryComponentType::UITransform:
					{
						auto& ut = deserializedEntity.AddComponent<UITransformComponent>();
						reader.Read(ut.AnchorMin);
						reader.Read(ut.AnchorMax);
						reader.Read(ut.OffsetMin);
						reader.Read(ut.OffsetMax);
						reader.Read(ut.ZIndex);
						break;
					}
					case BinaryComponentType::UIImage:
					{
						auto& ui = deserializedEntity.AddComponent<UIImageComponent>();
						reader.Read(ui.TintColor);
						ui.TextureAssetPath = reader.ReadString();

						if (!ui.TextureAssetPath.empty())
							ui.Texture = Application::Get().GetSubsystem<AssetManager>()->GetTexture(ui.TextureAssetPath);
						break;
					}
					case BinaryComponentType::UIText:
					{
						auto& ut = deserializedEntity.AddComponent<UITextComponent>();
						ut.Text = reader.ReadString();
						std::string fontPath = reader.ReadString();
						if (!fontPath.empty())
							ut.FontAsset = CreateRef<Font>(fontPath);
						reader.Read(ut.Color);
						reader.Read(ut.FontSize);
						reader.Read(ut.LineSpacing);
						reader.Read(ut.Kerning);
						break;
					}
					case BinaryComponentType::UIButton:
					{
						auto& ub = deserializedEntity.AddComponent<UIButtonComponent>();
						reader.Read(ub.NormalColor);
						reader.Read(ub.HoverColor);
						reader.Read(ub.PressedColor);
						break;
					}
					default:
						break;
				}
			}

			if (primaryCameraID != 0 && uuid == primaryCameraID)
			{
				m_Scene->SetPrimaryCameraEntity(deserializedEntity);
			}
		}

		return true;
	}

}