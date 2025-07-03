workspace "RXNEngine"

   language "C++"
   architecture "x64"
   startproject "Sandbox"
   configurations { "Debug", "Release", "Dist" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["GLFW"] = "RXNEngine/vendor/GLFW/include"
IncludeDir["Glad"] = "RXNEngine/vendor/Glad/include"
IncludeDir["Imgui"] = "RXNEngine/vendor/imgui"
IncludeDir["glm"] = "RXNEngine/vendor/glm"

group "Dependencies"
    include "RXNEngine/vendor/GLFW"
    include "RXNEngine/vendor/Glad"
    include "RXNEngine/vendor/imgui"

group ""

project "RXNEngine"
   location "RXNEngine"
   kind "SharedLib"
   language "C++"
   staticruntime "On"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

   pchheader "rxnpch.h"
   pchsource "RXNEngine/src/rxnpch.cpp"

   files
   {
       "%{prj.name}/src/**.h",
       "%{prj.name}/src/**.cpp",
       "%{prj.name}/vendor/glm/glm/**.hpp",
       "%{prj.name}/vendor/glm/glm/**.inl",
       "%{prj.name}/vendor/glm/glm/**.h"
   }

   includedirs
   { 
       "%{prj.name}/src",
       "%{prj.name}/vendor/spdlog/include",
       "%{IncludeDir.GLFW}",
       "%{IncludeDir.Glad}",
       "%{IncludeDir.Imgui}",
       "%{IncludeDir.glm}"
   }



   links { "GLFW", "Glad", "Imgui", "opengl32.lib", "dwmapi.lib" }

   filter "system:windows"
      cppdialect "C++latest"
      systemversion "latest"
      defines { "RXN_PLATFORM_WINDOWS", "RXN_BUILD_DLL", "GLFW_INCLUDE_NONE" }

      postbuildcommands { "{COPYFILE} %{cfg.buildtarget.relpath} \"../bin/" .. outputdir .. "/Sandbox/\"" }

      filter "configurations:Debug"
        defines "RXN_DEBUG"
        runtime "Debug"
        symbols "On"

      filter "configurations:Release"
        defines "RXN_DEBUG"
        runtime "Release"
        optimize "On"

      filter "configurations:Dist"
        defines "RXN_DEBUG"
        runtime "Release"
        optimize "On"

project "Sandbox"
 location "RXNEngine"
   kind "ConsoleApp"
   language "C++"
   staticruntime "Off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

   files { "%{prj.name}/src/**.h", "%{prj.name}/src/**.cpp" }
   includedirs
   {
       "RXNEngine/vendor/spdlog/include",
       "RXNEngine/src",
       "%{IncludeDir.glm}"
   }
   links "RXNEngine"

   filter "system:windows"
      cppdialect "C++latest"
      systemversion "latest"
      defines "RXN_PLATFORM_WINDOWS"

   filter "configurations:Debug"
      defines "RXN_DEBUG"
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines "RXN_DEBUG"
      runtime "Release"
      optimize "On"

   filter "configurations:Dist"
      defines "RXN_DEBUG"
      runtime "Release"
      optimize "On"