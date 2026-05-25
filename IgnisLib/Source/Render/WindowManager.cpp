#include <cstring>

#include "../IgnisLib.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace Ignis {

////////////////////////
////    Structs    /////
////////////////////////

struct SwapChainSupportDetails {
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

////////////////////////
//////    Data    //////
////////////////////////

class Render::WindowManager {
   private:
    VkInstance vulkan;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkSurfaceKHR surface;

    QueueFamilyIndices queueFamilyIndicies;
    SwapChainSupportDetails swapChainSupport;

    bool enableValidationLayers = false;
    const std::vector<const char *> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    VkDebugUtilsMessengerEXT debugMessenger;

    const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME
    };

    std::unordered_set<GLFWwindow *> windows;

    ////////////////////////
    ////   Functions   /////
    ////////////////////////

    GLFWwindow *CreateTmpSurface() {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        GLFWwindow *windowOut = glfwCreateWindow(1, 1, "tmp", nullptr, nullptr);
        if (!windowOut) {
            const char *desc = nullptr;
            int code = glfwGetError(&desc);
            std::cerr << "GLFW window creation failed. Code: " << code
                      << " Message: " << (desc ? desc : "unknown") << std::endl;
            return nullptr;
        }

        VkSurfaceKHR surfaceOut;
        VkResult res = glfwCreateWindowSurface(vulkan, windowOut, nullptr, &surfaceOut);
        if (res != VK_SUCCESS) {
            std::cerr << "Vulkan surface creation failed. VkResult: " << res << std::endl;
            glfwDestroyWindow(windowOut);
            return nullptr;
        }

        surface = surfaceOut;

        return windowOut;
    }

    std::vector<const char *> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

   public:
    // maytodo: this needs to be split to getqueuefamilies and find, we dont need to get it every time cause physical device doesn't change
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount);
        std::for_each(queueFamilies.begin(), queueFamilies.end(), [](VkQueueFamilyProperties2 &p) { p.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2; });
        vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies) {
            if (!indices.isMinimumComplete()) {
                if (queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    indices.graphics.family = i;
                    indices.graphics.index = 0;
                }

                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

                if (presentSupport) {
                    indices.present.family = i;
                    indices.present.index = 0;
                }
            }

            if (!(queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                indices.compute.family = i;
                indices.compute.index = 0;
            }

            if (!(queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !(queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) && (queueFamily.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
                indices.transfer.family = i;
                indices.transfer.index = 0;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }
        // if there is no transfer q yet, then see if compute's family has additional q that can be transfer
        if (!indices.transfer.isFamilySet() && indices.compute.isFamilySet() && queueFamilies[indices.compute.family].queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT && queueFamilies[indices.compute.family].queueFamilyProperties.queueCount >= 2) {
            indices.transfer.family = indices.compute.family;
            indices.transfer.index = 1;
        }

        // if no unique compute or transfer,
        // set compute and transfer to graphics q's family
        if (!indices.isComplete() && indices.isMinimumComplete()) {
            if (queueFamilies[indices.graphics.family].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                indices.compute.family = indices.graphics.family;
                indices.compute.index = indices.graphics.index;
            }
            if (queueFamilies[indices.graphics.family].queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                indices.transfer.family = indices.graphics.family;
                indices.transfer.index = indices.graphics.index;
            }
        }
        if (!indices.isMinimumComplete()) {
            throw std::runtime_error("Couldn't find valid queues!");
        }

        queueFamilyIndicies = indices;
    }
    QueueFamilyIndices GetQueueFamilies() {
        return queueFamilyIndicies;
    };

   private:
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto &extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

   public:
    SwapChainSupportDetails QuerySwapChainSupport() { return swapChainSupport; };

   private:
    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = FindQueueFamilies(device);

        bool extensionsSupported = CheckDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            swapChainSupport = QuerySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.isMinimumComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    }
    void PickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(vulkan, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(vulkan, &deviceCount, devices.data());

        for (const auto &device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : validationLayers) {
            bool layerFound = false;

            for (const auto &layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
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
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
        std::string msg = pCallbackData->pMessage;

        // we dont talk about the api version around here
        if (msg.find("is older than the application specified API version") != std::string::npos) return VK_FALSE;

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }
    void SetupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(vulkan, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void CreateLogicalDevice() {
        // get requested queues
        std::vector<QueueFamilyIndices::QueueInfo *> queueInfos = {
            &queueFamilyIndicies.graphics,
            &queueFamilyIndicies.present,
            &queueFamilyIndicies.compute,
            &queueFamilyIndicies.transfer
        };

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        std::unordered_map<uint32_t, uint32_t> queueFamilies{};
        for (auto &qI : queueInfos) {
            auto [it, inserted] = queueFamilies.insert({ qI->family, qI->index + 1 });
            if (!inserted) {
                it->second = std::max(it->second, qI->index + 1);
            }
        }

        float queuePriority = 1.0f;
        for (auto [queueFamily, queueCount] : queueFamilies) {
            // std::cout << "qf: " << queueFamily << " qC: " << queueCount << "\n";
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = queueCount;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // features like what we requested from the physical device
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features{};
        robustness2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
        robustness2Features.nullDescriptor = VK_TRUE;

        VkPhysicalDeviceVulkan12Features features12{};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        features12.descriptorIndexing = VK_TRUE;
        features12.runtimeDescriptorArray = VK_TRUE;
        features12.descriptorBindingPartiallyBound = VK_TRUE;
        features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
        features12.pNext = &robustness2Features;

        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{};
        dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        dynamicRenderingFeature.dynamicRendering = VK_TRUE;
        dynamicRenderingFeature.pNext = &features12;

        // main device creation struct
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        createInfo.pNext = &dynamicRenderingFeature;

        // not needed in newer vulkan versions, but can be set for compatibility
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        // creating the device and getting a handle for the queue
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }
    }

    void CreateInstance() {
        // check if debugging is possible if needed
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Ignis Rendering";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 4, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 4, 0);
        appInfo.apiVersion = VK_API_VERSION_1_4;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // getting extensions
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // enabling debugging
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);

            createInfo.pNext = &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &vulkan) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

   public:
    WindowManager(bool debuging) {
#ifndef IGNIS_INPUT
        glfwInit();
#endif

        if (debuging) enableValidationLayers = true;

        // basicly the whole system, the connection between the app and the vulkan api
        CreateInstance();

        // creating the messennger if the debug layer is enabled
        SetupDebugMessenger();

        GLFWwindow *window = CreateTmpSurface();

        // basicly selects the "GPU"
        PickPhysicalDevice();

        // creates the "computing" part of the instance, stuff get done with this
        CreateLogicalDevice();

        // command pool is managing the memory used for the command buffers
        // CreateCommandPools();

        vkDestroySurfaceKHR(vulkan, surface, nullptr);
        surface = VK_NULL_HANDLE;
        glfwDestroyWindow(window);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

        std::cout << "Instance Successfuly initialized" << std::endl;
    }

    GLFWwindow *Open(int width, int height, const char *title, GLFWmonitor *monitor, GLFWwindow *share) {
        GLFWwindow *windowOut;
        return glfwCreateWindow(width, height, title, monitor, share);
    }

    void Close(Window window) {
        if (windows.contains(window.ptr)) {
            glfwDestroyWindow(window.ptr);
        }
    }

    void Cleanup() {
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(vulkan, debugMessenger, nullptr);
        }

        for (auto &window : windows) {
            glfwDestroyWindow(window);
        }

        vkDestroyInstance(vulkan, nullptr);

        glfwTerminate();
    }

    void *GetDevice() { return &device; }
    void *GetInstance() { return &vulkan; }
    void *GetSurface() { return &surface; }
    void *GetPhyDevice() { return &physicalDevice; }
};

void Render::InitWindowManager(bool debugging) {
    windowManager = new WindowManager(debugging);
}

Render::QueueFamilyIndices Render::GetQueueFamilies() {
    return windowManager->GetQueueFamilies();
}

void *Render::GetDevice() {
    return windowManager->GetDevice();
}
void *Render::GetInstance() {
    return windowManager->GetInstance();
}
void *Render::GetSurface() {
    return windowManager->GetSurface();
}
void *Render::GetPhyDevice() {
    return windowManager->GetPhyDevice();
}

void *Render::QuerySwapChainSupport() {
    SwapChainSupportDetails *output = new SwapChainSupportDetails;
    *output = windowManager->QuerySwapChainSupport();
    return output;
}

GLFWwindow *Render::CreateWindow(int width, int height, const char *title, GLFWmonitor *screen, GLFWwindow *share) {
    return windowManager->Open(width, height, title, screen, share);
}

Render::WindowManager *Render::windowManager = nullptr;
}  // namespace Ignis
