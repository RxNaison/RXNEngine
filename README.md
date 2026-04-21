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




### The Master Roadmap

**Phase 1: Core Engine Architecture V1 [✅ COMPLETED]**
*   **Service Locator Pattern:** Engine and Scene subsystems are strictly decoupled (Application owns Audio/Input/Global Physics; Scene owns PhysicsWorld/AI).
*   **Deterministic Teardown:** Explicit RAII and ordered loop destruction to prevent C++ memory leaks and ucrtbase.dll crashes.
*   **Modern C# Interop:** CoreCLR loaded, managed components bridged, unmanaged callbacks registered, and the Time Class / Coroutines implemented.
*   **Extended Input:** Cross-platform Factory Pattern for Gamepads and Mouse polling.

**Phase 2: Asset & Material Pipeline [✅ COMPLETED]**
*   **Managed Workspace:** The engine extracts embedded .gltf textures and physically saves them to the project directory, removing dependencies on external user folders.
*   **Decoupled Material Assets:** ModelImporter strips materials from .gltf and generates standalone .rxnmat YAML files.
*   **Binary Cache Upgrades:** .rxn model files bumped to V3. They store asset pointers instead of raw embedded data to prevent memory duplication.
*   **Path Normalization:** `std::filesystem` deployed across the AssetManager to prevent cache misses.
*   **Standalone Material Editor:** Dedicated MaterialEditorPanel with live preview, texture drag-and-drop, and automatic memory revert on window close.

**Phase 3: The Prefab & Hierarchy System [✅ COMPLETED]**
*   **Intrusive Reference Counting:** `std::shared_ptr` stripped from core assets and replaced with a custom `Ref<T>` utilizing `RefCounted` base classes to eliminate control-block heap allocations.
*   **Prefab Serialization:** PrefabSerializer implemented to extract a Root Entity and its children and save them as an independent .rxnprefab YAML file.
*   **Prefab Deserialization:** Logic implemented to safely spawn .rxnprefab entity trees with fresh UUIDs while preserving internal parent-child relationship arrays.
*   **Submesh Modification:** Bypassed monolithic .gltf limits, allowing users to delete specific submeshes and save the customized result as a reusable asset.

**Phase 4: Low-Level Refactoring [⏳ IN PROGRESS]**
*   **Lock-Free Job System:** Rip out the global `std::mutex` and `std::queue`. Implement a Chase-Lev work-stealing deque architecture.
    *   **[🔥 HOTFIX]:** Change `m_IsRunning` to `std::atomic<bool>` to fix worker thread data races.
    *   **[🔥 HOTFIX]:** Fix the Shutdown Use-After-Free by explicitly joining JobSystem threads *before* `AssetManager` is destroyed.
    *   **[🚀 OPTIMIZATION]:** Replace the CPU-burning `std::this_thread::yield()` spin-lock in `Wait()` with a `std::condition_variable`.
*   **Custom Memory Allocators:** Replace global OS-level `new`/`delete` calls. Implement Linear, Pool, and Stack Allocators.
    *   **[🔥 HOTFIX]:** Patch the `AssetManager::Clear()` memory leak by explicitly clearing the Texture and Material maps on shutdown.
    *   **[🔥 HOTFIX]:** Fix the `Scene::RemoveEntity` double-free crash by replacing `std::vector<Entity> m_EntitiesToDestroy` with a `std::unordered_set`.
    *   **[🔧 REFACTOR]:** Wrap raw `RendererData* m_Data` pointers in RAII smart pointers.
*   **GUID Asset Registry:** Strip out `std::string` hashing in the AssetManager. Map all assets to 64-bit integer GUIDs.
    *   **[🔥 HOTFIX]:** Fix the `UUID()` generation data race by making `std::mt19937_64` a `thread_local` variable.
*   **Data-Oriented Transforms:** Convert `Scene::UpdateWorldTransforms` from pointer-chasing OOP to a Struct of Arrays (SoA) layout.
    *   **[🔥 HOTFIX]:** Replace integer-based `SDL_GetTicks()` with `SDL_GetPerformanceCounter()` to eradicate high-framerate micro-stutters and physics jitter.
*   **Parallel Radix Render Sorting:** Replace the `std::sort` bottleneck on the main thread with a multithreaded Radix sort for rendering queues.
    *   **[🚀 OPTIMIZATION]:** Stop sorting heavy 120-byte `RenderCommandPacket` structs; sort an array of pointers/indices instead to prevent CPU cache thrashing.
*   **[🔧 REFACTOR] Renderer Thread Safety:** Eradicate the `static InstanceData` arrays in `ExecuteQueue` and `FlushShadows`. Move them into the `RendererData` struct so the pipeline is fully reentrant and thread-safe.
*   **[🔧 REFACTOR] Subsystem DRY:** Create a generic `SubsystemRegistry` base class to eliminate the duplicated boilerplate template code currently sitting in both `Application.h` and `Scene.h`. Fix the Linux include casing bug (`RXNengine` -> `RXNEngine`).

**Phase 5: Physics & Scripting Polish**
*   **[🚀 OPTIMIZATION] C# Reflection Purge:** *MASSIVE* speedup. Remove `MethodInfo.Invoke` inside `InvokeOnUpdate`/`InvokeOnFixedUpdate`. Cast the instance directly (`((Entity)instance).InternalUpdate();`) to eliminate per-frame GC allocation and reflection overhead.
*   **[🔥 HOTFIX] C# CoreCLR Memory Leak:** Implement a `ClearInstances()` unmanaged callback to flush the C# `s_EntityInstances` dictionary when Play Mode stops, allowing the .NET GC to actually clean up scripts.
*   **[🔥 HOTFIX] Child Entity Physics Sync:** In `Scene::SyncTransformToPhysics`, decompose the *World Transform* and pass that to PhysX `setGlobalPose`, rather than blindly passing the local `Translation`.
*   **[🔧 REFACTOR] PhysX Multithreading:** Replace the hardcoded `PxDefaultCpuDispatcherCreate(2)` with `std::thread::hardware_concurrency() - 1` to unlock AAA physics simulation scale.
*   **Physics Materials:** Extract static/dynamic friction and restitution out of the Collider components and into standalone .rxnphys asset files.
*   **Advanced Collisions:** Integrate V-HACD to automatically slice complex meshes into dozens of perfect convex physics hulls.
*   **PhysX Character Controller (CCT):** A proper player capsule that slides along walls and walks up stairs natively.
*   **Script Variables UI:** Support for dragging and dropping Prefabs or Entities into C# public `Entity Target;` fields within the editor.

**Phase 6: Graphics Pipeline**
*   **[🔥 HOTFIX] PBR IBL Resolution Bug:** Remove the hardcoded `u_Resolution = 512.0;` in `ibl_prefilter.glsl`. Pass it as a uniform from `OpenGLCubemap` so custom HDR skies don't result in broken, noisy specular reflections.
*   **[🔧 REFACTOR] Dynamic Render Targets:** Remove the hardcoded `1280x720` magic numbers in `SceneRenderer::Init()`.
*   **Post-Processing Pipeline:** ACES Filmic Tonemapping, Gamma Correction, and Auto-Exposure.
*   **Cascaded Shadow Maps (CSM):** Crisp shadows near the camera, performant low-res shadows in the distance.
*   **Screen Space Reflections (SSR):** Real-time puddle reflections and glossy surfaces.
*   **Screen Space Ambient Occlusion (SSAO):** Contact shadows in corners and crevices.
*   **Volumetric Lighting (God Rays):** Sun shafts bleeding through the geometry.
*   **Translucency / Subsurface Scattering:** Light passing through leaves, skin, and wax.

**Phase 7: World Building & Life**
*   **Frustum & Occlusion Culling:** Preventing the GPU from rendering objects hidden behind walls.
*   **Terrain System:** Heightmaps, sculpting, and chunk-based LODs.
*   **Foliage & Grass Instancer:** Rendering 100,000 blades of grass with a single draw call.
*   **Wind & Vegetation Shaders:** Moving the foliage using sine waves in the vertex shader.
*   **Animation System:** Skeletal meshes and bone matrices.
*   **Particle System:** Compute-shader driven GPU particles.
*   **Audio Engine:** FMOD integration for 3D spatial sound.
