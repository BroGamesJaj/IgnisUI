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

VulkanWindow window  = {0};

int setupDebugMessenger();
bool CheckValidationLayerSupport();
int PickPhysicalDevice();
int	CreateLogicalDevice();
int CreateSwapChain();
void CreateImageViews();
int CreateRenderPass();
int CreateGraphicsPipeline();

int IgnisSetupInternal(VkInstance* instance, VkSurfaceKHR* surface)
{
    printf("Start initing\n");
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
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
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

    VkDeviceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.Size;
    createInfo.pQueueCreateInfos = queueCreateInfos.data;

    createInfo.pEnabledFeatures = &deviceFeatures;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    createInfo.enabledExtensionCount = 1;
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

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const Rat/*VkSurfaceFormatKHR*/ availableFormats) {
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
VkPresentModeKHR chooseSwapPresentMode(const Rat/*VkPresentModeKHR*/ availablePresentModes) {
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
    createInfo.surface = window.surface;

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
        VkImageView imageView = createImageView(image, window.swapChainImageFormat);
        IRatSet(&imageView, &window.swapChainImageViews, i);
    }

    return 0;
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
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = NULL;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = { uboLayoutBinding, samplerLayoutBinding };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(window.device, &layoutInfo, NULL, &window.descriptorSetLayout) != VK_SUCCESS) 
        return 1;
    
    return 0;
}

int CreateGraphicsPipeline()
{
    return 0;
}

