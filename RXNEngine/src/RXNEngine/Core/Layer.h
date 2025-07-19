#pragma once

#include "RXNEngine/Core/Core.h"
#include "RXNEngine/Events/Event.h"

namespace RXNEngine {

	class RXN_API Layer
	{
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(float deltaTime) {}
		virtual void OnFixedUpdate(float fixedDeltaTime) {}
		virtual void OnEvent(Event& event) {}
		virtual void OnImGuiRenderer() {}

		inline const std::string& GetName() const { return m_DebugName; }
	protected:
		std::string m_DebugName;
	};

}