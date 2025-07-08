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
   kind "StaticLib"
   language "C++"
   cppdialect "C++latest"
   staticruntime "off"

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

   	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
        "GLFW_INCLUDE_NONE",
        "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING"
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


   links { "GLFW", "Glad", "Imgui", "opengl32.lib" }

   filter "system:windows"
      systemversion "latest"
      defines { "RXN_PLATFORM_WINDOWS", "RXN_BUILD_DLL", "GLFW_INCLUDE_NONE" }

      filter "configurations:Debug"
        defines "RXN_DEBUG"
        runtime "Debug"
        symbols "on"
        defines { "RXN_ENABLE_ASSERTS" }

      filter "configurations:Release"
        defines "RXN_RELEASE"
        runtime "Release"
        optimize "on"

      filter "configurations:Dist"
        defines "RXN_DIST"
        runtime "Release"
        optimize "on"

project "Sandbox"
 location "RXNEngine"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++latest"
   staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

   files { "%{prj.name}/src/**.h", "%{prj.name}/src/**.cpp" }

    defines
	{
        "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING"
	}

   includedirs
   {
       "RXNEngine/vendor/spdlog/include",
       "RXNEngine/src",
       "%{IncludeDir.glm}"
   }
   links "RXNEngine"

   filter "system:windows"
      systemversion "latest"
      defines "RXN_PLATFORM_WINDOWS"

   filter "configurations:Debug"
      defines "RXN_DEBUG"
      runtime "Debug"
      symbols "on"

   filter "configurations:Release"
      defines "RXN_RELEASE"
      runtime "Release"
      optimize "on"

   filter "configurations:Dist"
      defines "RXN_DIST"
      runtime "Release"
      optimize "on"