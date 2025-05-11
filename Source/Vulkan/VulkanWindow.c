#include "IgnisInternal.h"

#define MAX_FRAMES_IN_FLIGHT 2
#define MAX_TEXTURES 1024

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

    VkBuffer vertexBuffer[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory vertexBufferMemory[MAX_FRAMES_IN_FLIGHT];
    void* mappedVertexData[MAX_FRAMES_IN_FLIGHT];

    VkBuffer indexBuffer[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory indexBufferMemory[MAX_FRAMES_IN_FLIGHT];
    void* mappedIndexData[MAX_FRAMES_IN_FLIGHT];

    VkBuffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory uniformBuffersMemory[MAX_FRAMES_IN_FLIGHT];
    void* uniformBuffersMapped[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSets[MAX_FRAMES_IN_FLIGHT];

    Rat/*VkImage*/ textureImage;
    Rat/*VkDeviceMemory*/ textureImageMemory;
    VkImageView textureImageViews[MAX_FRAMES_IN_FLIGHT][MAX_TEXTURES];
    VkSampler textureSampler;
} VulkanWindow;

bool framebufferResized = false;
int8_t currentFrame = 0;

//main data!!!! very important!!!;
Rat/*Vertex*/ vertices[MAX_FRAMES_IN_FLIGHT];
bool needVertexUpdate[MAX_FRAMES_IN_FLIGHT];
Rat/*uint16_t*/ indicies[MAX_FRAMES_IN_FLIGHT];
bool needIndexUpdate[MAX_FRAMES_IN_FLIGHT];
bool vertexBufferChanged = false;
bool indexBufferChanged = false;
VkDeviceSize currentOffset[MAX_FRAMES_IN_FLIGHT] = {0}; //vertexBuffer offset
VkDeviceSize totalVertexBufferSize[MAX_FRAMES_IN_FLIGHT];
VkDeviceSize totalIndexBufferSize[MAX_FRAMES_IN_FLIGHT];
//now, not so important stuff

VulkanWindow window  = {0};

VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription = {0};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}
void getAttributeDescriptions(VkVertexInputAttributeDescription* out) {
    out[0].binding = 0;
    out[0].location = 0;
    out[0].format = VK_FORMAT_R32G32_SFLOAT;
    out[0].offset = offsetof(Vertex, pos);

    out[1].binding = 0;
    out[1].location = 1;
    out[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    out[1].offset = offsetof(Vertex, color);

    out[2].binding = 0;
    out[2].location = 2;
    out[2].format = VK_FORMAT_R32G32_SFLOAT;
    out[2].offset = offsetof(Vertex, uv);

    out[3].binding = 0;
    out[3].location = 3;
    out[3].format = VK_FORMAT_R32_SINT;
    out[3].offset = offsetof(Vertex, textureIndex);
}

int setupDebugMessenger();
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

int IgnisSetupInternal(VkInstance* instance, VkSurfaceKHR* surface)
{
    printf("Start initing\n");

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        totalVertexBufferSize[i] = 1000;
        totalIndexBufferSize[i] = 1000;
    }

    window.instance = instance;
    window.surface = surface;

    if(!CheckValidationLayerSupport()) return 1;
    setupDebugMessenger();

    PickPhysicalDevice();
	CreateLogicalDevice();

    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();

    CreateDescriptorSetLayout();

    CreateGraphicsPipeline();

    CreateFramebuffers();
    CreateCommandPool();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateUniformBuffers();

    return 0;
}

////////////Debugger/////////////
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

////////Physical & Logical Device//////////
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

        if (isDeviceSuitable(device)) {
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
    QueueFamilyIndices indices = findQueueFamilies(window.physicalDevice);
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

    VkPhysicalDeviceFeatures deviceFeatures = {0};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {0};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    indexingFeatures.runtimeDescriptorArray = VK_TRUE;
    indexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
    indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    indexingFeatures.pNext = NULL;

    
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {0};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
    deviceFeatures2.pNext = &indexingFeatures;

    VkDeviceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.Size;
    createInfo.pQueueCreateInfos = queueCreateInfos.data;

    createInfo.pEnabledFeatures = &deviceFeatures;

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

VkSurfaceFormatKHR chooseSwapSurfaceFormat(Rat/*VkSurfaceFormatKHR*/ availableFormats) {
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
VkPresentModeKHR chooseSwapPresentMode(Rat/*VkPresentModeKHR*/ availablePresentModes) {
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
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
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
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(window.physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

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

    QueueFamilyIndices indices = findQueueFamilies(window.physicalDevice);
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
    IRat(&(window.swapChainImages), imageCount, sizeof(VkImage));
    vkGetSwapchainImagesKHR(window.device, window.swapChain, &imageCount, window.swapChainImages.data);
    window.swapChainImageFormat = surfaceFormat.format;
    window.swapChainExtent = extent;

    return 0;
}

VkImageView CreateImageView(VkImage image, VkFormat format) 
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
    IRatAlloc(&window.swapChainImageViews, window.swapChainImages.Size);

    for (uint32_t i = 0; i < window.swapChainImages.Size; i++) {
        VkImage image;
        IRatGet(&image, &window.swapChainImages, i);
        VkImageView imageView = CreateImageView(image, window.swapChainImageFormat);
        IRatSet(&imageView, &window.swapChainImageViews, i);
    }
}
/////////////////////////////////
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

size_t readFile(const char* filename, char** buffer) {
    FILE* file = fopen(filename, "rb");
    if (!file) return 0;

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    *buffer = (char*)malloc(fileSize);
    if (!*buffer) {
        fclose(file);
        return 0;
    }

    fread(*buffer, 1, fileSize, file);
    fclose(file);

    return fileSize;
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
    size_t vertSize = readFile("vert.spv", &vertShaderCode);
    size_t fragSize = readFile("frag.spv", &fragShaderCode);

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
    getAttributeDescriptions(attrDescs);
    VkVertexInputBindingDescription binding = getBindingDescription();

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

    return 1;
}

int CreateCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(window.physicalDevice);
    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value;

    if (vkCreateCommandPool(window.device, &poolInfo, NULL, &window.commandPool) != VK_SUCCESS) {
        return 1;
    }
    return 0;
}

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) 
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
int createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) 
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
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

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
        
        if(createBuffer(bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &window.vertexBuffer[i], &window.vertexBufferMemory[i]))
            return 1;
        
        vkMapMemory(window.device, window.vertexBufferMemory[i], 0, 
            bufferSize, 0, &window.mappedVertexData[i]);
    }

    return 0;
}
void ResizeVertexBuffer(uint16_t frameIndex, VkDeviceSize newSize) {
    vkDestroyBuffer(window.device, window.vertexBuffer[frameIndex], NULL);
    vkFreeMemory(window.device, window.vertexBufferMemory[frameIndex], NULL);

    createBuffer(newSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &window.vertexBuffer[frameIndex], &window.vertexBufferMemory[frameIndex]);

    vkMapMemory(window.device, window.vertexBufferMemory[frameIndex], 0,
        newSize, 0, &window.mappedVertexData[frameIndex]);

    totalVertexBufferSize[frameIndex] = newSize;
}
void updateVertexBuffer() {
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
            
        if(createBuffer(bufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &window.indexBuffer[i], &window.indexBufferMemory[i]))
            return 1;
        
        vkMapMemory(window.device, window.indexBufferMemory[i], 0, 
            bufferSize, 0, &window.mappedIndexData[i]);
    }
    return 0;
}
void ResizeIndexBuffer(uint16_t frameIndex, VkDeviceSize newSize) {
    vkDestroyBuffer(window.device, window.indexBuffer[frameIndex], NULL);
    vkFreeMemory(window.device, window.indexBufferMemory[frameIndex], NULL);

    createBuffer(newSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &window.indexBuffer[frameIndex], &window.indexBufferMemory[frameIndex]);

    vkMapMemory(window.device, window.indexBufferMemory[frameIndex], 0, newSize, 0, &window.mappedIndexData[frameIndex]);
    totalIndexBufferSize[frameIndex] = newSize;
}
void updateIndexBuffer() {
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

int CreateUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferData);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            &window.uniformBuffers[i], &window.uniformBuffersMemory[i]);
        vkMapMemory(window.device, window.uniformBuffersMemory[i], 0, bufferSize, 0, &window.uniformBuffersMapped[i]);
    }

    return 0;
}
void updateUniformBuffer(uint32_t currentImage) {
    UniformBufferData ubo = {0};
    memcpy(window.uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

int_least64_t createDescriptorPool() {
    VkDescriptorPoolSize poolSizes[2];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = MAX_TEXTURES;

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

int createDescriptorSets() {
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

    IRat(&window.descriptorSets, MAX_FRAMES_IN_FLIGHT, sizeof(VkDescriptorSet));

    if (vkAllocateDescriptorSets(window.device, &allocInfo, &window.descriptorSets) != VK_SUCCESS) {
        return 1;
    }

    return 0;
}

int UpdateDescritorSets()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo = {0};
        bufferInfo.buffer = window.uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferData);

        VkDescriptorImageInfo imageInfos[MAX_TEXTURES];

        for (size_t x = 0; x < MAX_TEXTURES; x++)
        {
            imageInfos[x].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[x].imageView = window.textureImageViews[i][x];
            imageInfos[x].sampler = window.textureSampler;
        }
        


        VkWriteDescriptorSet descriptorWrites[2];

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
        descriptorWrites[1].pImageInfo = &imageInfos;

        vkUpdateDescriptorSets(window.device, (uint32_t)2, descriptorWrites, 0, NULL);
    }

    return 0;
}

void cleanupVertexBuffer() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkUnmapMemory(window.device, window.vertexBufferMemory[i]);
        vkDestroyBuffer(window.device, window.vertexBuffer[i], NULL);
        vkFreeMemory(window.device, window.vertexBufferMemory[i], NULL);
    }
    

}






