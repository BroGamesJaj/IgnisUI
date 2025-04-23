#include "Ignis.h"
#include "IgnisInternal.h"

int IgnisSetup(VkInstance* instance, VkSurfaceKHR* surface) {
    return IgnisSetupInternal(instance, surface);
}