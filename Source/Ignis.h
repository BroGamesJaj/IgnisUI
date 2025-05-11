#pragma once

#define GLFW_INCLUDE_NONE

#ifdef APIENTRY
#undef APIENTRY
#endif

#include "vulkan/vulkan.h" // Vulkan header
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include <stdio.h>

#ifdef __cplusplus
namespace ignis_internal 
{
	extern "C" {
#endif


	void IgnisSetup(VkInstance* instance, VkSurfaceKHR* surface, GLFWwindow* windowIn);


#ifdef __cplusplus
	}
}
#endif

#ifdef __cplusplus
class Ignis {
public:
	static void IgnisSetup(VkInstance* instance, VkSurfaceKHR* surface, GLFWwindow* windowIn) {
		ignis_internal::IgnisSetup(instance, surface, windowIn);
	}
};
#endif