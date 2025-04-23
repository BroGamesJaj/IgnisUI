#include "IgnisInternal.h"
#include "Ignis.h"

typedef struct VulkanWindow {
    VkInstance instance;
    VkDevice device; //Logical device
    VkPhysicalDevice physicalDevice; //VK_NULL_HANDLE

    VkDebugUtilsMessengerEXT debugMessenger;   

    VkSurfaceKHR surface;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    <VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
};