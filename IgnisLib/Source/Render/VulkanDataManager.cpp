#include "../IgnisLib.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace Ignis {

struct WindowData {
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkExtent2D swapChainExtent;
    VkSurfaceCapabilitiesKHR capabilities;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    bool haveVertexData;

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indiceCount;

    bool framebufferResized = false;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    size_t currentFrame = 0;

    std::vector<int> pipelines;
};

}


