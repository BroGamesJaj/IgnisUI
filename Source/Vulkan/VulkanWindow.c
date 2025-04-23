#include "IgnisInternal.h"

const int MAX_FRAMES_IN_FLIGHT = 2;

typedef struct VulkanWindow {
    VkInstance* instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkDebugUtilsMessengerEXT debugMessenger;   

    VkSurfaceKHR* surface;
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

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    Rat/*VkBuffer*/ uniformBuffers;
    Rat/*VkDeviceMemory*/ uniformBuffersMemory;
    Rat/*void**/ uniformBuffersMapped;

    VkDescriptorPool descriptorPool;
    Rat/*VkDescriptorPool*/ descriptorSets;

    Rat/*VkImage*/ textureImage;
    Rat/*VkDeviceMemory*/ textureImageMemory;
    Rat/*VkImageView*/ textureImageView;
    Rat/*VkSampler*/ textureSampler;
} VulkanWindow;

bool framebufferResized = false;
int8_t currentFrame = 0;

VulkanWindow window;

int setupDebugMessenger();
bool checkValidationLayerSupport();

Rat validationLayers;
char* validationLayer = "VK_LAYER_KHRONOS_validation";

int IgnisSetupInternal(VkInstance* instance, VkSurfaceKHR* surface)
{
    window.instance = instance;
    window.surface = surface;

    IRat(&validationLayers, 1, sizeof(char*));
    IRatAdd(validationLayer, &validationLayers);

    if(!checkValidationLayerSupport()) return 1;

    prinf("Suppordet stuff works");

    return 0;
}

bool checkValidationLayerSupport() 
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);

    Rat/*VkLayerProperties*/ availableLayers;
    IRat(&availableLayers, layerCount, sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data);

    for (size_t i = 0; i < validationLayers.Size; i++)
    {
        bool layerFound = false;
        const char* layerName;
        IRatGet(&layerName, &validationLayers, i);
        for (size_t j = 0; j < availableLayers.Size; j++)
        {
            VkLayerProperties propLayerName;
            IRatGet(&propLayerName, &availableLayers, j);
            if (strcmp(layerName, propLayerName.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            return false;
        }    
    }
    return true;
}
/*
int setupDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(*window.instance, &createInfo, NULL, &window.debugMessenger) != VK_SUCCESS) {
        return 1;
    }
    return 0;
}*/