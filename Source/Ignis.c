#include "IgnisInternal.h"
#include "Ignis.h"

int IgnisSetup(VkInstance* instance, VkSurfaceKHR* surface) {
    return IgnisSetupInternal(instance, surface);
}