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
int PickPhysicalDevice();
int	createLogicalDevice();

int IgnisSetupInternal(VkInstance* instance, VkSurfaceKHR* surface)
{
    printf("Start initing\n");
    window.instance = instance;
    window.surface = surface;

    if(!checkValidationLayerSupport()) return 1;
    setupDebugMessenger();


    PickPhysicalDevice();
	createLogicalDevice();

    return 0;
}

////////////Debugger/////////////
bool checkValidationLayerSupport() 
{
    Rat/*char**/ validationLayers;
    char* validationLayer = "VK_LAYER_KHRONOS_validation";
    IRat(&validationLayers, 1, sizeof(char*));
    IRatAdd(&validationLayer, &validationLayers);

    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    Rat/*VkLayerProperties*/ availableLayers;
    IRat(&availableLayers, layerCount, sizeof(VkLayerProperties));

    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data);
    IRatCheckSize(&availableLayers);

    for (size_t i = 0; i < validationLayers.Size; i++)
    {
        bool layerFound = false;

        char* layerName;
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
            IRatFree(&validationLayers);
            IRatFree(&availableLayers);

            return false;
        }    
    }
    IRatFree(&validationLayers);
    IRatFree(&availableLayers);

    return true;
}
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
{
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		fprintf(stderr, "validation layer: %s\n", pCallbackData->pMessage);
	}
    return VK_FALSE;
}
void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT* createInfo) 
{
    *createInfo = (VkDebugUtilsMessengerCreateInfoEXT){0};
    createInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo->pfnUserCallback = debugCallback;
}
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) 
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL) {
        func(instance, debugMessenger, pAllocator);
    }
}
int setupDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(&createInfo);

    if (CreateDebugUtilsMessengerEXT(*window.instance, &createInfo, NULL, &window.debugMessenger) != VK_SUCCESS) {
        return 1;
    }
    return 0;
}
/////////////////////////////////

////////Physical Device//////////
typedef struct {
    uint32_t value;
    int has_value;
} OptionalUInt32;
typedef struct {
    OptionalUInt32 graphicsFamily;
    OptionalUInt32 presentFamily;
} QueueFamilyIndices;
bool isComplete(QueueFamilyIndices* indices) 
{
    return indices->graphicsFamily.has_value && indices->presentFamily.has_value;
}

typedef struct  {
     VkSurfaceCapabilitiesKHR capabilities;
     Rat/*VkSurfaceFormatKHR*/ formats;
     Rat/*VkPresentModeKHR*/ presentModes;
}SwapChainSupportDetails;

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, *(window.surface), &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, *(window.surface), &formatCount, NULL);

    if (formatCount != 0) {
        IRat(&(details.formats), formatCount, sizeof(VkSurfaceFormatKHR));
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, *(window.surface), &formatCount, details.formats.data);
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, *(window.surface), &presentModeCount, NULL);
    if (presentModeCount != 0) {
        IRat(&(details.presentModes), presentModeCount, sizeof(VkPresentModeKHR));
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, *(window.surface), &presentModeCount, details.presentModes.data);
    }

    return details;
}
bool checkDeviceExtensionSupport(VkPhysicalDevice device) 
{
    Rat/*char**/ deviceExtensions;
    char* deviceExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    IRat(&deviceExtensions, 1, sizeof(char*));
    IRatAdd(&deviceExtension, &deviceExtensions);


    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);

    Rat/*VkExtensionProperties*/ availableExtensions;
    IRat(&availableExtensions,extensionCount, sizeof(VkExtensionProperties));

    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions.data);
    availableExtensions.Size = extensionCount;

    int fulfilled = 0;

    for (size_t i = 0; i < availableExtensions.Size; i++)
    {
        for (size_t j = 0; j < deviceExtensions.Size; j++)
        {
            VkExtensionProperties avail;
            VkExtensionProperties needed;

            IRatGet(&avail, &availableExtensions, i);
            IRatGet(&needed, &deviceExtensions, j);
            if (strcmp(avail, needed) == 0) {
                fulfilled++;
                break;
            }
        }        
    }

    IRatFree(&availableExtensions);
    IRatFree(&deviceExtensions);

    return (fulfilled == deviceExtensions.Size);
}
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) 
{
    QueueFamilyIndices indices;
    
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

    Rat/*VkQueueFamilyProperties*/ queueFamilies;
    IRat(&queueFamilies,queueFamilyCount, sizeof(VkQueueFamilyProperties));

    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data);
    IRatCheckSize(&queueFamilies);

    indices.graphicsFamily.has_value = 0;
    indices.presentFamily.has_value = 0;

    for (size_t i = 0; i < queueFamilies.Size; i++)
    {
        VkQueueFamilyProperties queueFamily;
        IRatGet(&queueFamily, &queueFamilies, i);

        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily.has_value = 1;
            indices.graphicsFamily.value = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, *(window.surface), &presentSupport);

        if (presentSupport) {
            indices.presentFamily.value = i;
            indices.presentFamily.has_value = 1;
        }

        if (isComplete(&indices)) {
            break;
        }
    }

    IRatFree(&queueFamilies);

    return indices;
}
bool isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;

    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        bool formatsEmpty, presentEmpty;
        IRatEmpty(&formatsEmpty,&swapChainSupport.formats);
        IRatEmpty(&presentEmpty,&swapChainSupport.presentModes);
        swapChainAdequate = !formatsEmpty && !presentEmpty;

        IRatFree(&(swapChainSupport.formats));
        IRatFree(&(swapChainSupport.presentModes));
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    return isComplete(&indices) && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}
/////////////////////////////////