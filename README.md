# RXNEngine

![Platform](https://img.shields.io/badge/Platform-Windows-blue)
![Language](https://img.shields.io/badge/Language-C++17-green)
![Scripting](https://img.shields.io/badge/Scripting-C%23_.NET_Core-purple)

RXNEngine is a custom, high-performance 3D game engine built in C++. It features a modern Entity-Component-System (ECS) architecture, physically based rendering (PBR), and a deeply integrated C# scripting environment powered by CoreCLR

## Screenshots
<img width="2560" height="1528" alt="image" src="https://github.com/user-attachments/assets/cf5eb282-add5-4022-a55b-80a16b95e060" />
<img width="1280" height="764" alt="Editor Screenshot 1" src="https://github.com/user-attachments/assets/64b8a6bf-eb95-4c18-8308-30049892f1e2" />
<img width="1280" height="764" alt="Editor Screenshot 2" src="https://github.com/user-attachments/assets/008f537b-f7b5-4ce8-91bb-275c7e463dc1" />

## Key Features
- **Modern Rendering:** PBR pipeline using OpenGL
- **Robust ECS:** Fast and flexible Entity-Component-System using EnTT
- **Advanced Physics:** AAA-grade physics integration using NVIDIA PhysX (Rigidbodies, Colliders, Raycasting, and Event Callbacks)
- **C# Scripting Engine:** Native-to-managed bridge using .NET CoreCLR, allowing gameplay logic to be written entirely in C# with hot-reloading support
- **ImGui Editor:** A fully featured, drag-and-drop interactive UI editor with a scene hierarchy panel and properties inspector
- **Asset Management:** Integrated system for managing meshes, materials, and textures

## The Scripting API

RXNEngine allows you to write clean, component-driven gameplay code in C# that communicates seamlessly with the C++ backend:

```csharp
using RXNEngine;

public class Player : Entity
{
    public float Speed = 5.0f;
    public float JumpForce = 50.0f;
    private bool m_JumpRequested = false;

    public override void OnUpdate(float deltaTime)
    {
        Vector3 velocity = new Vector3(0, 0, 0);

        if (Input.IsKeyDown(KeyCode.W))
            velocity += this.Forward;
        if (Input.IsKeyDown(KeyCode.S))
             velocity -= this.Forward;
        if (Input.IsKeyDown(KeyCode.Space))
             m_JumpRequested = true;

        this.Translation += velocity * Speed * deltaTime;
    }

    public override void OnFixedUpdate(float fixedDeltaTime)
    {
        if (m_JumpRequested)
        {
            this.ApplyLinearImpulse(this.Up * JumpForce);
            m_JumpRequested = false;
        }
    }

    public override void OnCollisionEnter(Entity other)
    {
        Console.WriteLine($"Collided with Entity ID: {other.ID}");
    }
}
```

## Repository Structure
* `RXNEngine/` - The core C++ engine runtime and subsystems.
* `RXNEditor/` - The C++ ImGui-based visual editor application.
* `RXNScriptCore/` - The base C# API library (Entity, Vector3, Physics, etc.).
* `RXNScriptApp/` - The C# project where your actual game scripts (like `Player.cs`) live.

## Getting Started

### Prerequisites
Before building the engine, ensure you have the following installed:
* Visual Studio 2022 (with C++ Desktop Development)
* .NET SDK (for the CoreCLR C# scripting bridge)
* Git

### Windows Build Instructions

1.  Clone the repository recursively to pull in all submodules:
```bash
git clone --recursive https://github.com/RxNaison/RXNEngine
cd RXNEngine
```
2.  Run the library setup script to unpack and configure vendor binaries:
```bash
PreBuildLibraries.bat
```
3.  Generate the Visual Studio project files (using Premake):
```bash
GenerateProjectScript.bat
```
4.  Open the generated `.sln` file in Visual Studio, set the Editor as the startup project, and hit **Build**.

## Tech Stack & Dependencies

  * **Core:** C++17, .NET CoreCLR
  * **Math:** GLM
  * **ECS:** EnTT
  * **Physics:** NVIDIA PhysX
  * **Windowing/Input:** GLFW
  * **UI:** Dear ImGui

## Roadmap

**Supported Platforms**
- [x] Windows
- [ ] Linux (Arch/Wayland support planned)
- [ ] macOS
- [ ] Android
- [ ] iOS

**Rendering APIs**
- [x] OpenGL
- [ ] Vulkan
- [ ] DirectX 12
- [ ] Metal
