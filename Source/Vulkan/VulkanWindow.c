#include "IgnisInternal.h"

#define MAX_FRAMES_IN_FLIGHT 2
#define MAX_TEXTURES 1024

typedef struct VulkanWindow {
    GLFWwindow* window;                                                 //user free
    VkInstance* instance;                                               //user free
    VkDevice device;                                                    //freed
    VkPhysicalDevice physicalDevice;                                    //neednt free
    VkDebugUtilsMessengerEXT debugMessenger;                            //freed

    VkSurfaceKHR* surface;                                              //user free
    VkQueue graphicsQueue;                                              //neednt free
    VkQueue presentQueue;                                               //neednt free

    VkSwapchainKHR swapChain;                                           //freed
    VkFormat swapChainImageFormat;                                      //neednt free
    VkExtent2D swapChainExtent;                                         //neednt free
    Rat/*VkImage*/ swapChainImages;                                     //freed
    Rat/*VkImageView*/ swapChainImageViews;                             //freed
    Rat/*VkFramebuffer*/ swapChainFramebuffers;                         //freed       

    VkRenderPass renderPass;                                            //freed
    VkDescriptorSetLayout descriptorSetLayout;                          //freed
    VkPipelineLayout pipelineLayout;                                    //freed
    VkPipeline graphicsPipeline;                                        //freed

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];

    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];         //freed
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];         //freed
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];                       //freed

    VkBuffer vertexBuffer[MAX_FRAMES_IN_FLIGHT];                        //freed
    VkDeviceMemory vertexBufferMemory[MAX_FRAMES_IN_FLIGHT];            //freed
    void* mappedVertexData[MAX_FRAMES_IN_FLIGHT];                       //freed

    VkBuffer indexBuffer[MAX_FRAMES_IN_FLIGHT];                         //freed
    VkDeviceMemory indexBufferMemory[MAX_FRAMES_IN_FLIGHT];             //freed
    void* mappedIndexData[MAX_FRAMES_IN_FLIGHT];                        //freed

    VkBuffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];                      //freed
    VkDeviceMemory uniformBuffersMemory[MAX_FRAMES_IN_FLIGHT];          //freed
    void* uniformBuffersMapped[MAX_FRAMES_IN_FLIGHT];                   //freed

    VkDescriptorPool descriptorPool;                                    //freed
    VkDescriptorSet descriptorSets[MAX_FRAMES_IN_FLIGHT];               //neednt free            

    VkImage dummyImage;                                                 //freed
    VkDeviceMemory dummyImageMemory;                                    //freed
    Rat/*VkImage*/ textureImages;                                       //freed
    Rat/*VkDeviceMemory*/ textureImageMemorys;                          //freed
    VkImageView textureImageViews[MAX_FRAMES_IN_FLIGHT][MAX_TEXTURES];  //freed
    VkSampler textureSampler;                                           //freed
} VulkanWindow;

bool framebufferResized = false;
int8_t currentFrame = 0;

//main data!!!! very important!!!
Rat/*Vertex*/ vertices[MAX_FRAMES_IN_FLIGHT];
bool needVertexUpdate[MAX_FRAMES_IN_FLIGHT];
Rat/*uint16_t*/ indicies[MAX_FRAMES_IN_FLIGHT];
bool needIndexUpdate[MAX_FRAMES_IN_FLIGHT];
bool vertexBufferChanged = false;
bool indexBufferChanged = false;
VkDeviceSize currentOffset[MAX_FRAMES_IN_FLIGHT] = {0}; //vertexBuffer offset
VkDeviceSize totalVertexBufferSize[MAX_FRAMES_IN_FLIGHT];
VkDeviceSize totalIndexBufferSize[MAX_FRAMES_IN_FLIGHT];
VulkanWindow window  = {0};

int SetupDebugMessenger();
bool CheckValidationLayerSupport();
int PickPhysicalDevice();
int	CreateLogicalDevice();
int CreateSwapChain();
void CreateImageViews();
int CreateRenderPass();
int CreateGraphicsPipeline();
int CreateFramebuffers();
int CreateCommandPool();
int CreateVertexBuffer();
int CreateIndexBuffer();
int CreateUniformBuffers();
int_least64_t CreateDescriptorPool();
int CreateDescriptorSets();
int CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory);
int CreateTextureImage(const char* fileName, VkImage* texture, VkDeviceMemory* textureMemory);
int SetTextureIntoView(int viewIndex, VkImage imageToView);
int UpdateTextureDescritorSets(int imageViewIndex);
int CreateTextureImageView();
int CreateTextureSampler();
int CreateCommandBuffers();
int CreateSyncObjects();
int DrawFrame();

/*
VkImage monika;
VkDeviceMemory monikaMemory;
*/
int IgnisSetupInternal(VkInstance* instance, VkSurfaceKHR* surface, GLFWwindow* windowIn)
{
    printf("Started initialization\n");

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        totalVertexBufferSize[i] = 1000;
        totalIndexBufferSize[i] = 1000;
    }

    IRat(&window.textureImages, 0, sizeof(VkImage));
    IRat(&window.textureImageMemorys, 0, sizeof(VkDeviceMemory));

    IRat(&vertices[0], 100, sizeof(Vertex));
    IRat(&vertices[1], 100, sizeof(Vertex));
    IRat(&indicies[0], 100, sizeof(uint16_t));
    IRat(&indicies[1], 100, sizeof(uint16_t));

    Vertex nya[] = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, -1},
        {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, -1},
        {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, -1},
        {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, -1}
    };

    uint16_t nye[] = { 0, 2, 1, 0, 3, 2 };

    IRatAdd(&nya[0], &vertices[0]);
    IRatAdd(&nya[1], &vertices[0]);
    IRatAdd(&nya[2], &vertices[0]);
    IRatAdd(&nya[3], &vertices[0]);
    IRatAdd(&nya[0], &vertices[1]);
    IRatAdd(&nya[1], &vertices[1]);
    IRatAdd(&nya[2], &vertices[1]);
    IRatAdd(&nya[3], &vertices[1]);

    indicies[0].data = nye;
    IRatCheckSize(&indicies[0]);
    indicies[1].data = nye;
    IRatCheckSize(&indicies[1]);

    window.instance = instance;
    window.surface = surface;
    window.window = windowIn;

    if(!CheckValidationLayerSupport()) return 1;
    SetupDebugMessenger();

    int result = 0;

    result = PickPhysicalDevice();
    if(result) printf("PickPhysicalDevice error\n");
	result = CreateLogicalDevice();
    if(result) printf("CreateLogicalDevice error\n");
    result = CreateSwapChain();
    if(result) printf("CreateSwapChain error\n");
    CreateImageViews();
    result = CreateRenderPass();
    if(result) printf("CreateRenderPass error\n");

    result = CreateDescriptorSetLayout();
    if(result) printf("CreateDescriptorSetLayout error\n");
    result = CreateGraphicsPipeline();
    if(result) printf("CreateGraphicsPipeline error\n");
    result = CreateFramebuffers();
    if(result) printf("CreateFramebuffers error\n");
    result = CreateCommandPool();
    if(result) printf("CreateCommandPool error\n");
    result = CreateTextureImageView();
    if(result) printf("CreateTextureImageView error\n");
    result = CreateTextureSampler();
    if(result) printf("CreateTextureSampler error\n");
    result = CreateVertexBuffer();
    if(result) printf("CreateVertexBuffer error\n");
    result = CreateIndexBuffer();
    if(result) printf("CreateIndexBuffer error\n");
    result = CreateUniformBuffers();
    if(result) printf("CreateUniformBuffers error\n");

    result = CreateDescriptorPool();
    if(result) printf("CreateDescriptorPool error\n");
    result = CreateDescriptorSets();
    if(result) printf("CreateDescriptorSets error\n");

    result = CreateCommandBuffers();
    if(result) printf("CreateCommandBuffers error\n");
    result = CreateSyncObjects();
    if(result) printf("CreateSyncObjects error\n");

    printf("Finished initialization\n");
/*
    CreateTextureImage("monika.png\0", &monika, &monikaMemory);
    SetTextureIntoView(0, monika);
    UpdateTextureDescritorSets(0);
*/
    return 0;
}

void MainLoop() 
{
    while (!glfwWindowShouldClose(window.window)) {
        glfwPollEvents();
        if(DrawFrame()){
            printf("ajaj");
        }
    }
    vkDeviceWaitIdle(window.device);
    printf("Closing...\n");
}


/////////// Debugger ///////////
bool CheckValidationLayerSupport() 
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
void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT* createInfo) 
{
    *createInfo = (VkDebugUtilsMessengerCreateInfoEXT){0};
    createInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo->pfnUserCallback = debugCallback;
}
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    PFN_vkCreateDebugUtilsMessengerEXT func = 
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) 
{
    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != NULL) {
        func(instance, debugMessenger, pAllocator);
    }
}
int SetupDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    PopulateDebugMessengerCreateInfo(&createInfo);

    if (CreateDebugUtilsMessengerEXT(*window.instance, &createInfo, NULL, &window.debugMessenger) != VK_SUCCESS) {
        return 1;
    }
    return 0;
}
////////////////////////////////


// Physical & Logical Device //
typedef struct {
    uint32_t value;
    int has_value;
} OptionalUInt32;
typedef struct {
    OptionalUInt32 graphicsFamily;
    OptionalUInt32 presentFamily;
} QueueFamilyIndices;
bool IsComplete(QueueFamilyIndices* indices) 
{
    return indices->graphicsFamily.has_value && indices->presentFamily.has_value;
}

typedef struct  {
     VkSurfaceCapabilitiesKHR capabilities;
     Rat/*VkSurfaceFormatKHR*/ formats;
     Rat/*VkPresentModeKHR*/ presentModes;
}SwapChainSupportDetails;

SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) {
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
bool CheckDeviceExtensionSupport(VkPhysicalDevice device) 
{
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME };
    int requiredCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]);

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);

    Rat/*VkExtensionProperties*/ availableExtensions;
    IRat(&availableExtensions,extensionCount, sizeof(VkExtensionProperties));

    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions.data);
    availableExtensions.Size = extensionCount;

    int fulfilled = 0;

    for (size_t i = 0; i < availableExtensions.Size; i++)
    {
        for (size_t j = 0; j < requiredCount; j++)
        {
            VkExtensionProperties avail;
            const char* needed = deviceExtensions[j];
            IRatGet(&avail, &availableExtensions, i);

            if (strcmp(avail.extensionName, needed) == 0) {
                fulfilled++;
                break;
            }
        }        
    }

    IRatFree(&availableExtensions);

    return (fulfilled == requiredCount);
}
QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) 
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

        if (IsComplete(&indices)) {
            break;
        }
    }

    IRatFree(&queueFamilies);

    return indices;
}
bool IsDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = FindQueueFamilies(device);
    bool extensionsSupported = CheckDeviceExtensionSupport(device);

    bool swapChainAdequate = false;

    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
        bool formatsEmpty, presentEmpty;
        IRatEmpty(&formatsEmpty,&swapChainSupport.formats);
        IRatEmpty(&presentEmpty,&swapChainSupport.presentModes);
        swapChainAdequate = !formatsEmpty && !presentEmpty;

        IRatFree(&(swapChainSupport.formats));
        IRatFree(&(swapChainSupport.presentModes));
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    return IsComplete(&indices) && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

int PickPhysicalDevice() 
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(*window.instance, &deviceCount, NULL);

    if (deviceCount == 0) {
        fprintf(stderr, "Failed to find GPUs with Vulkan support!\n");
        return 1;
    }

    Rat/*VkPhysicalDevice*/ devices;
    IRat(&devices, deviceCount, sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(*window.instance, &deviceCount, devices.data);
    IRatCheckSize(&devices);

    for (size_t i = 0; i < devices.Size; i++)
    {
        VkPhysicalDevice device;
        IRatGet(&device, &devices, i);

        if (IsDeviceSuitable(device)) {
            window.physicalDevice = device;
            break;
        }
    }

    IRatFree(&devices);

    if (window.physicalDevice == VK_NULL_HANDLE) {
        fprintf(stderr, "Failed to find a suitable GPU!\n");
        return 1;
    }
    
    return 0;
}
int CreateLogicalDevice()
{
    QueueFamilyIndices indices = FindQueueFamilies(window.physicalDevice);
    Rat/*VkDeviceQueueCreateInfo*/ queueCreateInfos;

    uint32_t queueFamilies[2];
    uint32_t count = 0;

    if (indices.graphicsFamily.has_value) {
        queueFamilies[count++] = indices.graphicsFamily.value;
    }

    if (indices.presentFamily.has_value && indices.presentFamily.value != indices.graphicsFamily.value) {
        queueFamilies[count++] = indices.presentFamily.value;
    }

    IRat(&queueCreateInfos, count, sizeof(VkDeviceQueueCreateInfo));

    float queuePriority = 1.0f;

    for (uint32_t i = 0; i < count; i++)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {0};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilies[i];
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        IRatAdd(&queueCreateInfo, &queueCreateInfos);
    }

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {0};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    indexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;

    
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {0};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
    deviceFeatures2.pNext = &indexingFeatures;

    VkDeviceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.Size;
    createInfo.pQueueCreateInfos = queueCreateInfos.data;
    createInfo.pEnabledFeatures = NULL;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME };

    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    const char* validationLayers[] = { "VK_LAYER_KHRONOS_validation" };

    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = validationLayers;

    VkResult result = vkCreateDevice(window.physicalDevice, &createInfo, NULL, &window.device);
    if (result != VK_SUCCESS) {
        printf("Failed to create logical device!\n");
        return 1;
    }

    IRatFree(&queueCreateInfos);

    vkGetDeviceQueue(window.device, indices.graphicsFamily.value, 0, &window.graphicsQueue);
    vkGetDeviceQueue(window.device, indices.presentFamily.value, 0, &window.presentQueue);

    return 0;
}
///////////////////////////////


//// Swapchain Management /////
VkSurfaceFormatKHR ChooseSwapSurfaceFormat(Rat/*VkSurfaceFormatKHR*/ availableFormats) {
    VkSurfaceFormatKHR availableFormat;
    for (size_t i = 0; i < availableFormats.Size; i++)
    {
        IRatGet(&availableFormat, &availableFormats, i);
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    IRatGet(&availableFormat, &availableFormats, 0);
    return availableFormat;
}
VkPresentModeKHR ChooseSwapPresentMode(Rat/*VkPresentModeKHR*/ availablePresentModes) {
    VkPresentModeKHR availablePresentMode;
    for (size_t i = 0; i < availablePresentModes.Size; i++)
    {
        IRatGet(&availablePresentMode, &availablePresentModes, i);
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}
VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        glfwGetFramebufferSize(window.window, &width, &height);
        VkExtent2D actualExtent = {
            (uint32_t)width,
            (uint32_t)height
        };
        if (actualExtent.width < capabilities.minImageExtent.width)
            actualExtent.width = capabilities.minImageExtent.width;
        else if (actualExtent.width > capabilities.maxImageExtent.width)
            actualExtent.width = capabilities.maxImageExtent.width;

        if (actualExtent.height < capabilities.minImageExtent.height)
            actualExtent.height = capabilities.minImageExtent.height;
        else if (actualExtent.height > capabilities.maxImageExtent.height)
            actualExtent.height = capabilities.maxImageExtent.height;

        return actualExtent;
    }
}
int CreateSwapChain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(window.physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = *window.surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = FindQueueFamilies(window.physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value, indices.presentFamily.value };

    if (indices.graphicsFamily.value != indices.presentFamily.value) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = NULL;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(window.device, &createInfo, NULL, &window.swapChain) != VK_SUCCESS) {
        return 1;
    }
    vkGetSwapchainImagesKHR(window.device, window.swapChain, &imageCount, NULL);
    IRat(&window.swapChainImages, imageCount, sizeof(VkImage));
    vkGetSwapchainImagesKHR(window.device, window.swapChain, &imageCount, window.swapChainImages.data);
    IRatCheckSize(&window.swapChainImages);
    window.swapChainImageFormat = surfaceFormat.format;
    window.swapChainExtent = extent;

    return 0;
}
VkImageView CreateSwapChainImageView(VkImage image, VkFormat format) 
{
    VkImageViewCreateInfo viewInfo = {0};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(window.device, &viewInfo, NULL, &imageView) != VK_SUCCESS) {
        printf("imageview creation failed\n");
        return NULL;
    }
    return imageView;
}
void CreateImageViews() 
{
    IRat(&window.swapChainImageViews, window.swapChainImages.Size, sizeof(VkImageView));

    for (uint32_t i = 0; i < window.swapChainImages.Size; i++) {
        VkImage image;
        IRatGet(&image, &window.swapChainImages, i);
        VkImageView imageView = CreateSwapChainImageView(image, window.swapChainImageFormat);
        IRatSet(&imageView, &window.swapChainImageViews, i);
    }
    IRatCheckSize(&window.swapChainImageViews);
}
///////////////////////////////


//// Descriptor Management ////
int CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboLayoutBinding = {0};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = NULL;

    VkDescriptorSetLayoutBinding samplerLayoutBinding = {0};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = MAX_TEXTURES;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = NULL;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = { uboLayoutBinding, samplerLayoutBinding };

    VkDescriptorBindingFlags bindingFlags[2] = {
    0,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsInfo = {0};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.pBindingFlags = bindingFlags;
    bindingFlagsInfo.bindingCount = 2;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    layoutInfo.pNext = &bindingFlagsInfo;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    if (vkCreateDescriptorSetLayout(window.device, &layoutInfo, NULL, &window.descriptorSetLayout) != VK_SUCCESS) 
        return 1;
    
    return 0;
}
int_least64_t CreateDescriptorPool() {
    VkDescriptorPoolSize poolSizes[2] = {0};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT * MAX_TEXTURES;

    VkDescriptorPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(window.device, &poolInfo, NULL, &window.descriptorPool) != VK_SUCCESS) {
        return 1;
    }
    return 0;
}
int CreateDescriptorSets() {
    VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        layouts[i] = window.descriptorSetLayout;
    }

    uint32_t variableDescriptorCounts[MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        variableDescriptorCounts[i] = MAX_TEXTURES;
    }

    VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo = {0};
    countInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    countInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    countInfo.pDescriptorCounts = variableDescriptorCounts;

    VkDescriptorSetAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = window.descriptorPool;
    allocInfo.descriptorSetCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts;
    allocInfo.pNext = &countInfo;

    if (vkAllocateDescriptorSets(window.device, &allocInfo, window.descriptorSets) != VK_SUCCESS) {
        return 1;
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo = {0};
        bufferInfo.buffer = window.uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferData);

        VkDescriptorImageInfo imageInfos[MAX_TEXTURES];


        for (size_t x = 0; x < MAX_TEXTURES; x++)
        {
            imageInfos[x] = (VkDescriptorImageInfo){0};
            imageInfos[x].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[x].imageView = window.textureImageViews[i][x];
            imageInfos[x].sampler = window.textureSampler;
        }

        VkWriteDescriptorSet descriptorWrites[2] = {0};

        // Write the uniform buffer info
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = window.descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        // Write the image sampler info
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = window.descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = MAX_TEXTURES;
        descriptorWrites[1].pImageInfo = imageInfos;

        vkUpdateDescriptorSets(window.device, (uint32_t)2, descriptorWrites, 0, NULL);
    }

    return 0;
}
int UpdateTextureDescritorSets(int imageViewIndex)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorImageInfo newImageInfo = {
            .imageView = window.textureImageViews[i][imageViewIndex],
            .sampler = window.textureSampler,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = window.descriptorSets[i],
            .dstBinding = 1,
            .dstArrayElement = imageViewIndex,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &newImageInfo,
        };

        vkUpdateDescriptorSets(window.device, 1, &write, 0, NULL);
    }

    return 0;
}
///////////////////////////////


////// Render Management //////
int CreateRenderPass() 
{
    VkAttachmentDescription colorAttachment = {0};
    colorAttachment.format = window.swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {0};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    if (vkCreateRenderPass(window.device, &renderPassInfo, NULL, &window.renderPass) != VK_SUCCESS) {
        return 1;
    }
    return 0;
}
int createShaderModule(VkShaderModule* module, const char* code, size_t codeSize) {
    VkShaderModuleCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = (const uint32_t*)code;

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(window.device, &createInfo, NULL, &shaderModule) != VK_SUCCESS) {
        return 1;
    }
    *module = shaderModule;

    return 0;
}
int CreateGraphicsPipeline()
{
    char* vertShaderCode;
    char* fragShaderCode;
    size_t vertSize = ReadFile("vert.spv", &vertShaderCode);
    size_t fragSize = ReadFile("frag.spv", &fragShaderCode);

    VkShaderModule vertShaderModule, fragShaderModule;
    if(createShaderModule(&vertShaderModule, vertShaderCode, vertSize))
        return 1;
    if(createShaderModule(&fragShaderModule,fragShaderCode, fragSize))
        return 1;

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {0};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {0};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkVertexInputAttributeDescription attrDescs[4];
    GetAttributeDescriptions(attrDescs);
    VkVertexInputBindingDescription binding = GetBindingDescription();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = 4;
    vertexInputInfo.pVertexBindingDescriptions = &binding;
    vertexInputInfo.pVertexAttributeDescriptions = attrDescs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {0};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)window.swapChainExtent.width;
    viewport.height = (float)window.swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = window.swapChainExtent;

    VkDynamicState dynamicStates[] = {             
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {0};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState = {0};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {0};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;

    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {0};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = NULL;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending = {0};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &window.descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = NULL;

    if (vkCreatePipelineLayout(window.device, &pipelineLayoutInfo, NULL, &window.pipelineLayout) != VK_SUCCESS) {
        return 1;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {0};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = NULL;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = window.pipelineLayout;
    pipelineInfo.renderPass = window.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(window.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &window.graphicsPipeline) != VK_SUCCESS) {
        return 1;
    }

    vkDestroyShaderModule(window.device, fragShaderModule, NULL);
    vkDestroyShaderModule(window.device, vertShaderModule, NULL);

    free(vertShaderCode);
    free(fragShaderCode);

    return 0;
}
int CreateFramebuffers()
{
    IRat(&window.swapChainFramebuffers, window.swapChainImageViews.Size, sizeof(VkFramebuffer));

    for (size_t i = 0; i < window.swapChainImageViews.Size; i++) {
        VkImageView imageView;
        IRatGet(&imageView, &window.swapChainImageViews, i);

        VkImageView attachments[] = { imageView };

        VkFramebufferCreateInfo framebufferInfo = {0};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = window.renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = window.swapChainExtent.width;
        framebufferInfo.height = window.swapChainExtent.height;
        framebufferInfo.layers = 1;

        VkFramebuffer buffer;
        if (vkCreateFramebuffer(window.device, &framebufferInfo, NULL, &buffer) != VK_SUCCESS) {
            return 1;
        }
        IRatSet(&buffer, &window.swapChainFramebuffers, i);
    }
    IRatCheckSize(&window.swapChainFramebuffers);
    return 0;
}
int CreateCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(window.physicalDevice);
    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value;

    if (vkCreateCommandPool(window.device, &poolInfo, NULL, &window.commandPool) != VK_SUCCESS) {
        return 1;
    }
    return 0;
}
///////////////////////////////


///////// Buffers ////////////
uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) 
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(window.physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}
int CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) 
{
    VkBufferCreateInfo bufferInfo = {0};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(window.device, &bufferInfo, NULL, buffer) != VK_SUCCESS) {
        return 1;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(window.device, *buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(window.device, &allocInfo, NULL, bufferMemory) != VK_SUCCESS) {
        return 1;
    }

    vkBindBufferMemory(window.device, *buffer, *bufferMemory, 0);
    return 0;
}

int CreateVertexBuffer()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDeviceSize bufferSize = sizeof(Vertex) * totalVertexBufferSize[i];
        
        if(CreateBuffer(bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &window.vertexBuffer[i], &window.vertexBufferMemory[i]))
            return 1;
        
        vkMapMemory(window.device, window.vertexBufferMemory[i], 0, 
            bufferSize, 0, &window.mappedVertexData[i]);

        VkDeviceSize UpBufferSize = sizeof(Vertex) * vertices[i].Size;
        memcpy((char*)window.mappedVertexData[i] + currentOffset[i],
            vertices[i].data,UpBufferSize);
    }

    return 0;
}
void ResizeVertexBuffer(uint16_t frameIndex, VkDeviceSize newSize) {
    vkDestroyBuffer(window.device, window.vertexBuffer[frameIndex], NULL);
    vkFreeMemory(window.device, window.vertexBufferMemory[frameIndex], NULL);

    CreateBuffer(newSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &window.vertexBuffer[frameIndex], &window.vertexBufferMemory[frameIndex]);

    vkMapMemory(window.device, window.vertexBufferMemory[frameIndex], 0,
        newSize, 0, &window.mappedVertexData[frameIndex]);

    totalVertexBufferSize[frameIndex] = newSize;
}
//To change vertecies, set the data in vertices and set the vertexBufferChanged flag 
void UpdateVertexBuffer() {
    if(vertexBufferChanged){
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            needVertexUpdate[i] = true;  
    }
    int updateFrame = (currentFrame+1)%MAX_FRAMES_IN_FLIGHT;
    if(needVertexUpdate[updateFrame]){
        VkDeviceSize bufferSize = sizeof(Vertex) * vertices[updateFrame].Size;
        if(bufferSize*2 < totalVertexBufferSize[updateFrame]/2){
            ResizeVertexBuffer(updateFrame, totalVertexBufferSize[updateFrame]/2);
        }
        if (bufferSize > totalVertexBufferSize[updateFrame]/2) {
            ResizeVertexBuffer(updateFrame, bufferSize*2);
        }   
        if (currentOffset[updateFrame] + bufferSize > totalVertexBufferSize[updateFrame]) {
            currentOffset[updateFrame] = 0;
        }
        memcpy((char*)window.mappedVertexData[updateFrame] + currentOffset[updateFrame], vertices[updateFrame].data, bufferSize);
        currentOffset[updateFrame] += bufferSize;
    }
}

int CreateIndexBuffer()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDeviceSize bufferSize = sizeof(uint16_t) * totalIndexBufferSize[i];
            
        if(CreateBuffer(bufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &window.indexBuffer[i], &window.indexBufferMemory[i]))
            return 1;
        
        vkMapMemory(window.device, window.indexBufferMemory[i], 0, 
            bufferSize, 0, &window.mappedIndexData[i]);
        
        VkDeviceSize UpBufferSize = sizeof(uint16_t) * indicies[i].Size;
        memcpy((char*)window.mappedIndexData[i],
            indicies[i].data,UpBufferSize);
    }
    return 0;
}
void ResizeIndexBuffer(uint16_t frameIndex, VkDeviceSize newSize) {
    vkDestroyBuffer(window.device, window.indexBuffer[frameIndex], NULL);
    vkFreeMemory(window.device, window.indexBufferMemory[frameIndex], NULL);

    CreateBuffer(newSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &window.indexBuffer[frameIndex], &window.indexBufferMemory[frameIndex]);

    vkMapMemory(window.device, window.indexBufferMemory[frameIndex], 0, newSize, 0, &window.mappedIndexData[frameIndex]);
    totalIndexBufferSize[frameIndex] = newSize;
}
//To change indicies, set the data in indicies and set the indexBufferChanged flag 
void UpdateIndexBuffer() {
    if(indexBufferChanged){
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            needIndexUpdate[i] = true;  
    }
    int updateFrame = (currentFrame+1)%MAX_FRAMES_IN_FLIGHT;
    if(needVertexUpdate[updateFrame]){
        VkDeviceSize bufferSize = sizeof(uint16_t) * indicies[updateFrame].Size;
        
        if(bufferSize*2 < totalIndexBufferSize[updateFrame]/2){
            ResizeIndexBuffer(updateFrame, totalIndexBufferSize[updateFrame]/2);
        }
        else if (bufferSize > totalIndexBufferSize[updateFrame]/2) {
            ResizeIndexBuffer(updateFrame, bufferSize*2);
        }  

        memcpy((char*)window.mappedVertexData[updateFrame], indicies[updateFrame].data, bufferSize);
    }
}

//does not use uniform buffer at the moment
int CreateUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferData);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            &window.uniformBuffers[i], &window.uniformBuffersMemory[i]);
        vkMapMemory(window.device, window.uniformBuffersMemory[i], 0, bufferSize, 0, &window.uniformBuffersMapped[i]);
    }

    return 0;
}
void UpdateUniformBuffer(uint32_t currentImage) {
    UniformBufferData ubo = {0};
    memcpy(window.uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}
//////////////////////////////


///// Command Management //////
VkCommandBuffer BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = window.commandPool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(window.device, &allocInfo, &commandBuffer);
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}
void EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(window.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(window.graphicsQueue);
    vkFreeCommandBuffers(window.device, window.commandPool, 1, &commandBuffer);
}
///////////////////////////////


///// Texture Management /////
int TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        return 1;
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    EndSingleTimeCommands(commandBuffer);

    return 0;
}
void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
    VkBufferImageCopy region = {0};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){ 0, 0, 0 };
    region.imageExtent = (VkExtent3D){ width, height, 1 };
    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
    EndSingleTimeCommands(commandBuffer);
}

int CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory) {
    VkImageCreateInfo imageInfo = {0};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(window.device, &imageInfo, NULL, image) != VK_SUCCESS) {
        return 1;
    }
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(window.device, *image, &memRequirements);
    VkMemoryAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);
    if (vkAllocateMemory(window.device, &allocInfo, NULL, imageMemory) != VK_SUCCESS) {
        return 1;
    }
    vkBindImageMemory(window.device, *image, *imageMemory, 0);

    return 0;
}
int CreateTextureImage(const char* fileName, VkImage* texture, VkDeviceMemory* textureMemory) // be nullterminated
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(fileName, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;
    if (!pixels) {
        return 1;
    }
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);
    void* data;
    vkMapMemory(window.device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, (uint32_t)imageSize);
    vkUnmapMemory(window.device, stagingBufferMemory);
    stbi_image_free(pixels);
    CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture, textureMemory);

    TransitionImageLayout(*texture, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(stagingBuffer, *texture, (uint32_t)texWidth, (uint32_t)texHeight);
    TransitionImageLayout(*texture, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkDestroyBuffer(window.device, stagingBuffer, NULL);
    vkFreeMemory(window.device, stagingBufferMemory, NULL);

    return 0;
}
VkImageView CreateImageView(VkImage image, VkFormat format) {
    VkImageViewCreateInfo viewInfo = {0};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VkImageView imageView;
    if (vkCreateImageView(window.device, &viewInfo, NULL, &imageView) != VK_SUCCESS) {
        printf("ImageView creation failed");
    }
    return imageView;
}
int SetTextureIntoView(int viewIndex, VkImage imageToView)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        window.textureImageViews[i][viewIndex] = CreateImageView(imageToView, VK_FORMAT_R8G8B8A8_SRGB);
    }
    return 0;
}

int CreateTextureImageView()
{
    uint8_t whitePixel[4] = { 0, 255, 255, 255 };
    VkDeviceSize imageSize = 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer, &stagingBufferMemory);

    void* data;
    vkMapMemory(window.device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, whitePixel, 4);
    vkUnmapMemory(window.device, stagingBufferMemory);

    CreateImage(1, 1, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &window.dummyImage, &window.dummyImageMemory);

    TransitionImageLayout(window.dummyImage, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    CopyBufferToImage(stagingBuffer, window.dummyImage, 1, 1);

    TransitionImageLayout(window.dummyImage, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(window.device, stagingBuffer, NULL);
    vkFreeMemory(window.device, stagingBufferMemory, NULL);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        for (size_t x = 0; x < MAX_TEXTURES; x++)
        {
            window.textureImageViews[i][x] = CreateImageView(window.dummyImage, VK_FORMAT_R8G8B8A8_SRGB);
        }
    }
    return 0;
}
int CreateTextureSampler()
{
    VkPhysicalDeviceProperties properties = {0};
    vkGetPhysicalDeviceProperties(window.physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo = {0};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    if (vkCreateSampler(window.device, &samplerInfo, NULL, &window.textureSampler) != VK_SUCCESS) {
        return 1;
    }

    return 0;
}
//////////////////////////////


// Command & Draw Management //
int CreateCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = window.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(window.device, &allocInfo, window.commandBuffers) != VK_SUCCESS) {
        return 1;
    }
    return 0;
}
int CreateSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo = {0};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {0};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(window.device, &semaphoreInfo, NULL, &window.imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(window.device, &semaphoreInfo, NULL, &window.renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(window.device, &fenceInfo, NULL, &window.inFlightFences[i]) != VK_SUCCESS) {
            return 1;
        }
    }
    return 0;
}

void CleanupSwapChain() {
    for (size_t i = 0; i < window.swapChainFramebuffers.Size; i++) {
        VkFramebuffer buffer;
        IRatGet(&buffer, &window.swapChainFramebuffers, i);
        vkDestroyFramebuffer(window.device, buffer, NULL);
    }
    for (size_t i = 0; i < window.swapChainImageViews.Size; i++) {
        VkImageView imageView;
        IRatGet(&imageView, &window.swapChainImageViews, i);
        vkDestroyImageView(window.device, imageView, NULL);
    }
    vkDestroySwapchainKHR(window.device, window.swapChain, NULL);
}
void RecreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window.window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window.window, &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(window.device);
    CleanupSwapChain();
    CreateSwapChain();
    CreateImageViews();
    CreateFramebuffers();
}

int RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = NULL; 

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        printf("failed to begin recording command buffer!");
        return 1;
    }

    VkRenderPassBeginInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = window.renderPass;
    VkFramebuffer buffer;
    IRatGet(&buffer, &window.swapChainFramebuffers, imageIndex);
    renderPassInfo.framebuffer = buffer;
    renderPassInfo.renderArea.offset.x = 0;
    renderPassInfo.renderArea.offset.y = 0;
    renderPassInfo.renderArea.extent = window.swapChainExtent;
    VkClearValue clearColor = {0};
    clearColor.color.float32[0] = 0.1f;
    clearColor.color.float32[1] = 0.1f;
    clearColor.color.float32[2] = 0.1f;
    clearColor.color.float32[3] = 1.0f;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, window.graphicsPipeline);

    VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)window.swapChainExtent.width;
    viewport.height = (float)window.swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = window.swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    VkBuffer vertexBuffers[] = { window.vertexBuffer[currentFrame] };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, window.indexBuffer[currentFrame], 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, window.pipelineLayout, 0, 1, &window.descriptorSets[currentFrame], 0, NULL);
    vkCmdDrawIndexed(commandBuffer, (uint32_t)indicies[currentFrame].Size, 1, 0, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        printf("failed to record command buffer!");
        return 1;
    }

    return 0;
}
int DrawFrame() 
{
    vkWaitForFences(window.device, 1, &window.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(window.device, window.swapChain, UINT64_MAX, window.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapChain();
        return 1;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        printf("failed to acquire swap chain image!");
        return 1;
    }
    vkResetFences(window.device, 1, &window.inFlightFences[currentFrame]);
    vkResetCommandBuffer(window.commandBuffers[currentFrame], 0);
    RecordCommandBuffer(window.commandBuffers[currentFrame], imageIndex);
    //updateUniformBuffer(currentFrame);

    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = { window.imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &window.commandBuffers[currentFrame];
    VkSemaphore signalSemaphores[] = { window.renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    if (vkQueueSubmit(window.graphicsQueue, 1, &submitInfo, window.inFlightFences[currentFrame]) != VK_SUCCESS) {
        printf("failed to submit draw command buffer!");
        return 1;
    }
    VkPresentInfoKHR presentInfo = {0};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapChains[] = { window.swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = NULL;
    result = vkQueuePresentKHR(window.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        RecreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        printf("failed to present swap chain image!");
        return 1;
    }
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    return 0;
}
///////////////////////////////


//// Cleanup on shutdown /////
int DestroySwapchain()
{
    CleanupSwapChain();

    IRatFree(&window.swapChainImages);
    IRatFree(&window.swapChainImageViews);
    IRatFree(&window.swapChainFramebuffers);

    return 0;
}
int DestroyImageViews()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        for (size_t x = 0; x < MAX_TEXTURES; x++)
        {
            vkDestroyImageView(window.device, window.textureImageViews[i][x], NULL);
        }
        
    }
    return 0;
}
int DestroyTextureImages()
{
    vkDestroyImage(window.device, window.dummyImage, NULL);
    vkFreeMemory(window.device, window.dummyImageMemory, NULL);

    for (size_t i = 0; i < window.textureImages.Size; i++)
    {
        VkImage textureImage;
        IRatGet(&textureImage, &window.textureImages, i);
        vkDestroyImage(window.device, textureImage, NULL);

        VkDeviceMemory textureImageMemory;
        IRatGet(&textureImageMemory, &window.textureImageMemorys, i);
        vkFreeMemory(window.device, textureImageMemory, NULL);
    }

    IRatFree(&window.textureImages);
    IRatFree(&window.textureImageMemorys);

    return 0;
}
int DestoryUniformBuffers()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if(window.uniformBuffersMapped[i] != NULL){
            vkUnmapMemory(window.device, window.uniformBuffersMemory[i]);
            window.uniformBuffersMapped[i] = NULL;
        }

        vkDestroyBuffer(window.device, window.uniformBuffers[i], NULL);
        vkFreeMemory(window.device, window.uniformBuffersMemory[i], NULL);
    }

    return 0;
}
int DestroyDescriptorItems()
{
    vkDestroyDescriptorPool(window.device, window.descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(window.device, window.descriptorSetLayout, NULL);

    return 0;
}
int DestroyShaderBuffers()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if(window.mappedVertexData[i] != NULL){
            vkUnmapMemory(window.device, window.vertexBufferMemory[i]);
            window.mappedVertexData[i] = NULL;
        }

        vkDestroyBuffer(window.device, window.vertexBuffer[i], NULL);
        vkFreeMemory(window.device, window.vertexBufferMemory[i], NULL);


        if(window.mappedIndexData[i] != NULL){
            vkUnmapMemory(window.device, window.indexBufferMemory[i]);
            window.mappedIndexData[i] = NULL;
        }

        vkDestroyBuffer(window.device, window.indexBuffer[i], NULL);
        vkFreeMemory(window.device, window.indexBufferMemory[i], NULL);
    }

    return 0;  
}
int DestroyRender()
{
    vkDestroyPipeline(window.device, window.graphicsPipeline, NULL);
    vkDestroyPipelineLayout(window.device, window.pipelineLayout, NULL);

    vkDestroyRenderPass(window.device, window.renderPass, NULL);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(window.device, window.renderFinishedSemaphores[i], NULL);
        vkDestroySemaphore(window.device, window.imageAvailableSemaphores[i], NULL);
        vkDestroyFence(window.device, window.inFlightFences[i], NULL);
    }

    return 0;
}
int DestroyVulkanData()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        IRatFree(&vertices[i]);
        IRatFree(&indicies[i]);
    }

    return 0;
}

int IgnisShutdownInternal()
{
    DestroySwapchain();

    vkDestroySampler(window.device, window.textureSampler, NULL);
    DestroyImageViews();
    DestroyTextureImages();

    DestoryUniformBuffers();
    DestroyDescriptorItems();
    DestroyShaderBuffers(); // vertex & index

    DestroyRender();

    vkDestroyCommandPool(window.device, window.commandPool, NULL);

    vkDestroyDevice(window.device, NULL);

    DestroyDebugUtilsMessengerEXT(*window.instance, window.debugMessenger, NULL);

    DestroyVulkanData();

    return 0;
}
//////////////////////////////

