#include "IgnisInternal.h"
#include "Ignis.h"

const int MAX_FRAMES_IN_FLIGHT = 2;

typedef struct VulkanWindow {
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkDebugUtilsMessengerEXT debugMessenger;   

    VkSurfaceKHR surface;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    Rat/*VkImage*/ swapChainImages;
    Rat/*VkImageView*/ swapChainImageViews;
    Rat/*VkFramebuffer*/ swapChainFramebuffers;

    VkRenderPass renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkCommandPool commandPool;
    Rat/*VkCommandBuffer*/ commandBuffers;

    Rat/*VkSemaphore*/ imageAvailableSemaphores;
    Rat/*VkSemaphore*/ renderFinishedSemaphores;
    Rat/*VkFence*/ inFlightFences;
};

bool framebufferResized = false;
int8_t currentFrame = 0;

