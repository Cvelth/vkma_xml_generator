local templated = require "script/premake/template"
templated.third_party "third_party"

templated.language = "C++"
templated.cppdialect = "C++latest"
templated.manifest_path = ".manifest"
templated.vpaths = {
	{ ["precompiled"] = "**/precompiled/**.h*" },
	{ ["precompiled/source"] = "**/precompiled/**.c*" },
	{ ["include/detail"] = "include/detail/**.h*" },
	{ ["source/include/detail"] = "source/detail/**.h*" },
	{ ["source/include"] = "source/**.h*" },
	{ ["source/detail"] = "source/detail/**.c*" },
	{ ["source"] = "source/**.c*" },
	{ ["include"] = "include/**.h*" }
}

templated.workspace "vkma_xml"

project "doxygen"
	kind "Utility"
	prebuildcommands {
		"doxygen " .. _MAIN_SCRIPT_DIR .. "/doxygen/Vulkan-Headers",
		"doxygen " .. _MAIN_SCRIPT_DIR .. "/doxygen/VulkanMemoryAllocator",
		"doxygen " .. _MAIN_SCRIPT_DIR .. "/doxygen/vkma_bindings"
	}

templated.project "generator"
    templated.kind "ConsoleApp"
    templated.files ""
    targetdir "bin/%{cfg.system}_%{cfg.buildcfg}"
	links "doxygen"
	depends { "pugixml", "ctre" }
	