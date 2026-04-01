#include "rxnpch.h"
#include "SDLWindow.h"
#include "SDLInput.h"
#include "RXNEngine/Events/ApplicationEvent.h"
#include "RXNEngine/Events/KeyEvent.h"
#include "RXNEngine/Events/MouseEvent.h"
#include "RXNEngine/Core/Application.h"

#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

namespace RXNEngine {

    static bool s_SDLInitialized = false;

    Window* Window::Create(const WindowProps& props)
    {
        return new SDLWindow(props);
    }

    SDLWindow::SDLWindow(const WindowProps& props)
    {
        Init(props);
    }

    SDLWindow::~SDLWindow() noexcept
    { 
        Shutdown();
    }

    void SDLWindow::Init(const WindowProps& props)
    {
        m_Data.Title = props.Title;
        m_Data.Width = props.Width;
        m_Data.Height = props.Height;

        if (!s_SDLInitialized)
        {
            bool success = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
            RXN_CORE_ASSERT(success, "Could not initialize SDL3!");
            s_SDLInitialized = true;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        uint32_t windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
        if (props.Mode == WindowMode::Fullscreen)
            windowFlags |= SDL_WINDOW_FULLSCREEN;
        else if (props.Mode == WindowMode::Borderless)
            windowFlags |= SDL_WINDOW_BORDERLESS;
        else if (props.Mode == WindowMode::Maximized)
            windowFlags |= SDL_WINDOW_MAXIMIZED;

        m_Window = SDL_CreateWindow(m_Data.Title.c_str(), (int)props.Width, (int)props.Height, windowFlags);
        RXN_CORE_ASSERT(m_Window, "Failed to create SDL Window!");

        m_Context = GraphicsContext::Create((void*)m_Window);
        m_Context->Init();

        SetVSync(true);
    }

    void SDLWindow::Shutdown()
    {
        SDL_DestroyWindow(m_Window);
        if (s_SDLInitialized)
        {
            SDL_Quit();
            s_SDLInitialized = false;
        }
    }

    void SDLWindow::OnUpdate()
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSDL3_ProcessEvent(&e);

            switch (e.type)
            {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                {
                    WindowCloseEvent event;
                    m_Data.EventCallback(event);
                    break;
                }
                case SDL_EVENT_WINDOW_RESIZED:
                {
                    m_Data.Width = e.window.data1;
                    m_Data.Height = e.window.data2;
                    WindowResizeEvent event(m_Data.Width, m_Data.Height);
                    m_Data.EventCallback(event);
                    break;
                }
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                {
                    uint32_t keycode = e.key.key;

                    if (keycode == 0) break;

                    if (e.type == SDL_EVENT_KEY_DOWN)
                    {
                        KeyPressedEvent event(keycode, e.key.repeat);
                        m_Data.EventCallback(event);
                    }
                    else
                    {
                        KeyReleasedEvent event(keycode);
                        m_Data.EventCallback(event);
                    }
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                {
                    int button = e.button.button;
                    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                    {
                        MouseButtonPressedEvent event(button);
                        m_Data.EventCallback(event);
                    }
                    else
                    {
                        MouseButtonReleasedEvent event(button);
                        m_Data.EventCallback(event);
                    }
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION:
                {
                    MouseMovedEvent event(e.motion.x, e.motion.y);
                    m_Data.EventCallback(event);
                    break;
                }
                case SDL_EVENT_MOUSE_WHEEL:
                {
                    MouseScrolledEvent event(e.wheel.x, e.wheel.y);
                    m_Data.EventCallback(event);
                    break;
                }
                case SDL_EVENT_GAMEPAD_ADDED:
                {
                    auto input = std::static_pointer_cast<SDLInput>(Application::Get().GetSubsystem<Input>());
                    if (input)
                        input->AddGamepad(e.gdevice.which);
                    break;
                }
                case SDL_EVENT_GAMEPAD_REMOVED:
                {
                    auto input = std::static_pointer_cast<SDLInput>(Application::Get().GetSubsystem<Input>());
                    if (input)
                        input->RemoveGamepad(e.gdevice.which);
                    break;
                }
            }
        }

        m_Context->SwapBuffers();
    }

    void SDLWindow::SetVSync(bool enabled)
    {
        SDL_GL_SetSwapInterval(enabled ? 1 : 0);
        m_Data.VSync = enabled;
    }

    void SDLWindow::SetCursorMode(CursorMode mode)
    {
        ImGuiIO& io = ImGui::GetIO();

        switch (mode)
        {
            case CursorMode::Normal:
                SDL_SetWindowRelativeMouseMode(m_Window, false);
                SDL_ShowCursor();

                io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
                io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
                break;

            case CursorMode::Hidden:
                SDL_SetWindowRelativeMouseMode(m_Window, false);
                SDL_HideCursor();

                io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
                io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
                break;

            case CursorMode::Locked:
                SDL_SetWindowRelativeMouseMode(m_Window, true);

                io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
                io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
                break;
        }
    }
}