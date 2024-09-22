workspace "lethe"

flags {"NoBufferSecurityCheck"}
cppdialect "c++17"

exceptionhandling("off")
rtti("off")
configurations {"Debug", "Release"}
platforms {"x86", "x64", "x86_shared", "x64_shared"}

filter "platforms:x86"
	kind "StaticLib"
	architecture "x32"
	includedirs {"."}

filter "platforms:x64"
	kind "StaticLib"
	architecture "x64"
	includedirs {"."}

filter "platforms:x86_shared"
	kind "SharedLib"
	architecture "x32"
	includedirs {"."}
	defines {"LETHE_DYNAMIC", "LETHE_BUILD"}

filter "platforms:x64_shared"
	kind "SharedLib"
	architecture "x64"
	includedirs {"."}
	defines {"LETHE_DYNAMIC", "LETHE_BUILD"}

project "lethe"
	--files {"Lethe/Lethe_SCU*.cpp"}

	-- uncomment the above and comment these two lines for full SCU build
	files { "**.cpp", "**.h", "**.inl" }
	excludes {"Lethe/Lethe_SCU*.cpp"}

	filter "action:vs*"
		files { "**.natvis" }

	-- this is wrong actually should be when using Microsoft compiler only! but clang with ms codegen seems broken anyway
	filter "action:vs*"
		flags { "MultiProcessorCompile" }
		files { "Lethe/Script/Vm/JitX86/X64StubWin.asm" }

	filter "system:windows"
		links {"winmm"}

	-- mingw needs to link ws2_32
	filter {"system:windows", "toolset:gcc"}
		links {"ws2_32"}

	filter "configurations:Debug"
		defines{"_DEBUG", "DEBUG"}
		symbols "On"

	filter "configurations:Release"
		defines("NDEBUG")
		optimize "On"
		symbols "Off"
		omitframepointer "On"
