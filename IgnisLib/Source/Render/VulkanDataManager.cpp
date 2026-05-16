#include "../IgnisLib.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define MAX_FRAMES_IN_FLIGHT 2

namespace Ignis {

////////////////////////
////    Structs    /////
////////////////////////

struct SwapChainSupportDetails {
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct WindowData {
    VkSurfaceKHR surface;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    bool framebufferResized = false;

    std::vector<VkCommandBuffer> commandBuffers;

    VkExtent2D swapChainExtent;
    VkSurfaceCapabilitiesKHR capabilities;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    bool haveVertexData = false;

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indiceCount;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    std::vector<int> pipelines;
};

////////////////////////
//////    Data    //////
////////////////////////

class Render::VulkanDataManager {
   private:
    size_t currentFrame = 0;

    VkCommandPool graphicPool;
    VkCommandPool presentPool;
    VkCommandPool computePool;
    VkCommandPool transferPool;

    SwapChainSupportDetails swapChainSupport;
    VkSurfaceFormatKHR swapChainImageFormat;
    VkPresentModeKHR swapChainPresentMode;

    std::unordered_map<GLFWwindow *, WindowData> windows;

    VkInstance *instance;
    VkDevice *device;
    VkPhysicalDevice *phyDevice;
    VkSurfaceKHR *windowManagerSurface;

    ////////////////////////
    ////   Functions   /////
    ////////////////////////

    void CreateCommandPools() {
        QueueFamilyIndices queueFamilyIndices = GetQueueFamilies();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphics.family;

        VkDevice device = *(VkDevice *)GetDevice();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &graphicPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }

        poolInfo.queueFamilyIndex = queueFamilyIndices.present.family;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &presentPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }

        poolInfo.queueFamilyIndex = queueFamilyIndices.compute.family;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &computePool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }

        poolInfo.queueFamilyIndex = queueFamilyIndices.transfer.family;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &transferPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow *window) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
        for (const auto &availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
        for (const auto &availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }
    void GetSwapChainData() {
        SwapChainSupportDetails *input = (SwapChainSupportDetails *)QuerySwapChainSupport();
        swapChainSupport = *input;
        delete input;

        swapChainImageFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
        swapChainPresentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    }
    void CreateSwapChain(GLFWwindow *window) {
        WindowData &data = windows[window];
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*phyDevice, data.surface, &data.capabilities);
        data.swapChainExtent = ChooseSwapExtent(data.capabilities, window);
        // number of images in swapchain
        uint32_t imageCount = data.capabilities.minImageCount + 1;

        // not exceeding maximum image count
        if (data.capabilities.maxImageCount > 0 && imageCount > data.capabilities.maxImageCount) {
            imageCount = data.capabilities.maxImageCount;
        }

        // seting surface and other info for swapchain
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = data.surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = swapChainImageFormat.format;
        createInfo.imageColorSpace = swapChainImageFormat.colorSpace;
        createInfo.imageExtent = data.swapChainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // set ownership/sharing of images between queues
        QueueFamilyIndices indices = GetQueueFamilies();
        uint32_t queueFamilyIndices[] = { indices.graphics.family, indices.present.family };

        if (indices.graphics.family != indices.present.family) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;      // Optional
            createInfo.pQueueFamilyIndices = nullptr;  // Optional
        }

        createInfo.preTransform = data.capabilities.currentTransform;

        // if possible makes the buffer able to be transparent
        if (data.capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
        else
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        createInfo.presentMode = swapChainPresentMode;

        // doesnt caring about pixels that are covered by something else,
        // maybe need to disable for transparency
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(*device, &createInfo, nullptr, &data.swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // retrieveing the images for the swapchain
        vkGetSwapchainImagesKHR(*device, data.swapChain, &imageCount, nullptr);
        data.swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(*device, data.swapChain, &imageCount, data.swapChainImages.data());
    }

    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(*device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }

        return imageView;
    }
    void CreateImageViews(GLFWwindow *window) {
        WindowData &data = windows[window];
        data.swapChainImageViews.resize(data.swapChainImages.size());

        for (size_t i = 0; i < data.swapChainImages.size(); i++) {
            data.swapChainImageViews[i] = CreateImageView(data.swapChainImages[i], swapChainImageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }

    void CreateCommandBuffers(GLFWwindow *window) {
        WindowData &data = windows[window];
        data.commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = graphicPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)data.commandBuffers.size();

        if (vkAllocateCommandBuffers(*device, &allocInfo, data.commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void CreateSyncObjects(GLFWwindow *window) {
        WindowData &data = windows[window];
        data.imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        data.renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        data.inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(*device, &semaphoreInfo, nullptr, &data.imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(*device, &semaphoreInfo, nullptr, &data.renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(*device, &fenceInfo, nullptr, &data.inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

   public:
    VulkanDataManager() {
        GetSwapChainData();
        instance = (VkInstance *)GetInstance();
        device = (VkDevice *)GetDevice();
        phyDevice = (VkPhysicalDevice *)GetPhyDevice();
        windowManagerSurface = (VkSurfaceKHR *)GetSurface();
    }

    void CreateSurface(GLFWwindow *window) {
        windows[window] = WindowData{};

        glfwCreateWindowSurface(*instance, window, nullptr, &windows[window].surface);

        if (*windowManagerSurface == VK_NULL_HANDLE)
            *windowManagerSurface = windows[window].surface;

        CreateSwapChain(window);

        CreateImageViews(window);

        CreateSyncObjects(window);
    }
};

void Render::InitVulkanDataManager() {
    vulkanDataManager = new VulkanDataManager();
}

void Render::CreateSurface(void *window) {
    vulkanDataManager->CreateSurface((GLFWwindow *)window);
}

Render::VulkanDataManager *Render::vulkanDataManager = nullptr;
}  // namespace Ignis
