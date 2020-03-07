workspace "lethe_sample"

configurations {"Debug", "Release"}
platforms {"x86", "x64", "x86_shared", "x64_shared"}

filter "platforms:x86"
	architecture "x32"
	includedirs {"."}
	includedirs {"../src"}

filter "platforms:x64"
	architecture "x64"
	includedirs {"."}
	includedirs {"../src"}

filter "platforms:x86_shared"
	architecture "x32"
	includedirs {"../src"}
	defines {"LETHE_DYNAMIC"}

filter "platforms:x64_shared"
	architecture "x64"
	includedirs {"../src"}
	defines {"LETHE_DYNAMIC"}

-- this is dumb, I wonder if there's a better way...

filter {"platforms:x86", "configurations:Debug"}
	libdirs {"../src/bin/x86/Debug"}

filter {"platforms:x86", "configurations:Release"}
	libdirs {"../src/bin/x86/Release"}

filter {"platforms:x64", "configurations:Debug"}
	libdirs {"../src/bin/x64/Debug"}

filter {"platforms:x64", "configurations:Release"}
	libdirs {"../src/bin/x64/Release"}

filter {"platforms:x86_shared", "configurations:Debug"}
	libdirs {"../src/bin/x86_shared/Debug"}

filter {"platforms:x86_shared", "configurations:Release"}
	libdirs {"../src/bin/x86_shared/Release"}

filter {"platforms:x64_shared", "configurations:Debug"}
	libdirs {"../src/bin/x64_shared/Debug"}

filter {"platforms:x64_shared", "configurations:Release"}
	libdirs {"../src/bin/x64_shared/Release"}

-- project:

project "lethe_sample"
	kind "ConsoleApp"

	links {"lethe"}

	files {"main.cpp"}

	filter "system:windows"
		links {"winmm"}

	filter "configurations:Debug"
		defines{"_DEBUG", "DEBUG"}
		symbols "On"

	filter "configurations:Release"
		defines("NDEBUG")
		optimize "On"
		symbols "Off"
