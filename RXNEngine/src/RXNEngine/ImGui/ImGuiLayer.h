#pragma once

#include "RXNEngine/Core/Layer.h"

#include "RXNEngine/Events/Event.h"
#include "RXNEngine/Events/MouseEvent.h"
#include "RXNEngine/Events/KeyEvent.h"
#include "RXNEngine/Events/ApplicationEvent.h"

namespace RXNEngine {
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void Begin();
		void End();

		virtual void OnImGuiRenderer() override;

		void BlockEvents(bool block) { m_BlockEvents = block; }

		void SetDarkThemeColors();

		uint32_t GetActiveWidgetID() const;
	private:
		bool m_BlockEvents = true;
	};
}