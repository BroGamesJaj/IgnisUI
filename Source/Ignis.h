#pragma once

#include <stdio.h>
#include "vulkan/vulkan.h"

#ifdef __cplusplus
namespace ignis_internal 
{
	extern "C" {
#endif


	int IgnisSetup(VkInstance* instance, VkSurfaceKHR* surface);


#ifdef __cplusplus
	}
}
#endif




#ifdef __cplusplus
class Ignis {
public:
	static void IgnisSetup(VkInstance* instance, VkSurfaceKHR* surface) {
		ignis_internal::IgnisSetup(instance, surface);
	}
};
#endif