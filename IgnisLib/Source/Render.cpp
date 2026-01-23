#include "IgnisLib.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#define NOMINMAX

#include <filesystem>


#include <chrono>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace Ignis {

struct WindowUserPointer {
    void *vulkanData;
    void *inputData;
};

using Vertex = Render::Vertex;
using CreateRenderPassInfo = Render::CreateRenderPassInfo;
using CreateGraphicPipeLineInfo = Render::CreateGraphicPipeLineInfo;
using UIRenderData = Render::UIRenderData;
using Window = Render::Window;

// need to align to the 16bit grid if it will go into a shader
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct TextureData {
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
};

struct SurfaceVulkanData {
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass;
    VkPipeline pipeline;
    VkPipelineLayout layout;

    std::vector<VkCommandBuffer> commandBuffers;

    // semaphores are used to specify the execution order of operations on the GPU
    // while fences are used to keep the CPU and GPU in sync with each-other.
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    bool haveVertexData;

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indiceCount;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void *> uniformBuffersMapped;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    size_t currentFrame = 0;
};

struct WindowVulkanData {
    std::unordered_map<VkSurfaceKHR, SurfaceVulkanData> surfaces;
    VkExtent2D swapChainExtent;
    VkSurfaceCapabilitiesKHR capabilities;
    bool framebufferResized = false;
};

struct VertexData {
    std::vector<Vertex> vertecies;
    std::vector<uint32_t> indicies;
};

static VkVertexInputBindingDescription GetVertexBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};

    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

static std::array<VkVertexInputAttributeDescription, 4> GetVertexAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32_UINT;
    attributeDescriptions[3].offset = offsetof(Vertex, texId);

    return attributeDescriptions;
}

static std::vector<char> readFile(const std::string &filename) {
    // std::ios::ate - start reading from end
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    // because we read from the end of the file, the current position is the size of the needed buffer
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    // reads the whole file in
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

static void CompileShader(const std::string filename, const std::string name) {
    std::string shaderOut = name + ".spv";

#if defined(_WIN32)
    const char *envVar = "VULKAN_SDK";
    char *value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, envVar) == 0 && value) {
        std::string sdkPath = value;
        free(value);

        std::string cmd = sdkPath + "\\Bin\\glslc.exe " + filename + " -o " + shaderOut;
        system(cmd.c_str());
    }
#else
    const char *sdkPath = std::getenv("VULKAN_SDK");
    std::string cmd;
    if (sdkPath) {
        cmd = std::string(sdkPath) + "/bin/glslc " + filename + " -o " + shaderOut;
    } else {
        cmd = "glslc " + filename + " -o " + shaderOut;  // fallback if VULKAN_SDK not set
    }
    system(cmd.c_str());
#endif
}

struct CreateRenderPassInfoVKConvert {
    VkSampleCountFlagBits samples;
    VkAttachmentLoadOp loadOp;
    VkAttachmentStoreOp storeOp;
    VkAttachmentLoadOp stencilLoadOp;
    VkAttachmentStoreOp stencilStoreOp;
    VkImageLayout initialLayout;
    VkImageLayout finalLayout;
};

static CreateRenderPassInfoVKConvert RenderPassInfoToVK(CreateRenderPassInfo info) {
    CreateRenderPassInfoVKConvert output;

    switch (info.samples) {
        case CreateRenderPassInfo::Samples::x1:
            output.samples = VK_SAMPLE_COUNT_1_BIT;
            break;
        case CreateRenderPassInfo::Samples::x2:
            output.samples = VK_SAMPLE_COUNT_2_BIT;
            break;
        case CreateRenderPassInfo::Samples::x4:
            output.samples = VK_SAMPLE_COUNT_4_BIT;
            break;
        case CreateRenderPassInfo::Samples::x8:
            output.samples = VK_SAMPLE_COUNT_8_BIT;
            break;
    }

    switch (info.loadOp) {
        case CreateRenderPassInfo::LoadOp::Load:
            output.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            break;
        case CreateRenderPassInfo::LoadOp::Clear:
            output.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        case CreateRenderPassInfo::LoadOp::DontCare:
            output.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;
    }

    switch (info.storeOp) {
        case CreateRenderPassInfo::StoreOp::Store:
            output.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            break;
        case CreateRenderPassInfo::StoreOp::DontCare:
            output.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            break;
    }

    switch (info.stencilLoadOp) {
        case CreateRenderPassInfo::LoadOp::Load:
            output.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            break;
        case CreateRenderPassInfo::LoadOp::Clear:
            output.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        case CreateRenderPassInfo::LoadOp::DontCare:
            output.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;
    }

    switch (info.stencilStoreOp) {
        case CreateRenderPassInfo::StoreOp::Store:
            output.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            break;
        case CreateRenderPassInfo::StoreOp::DontCare:
            output.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            break;
    }

    switch (info.initialLayout) {
        case CreateRenderPassInfo::ImageLayout::Undefined:
            output.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        case CreateRenderPassInfo::ImageLayout::PresentSrcKHR:
            output.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
    }

    switch (info.finalLayout) {
        case CreateRenderPassInfo::ImageLayout::Undefined:
            output.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        case CreateRenderPassInfo::ImageLayout::PresentSrcKHR:
            output.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
    }

    return output;
}

class Render::Vulkan {
   public:
    Vulkan(bool debuging = false) {

#ifndef IGNIS_INPUT
        glfwInit();
#endif

        if (debuging) enableValidationLayers = true;

        // basicly the whole system, the connection between the app and the vulkan api
        CreateInstance();

        // creating the messennger if the debug layer is enabled
        SetupDebugMessenger();
    }

    ~Vulkan() {
        CleanUp();
        std::cout << "vulkan instance cleaned up" << std::endl;
    }

    Window CreateVulkanWindow(int width, int height, const char *title, GLFWmonitor *monitor, GLFWwindow *share) {
        GLFWwindow *windowOut;
        // Not opengl so dont do any opengl stuff
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        windowOut = glfwCreateWindow(width, height, title, monitor, share);
        windows[windowOut] = std::make_unique<WindowVulkanData>();

        WindowUserPointer* ptr = new WindowUserPointer();
        ptr->vulkanData = windows[windowOut].get();
        glfwSetWindowUserPointer(windowOut, ptr);

        Window win;
        win.ptr = windowOut;

#ifdef IGNIS_INPUT

        Input::HookFramebufferSizeCallback(win, FramebufferResizeCallback);
#else 
        glfwSetFramebufferSizeCallback(windowOut, FramebufferResizeCallback);
#endif


        return win;
    }

    int CreateFontPage(const std::vector<uint8_t> &rgbaData, uint32_t width, uint32_t height) {
        TextureData data;
        CreateTextureImageFromMemory(rgbaData.data(), width, height, data.textureImage, data.textureImageMemory);
        data.textureImageView = CreateTextureImageView(data.textureImage);
        textureData[nextTexture] = data;
        UpdateTextureDescriptor(nextTexture, data.textureImageView);
        return nextTexture++;  // returns texture slot ID
    }

    // surface creating
    int CreateSurface(Window windowIn, CreateGraphicPipeLineInfo graphicPipeLineInfo, CreateRenderPassInfo renderPassInfo) {
        GLFWwindow *curWindow = windowIn.ptr;
        if (windows.find(curWindow) == windows.end()) throw std::runtime_error("failed to get the specified window");

        VkSurfaceKHR curSurface;

        glfwSetErrorCallback([](int error, const char* desc) {
            std::cerr << "GLFW Error " << error << ": " << desc << std::endl;
            });

        VkResult result = glfwCreateWindowSurface(instance, curWindow, nullptr, &curSurface);
        if (result != VK_SUCCESS) {
            std::string msg = "Failed to create Vulkan surface: ";
            switch (result) {
            case VK_ERROR_OUT_OF_HOST_MEMORY:        msg += "Out of host memory"; break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:      msg += "Out of device memory"; break;
            case VK_ERROR_EXTENSION_NOT_PRESENT:     msg += "Required extension not present"; break;
            case VK_ERROR_SURFACE_LOST_KHR:          msg += "Surface lost"; break;
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:  msg += "Native window already in use"; break;
            default:                                 msg += "Unknown error code " + std::to_string(result); break;
            }
            throw std::runtime_error(msg);
        }

        if (firstSurface) {
            surface = curSurface;

            // basicly selects the "GPU"
            PickPhysicalDevice();

            // creates the "computing" part of the instance, stuff get done with this
            CreateLogicalDevice();

            // gets imageformat and supported stuff, so we dont need to get that on every surface
            GetSwapChainData();

            // command pool is managing the memory used for the command buffers
            CreateCommandPool();

            firstSurface = false;

            CreateTextureSampler();
        }

        windows[curWindow]->surfaces[curSurface] = SurfaceVulkanData{};

        SurfaceVulkanData *surface = &(windows[curWindow]->surfaces[curSurface]);

        surface->haveVertexData = false;

        // creates the swapchain, the images that are rendered onto the screen
        CreateSwapChain(surface->swapChain, surface->swapChainImages, windows[curWindow].get(), curWindow, curSurface);

        // creates the views for the imagese in the swapchain
        CreateImageViews(surface->swapChainImageViews, surface->swapChainImages);

        // the passes to be executed on the data
        CreateRenderPass(surface->renderPass, renderPassInfo);

        CreateDescriptorSetLayout(surface);

        // create the rendering procedure that the data passes to be rendered
        CreateGraphicPipeline(windows[curWindow].get(), surface->pipeline, surface->layout, surface->renderPass, graphicPipeLineInfo, surface->descriptorSetLayout);  // need a CreatePipelineInfo later

        // creates depth resources for the surface so it can depth check
        CreateDepthResources(surface, windows[curWindow]->swapChainExtent);

        // creates the buffers for the images
        CreateFramebuffers(surface->swapChainFramebuffers, surface->swapChainImageViews, surface->renderPass, windows[curWindow].get(), surface->depthImageView);

        CreateUniformBuffers(surface);

        CreateDescriptorPool(surface);

        CreateDescriptorSets(surface);

        // creates command buffer that can be used to submit commands to specific queues
        CreateCommandBuffers(surface->commandBuffers);

        // creates the fences & semaphores to handle cpu-gpu syncronization
        CreateSyncObjects(surface->imageAvailableSemaphores, surface->renderFinishedSemaphores, surface->inFlightFences);

        surfaceAccess[nextSurface] = {curWindow, curSurface};

        return nextSurface++;
    }

    int CreateTexture(std::string path) {
        TextureData data;
        CreateTextureImage(path, data.textureImage, data.textureImageMemory);
        data.textureImageView = CreateTextureImageView(data.textureImage);

        textureData[nextTexture] = data;

        UpdateTextureDescriptor(nextTexture, data.textureImageView);

        return nextTexture++;
    }

    static void FramebufferResizeCallback(Window& windowIn) {
        auto window = reinterpret_cast<WindowUserPointer*>(glfwGetWindowUserPointer(windowIn.ptr));
        ((WindowVulkanData *)window->vulkanData)->framebufferResized = true;
    }

    void Update() {

#ifndef IGNIS_INPUT
        glfwPollEvents();
#endif

        for (auto &window : windows) {
            if (glfwWindowShouldClose(window.first) || window.second->surfaces.size() == 0) {
                CloseWindow(window.first);
                break;
            }
        }

        // if a ui element changed update the vertex & index buffer for that window
        UpdateElementBuffers();
    }

    bool IsValidSurface(int surfaceIndex) { return surfaceAccess.count(surfaceIndex) > 0; }

    void Draw(int surfaceIndex) {
        if (surfaceAccess.count(surfaceIndex) > 0) {
            DrawFrame(surfaceAccess[surfaceIndex].window, &surfaceAccess[surfaceIndex].surface);
            vkDeviceWaitIdle(device);
        } else
            throw std::runtime_error("invalid surface!");
    }

    int AddUIElementData(UIRenderData &data) {
        renderData[nextElement] = data;
        return nextElement++;
    }

   private:
    struct SwapChainSupportDetails {
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

    bool enableValidationLayers = false;

    const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME};

    const int MAX_FRAMES_IN_FLIGHT = 2;

    bool firstSurface = true;

    bool firstTexture = true;

    std::unordered_map<GLFWwindow *, std::unique_ptr<WindowVulkanData>> windows;
    VkInstance instance;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSurfaceKHR surface;

    SwapChainSupportDetails swapChainSupport;
    VkSurfaceFormatKHR swapChainImageFormat;
    VkPresentModeKHR swapChainPresentMode;

    VkCommandPool commandPool;

    std::unordered_map<int, TextureData> textureData;
    VkSampler textureSampler;
    int nextTexture = 1;
    uint32_t MAX_TEXTURES;

    VkImageView dummyImageView;
    VkImage dummyImage;
    VkDeviceMemory dummyImageMemory;

    std::unordered_map<int, UIRenderData> renderData;
    int nextElement = 0;

    struct SurfaceAccess {
        GLFWwindow *window;
        VkSurfaceKHR surface;
    };

    std::unordered_map<int, SurfaceAccess> surfaceAccess;
    int nextSurface = 0;

    void CleanupSwapChain(SurfaceVulkanData *surface) {
        vkDestroyImageView(device, surface->depthImageView, nullptr);
        vkDestroyImage(device, surface->depthImage, nullptr);
        vkFreeMemory(device, surface->depthImageMemory, nullptr);

        for (auto framebuffer : surface->swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        for (auto imageView : surface->swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(device, surface->swapChain, nullptr);
    }

    void CleanUpSurface(SurfaceVulkanData *data) {
        CleanupSwapChain(data);

        vkDestroyDescriptorSetLayout(device, data->descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, data->descriptorPool, nullptr);

        vkDestroyBuffer(device, data->vertexBuffer, nullptr);
        vkFreeMemory(device, data->vertexBufferMemory, nullptr);

        vkDestroyBuffer(device, data->indexBuffer, nullptr);
        vkFreeMemory(device, data->indexBufferMemory, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(device, data->uniformBuffers[i], nullptr);
            vkFreeMemory(device, data->uniformBuffersMemory[i], nullptr);
        }

        vkDestroyPipeline(device, data->pipeline, nullptr);
        vkDestroyPipelineLayout(device, data->layout, nullptr);
        vkDestroyRenderPass(device, data->renderPass, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, data->imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, data->renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, data->inFlightFences[i], nullptr);
        }
    }

    void CloseWindow(GLFWwindow *windowIn) {
        for (auto it = surfaceAccess.begin(); it != surfaceAccess.end();) {
            if (it->second.window == windowIn)
                it = surfaceAccess.erase(it);
            else
                ++it;
        }

        WindowVulkanData *window = windows[windowIn].get();
        for (auto &[surface, surfaceData] : window->surfaces) {
            CleanUpSurface(&surfaceData);

            vkDestroySurfaceKHR(instance, surface, nullptr);
        }

        auto exists = static_cast<WindowUserPointer *>(glfwGetWindowUserPointer(windowIn));
        if (exists != nullptr) {
            glfwSetWindowUserPointer(windowIn, nullptr);
            delete exists;
        }

        glfwDestroyWindow(windowIn);

        windows.erase(windowIn);

        if (windows.size() > 0) surface = windows.begin()->second->surfaces.begin()->first;
    }

    // Cleans up upon closing all the windows
    void CleanUp() {
        for (auto &[window, windowData] : windows) {
            for (auto &[surface, surfaceData] : windowData->surfaces) {
                CleanUpSurface(&surfaceData);
                vkDestroySurfaceKHR(instance, surface, nullptr);
            }
        }

        vkDestroySampler(device, textureSampler, nullptr);

        for (auto &[textureId, texture] : textureData) {
            vkDestroyImageView(device, texture.textureImageView, nullptr);
            vkDestroyImage(device, texture.textureImage, nullptr);
            vkFreeMemory(device, texture.textureImageMemory, nullptr);
        }

        vkDestroyImageView(device, dummyImageView, nullptr);
        vkDestroyImage(device, dummyImage, nullptr);
        vkFreeMemory(device, dummyImageMemory, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        for (auto &[window, windowData] : windows) {
            void *exists = glfwGetWindowUserPointer(window);
            if (exists != nullptr) {
                delete static_cast<WindowUserPointer*>(exists);
                glfwSetWindowUserPointer(window, nullptr);
            }
            glfwDestroyWindow(window);
        }

        vkDestroyInstance(instance, nullptr);

        glfwTerminate();
    }

    // update
    void DrawFrame(GLFWwindow *window, VkSurfaceKHR *surface) {
        SurfaceVulkanData *data = &windows[window]->surfaces[*surface];
        if (!data->haveVertexData) return;
        // waits for last frame to complete (for the fence), then resets it
        vkWaitForFences(device, 1, &data->inFlightFences[data->currentFrame], VK_TRUE, UINT64_MAX);

        // aquires the next available image, when it did, it signals the semaphore
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, data->swapChain, UINT64_MAX, data->imageAvailableSemaphores[data->currentFrame], VK_NULL_HANDLE, &imageIndex);

        // check if swapchain recreation is necessary
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            RecreateSwapChain(window, *surface);
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // still part of the last frame wait
        vkResetFences(device, 1, &data->inFlightFences[data->currentFrame]);

        // updating the uniform buffer for the frame
        UpdateUniformBuffer(data, windows[window].get());

        // resets and records the command buffer
        vkResetCommandBuffer(data->commandBuffers[data->currentFrame], 0);
        RecordCommandBuffer(data, windows[window]->swapChainExtent, imageIndex);

        // submiting it to the graphics family queue
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        // what semaphore to wait for
        VkSemaphore waitSemaphores[] = {data->imageAvailableSemaphores[data->currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        // assigning the command buffer
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &data->commandBuffers[data->currentFrame];
        // what semaphore to signal when the command buffer finished execution
        VkSemaphore signalSemaphores[] = {data->renderFinishedSemaphores[data->currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, data->inFlightFences[data->currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        // presents the drawn image to the swapchain/queue? idk
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {data->swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;

        result = vkQueuePresentKHR(presentQueue, &presentInfo);

        // check if swapchain recreation is necessary
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || windows[window]->framebufferResized) {
            windows[window]->framebufferResized = false;
            RecreateSwapChain(window, *surface);
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        data->currentFrame = (data->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void UpdateUniformBuffer(SurfaceVulkanData *surface, WindowVulkanData *window) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(270.0f), glm::vec3(0.5f, -0.5f, 1.0f));

        ubo.model = glm::rotate(ubo.model, time * glm::radians(100.0f), glm::vec3(0.6f, -0.2f, 0.0f));

        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        ubo.proj = glm::perspective(glm::radians(45.0f), window->swapChainExtent.width / (float)window->swapChainExtent.height, 0.1f, 10.0f);

        ubo.proj[1][1] *= -1;

        memcpy(surface->uniformBuffersMapped[surface->currentFrame], &ubo, sizeof(ubo));
    }

    void RecreateSwapChain(GLFWwindow *window, VkSurfaceKHR surface) {
        SurfaceVulkanData *data = &windows[window]->surfaces[surface];
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(device);

        CleanupSwapChain(data);

        CreateSwapChain(data->swapChain, data->swapChainImages, windows[window].get(), window, surface);
        CreateImageViews(data->swapChainImageViews, data->swapChainImages);
        CreateDepthResources(data, windows[window]->swapChainExtent);
        CreateFramebuffers(data->swapChainFramebuffers, data->swapChainImageViews, data->renderPass, windows[window].get(), data->depthImageView);
    }

    // assumes one element per surface
    void UpdateElementBuffers() {
        for (auto &[surfaceId, element] : renderData) {
            if (element.changed) {
                SurfaceAccess acces = surfaceAccess[surfaceId];
                SurfaceVulkanData *surfaceData = &windows[acces.window]->surfaces[acces.surface];
                VertexData data = GetVertexData(surfaceId, surfaceData);
                CreateVertexBuffer(surfaceData, data.vertecies);
                CreateIndexBuffer(surfaceData, data.indicies);
                surfaceData->indiceCount = data.indicies.size();
            }
        }
    }

    // Basic info, nothing
    void CreateInstance() {
        // check if debugging is possible if needed
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Ignis Rendering";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

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
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        // instancing
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }
    // getting the needed extensions
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

    // semaphore and fence creation
    void CreateSyncObjects(std::vector<VkSemaphore> &imageAvailableSemaphores, std::vector<VkSemaphore> &renderFinishedSemaphores, std::vector<VkFence> &inFlightFences) {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS || vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS || vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    // creates a buffer
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory) {
        // basic vertex buffer data
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        // getting the memory specs that the buffer need
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

        // allocating the memory for the buffer
        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        //"If the offset is non-zero, then it is required to be divisible by memRequirements.alignment."
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    // moves data from one buffer to another
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        EndSingleTimeCommands(commandBuffer);
    }

    // gets the verticies and indexes for the said surface so we can create the vertex buffer
    VertexData GetVertexData(int surfaceId, SurfaceVulkanData *surfaceData) {
        VertexData data;
        uint32_t index = 0;

        UIRenderData &element = renderData[surfaceId];

        data.vertecies.insert(data.vertecies.end(), element.vertecies.begin(), element.vertecies.end());

        for (auto i : element.indicies) {
            data.indicies.push_back(i + index);
        }

        index += static_cast<uint32_t>(element.vertecies.size());

        element.changed = false;

        return data;
    }

    // vertex buffer creation
    void CreateVertexBuffer(SurfaceVulkanData *surface, std::vector<Vertex> &vertices) {
        if (surface->vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, surface->vertexBuffer, nullptr);
            vkFreeMemory(device, surface->vertexBufferMemory, nullptr);
            surface->vertexBuffer = VK_NULL_HANDLE;
        }

        surface->haveVertexData = true;

        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        // stage buffer so we can move the data to the GPU
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        // mapping the memory so it can be written by the cpu
        // and then copies the data, and unmaps it
        void *data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t)bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        // creates the vertex buffer on the GPU where the CPU cant interact with it but its more efficient, and then transfers the data to it
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, surface->vertexBuffer, surface->vertexBufferMemory);

        CopyBuffer(stagingBuffer, surface->vertexBuffer, bufferSize);

        // cleans up stuff we dont need anymore
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    // index buffer creation
    void CreateIndexBuffer(SurfaceVulkanData *surface, std::vector<uint32_t> &indices) {
        if (surface->indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, surface->indexBuffer, nullptr);
            vkFreeMemory(device, surface->indexBufferMemory, nullptr);
            surface->vertexBuffer = VK_NULL_HANDLE;
        }

        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void *data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t)bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, surface->indexBuffer, surface->indexBufferMemory);

        CopyBuffer(stagingBuffer, surface->indexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    // uniform buffer creation
    void CreateUniformBuffers(SurfaceVulkanData *surface) {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        surface->uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        surface->uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        surface->uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, surface->uniformBuffers[i], surface->uniformBuffersMemory[i]);

            vkMapMemory(device, surface->uniformBuffersMemory[i], 0, bufferSize, 0, &surface->uniformBuffersMapped[i]);
        }
    }

    // makes the descriptors for "Uniform"(set) bindings, need to make all of them here
    void CreateDescriptorSetLayout(SurfaceVulkanData *surface) {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        MAX_TEXTURES = props.limits.maxPerStageDescriptorSamplers;
        if (MAX_TEXTURES < 1028) throw std::runtime_error("Necessary texture amount not available on the GPU");
        MAX_TEXTURES = (1028 < MAX_TEXTURES) ? 1028 : MAX_TEXTURES;

        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding = 1;
        samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.descriptorCount = MAX_TEXTURES;  // array size
        samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerBinding};

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &(surface->descriptorSetLayout)) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    // creates the pool for the descriptor sets
    void CreateDescriptorPool(SurfaceVulkanData *surface) {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * MAX_TEXTURES);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &surface->descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    // creates the actual descriptor sets
    void CreateDescriptorSets(SurfaceVulkanData *surface) {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, surface->descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = surface->descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        surface->descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &allocInfo, surface->descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        CreateImage(1, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, dummyImage, dummyImageMemory);
        dummyImageView = CreateTextureImageView(dummyImage);

        // idk what the pp happening here
        // update: now i know what the pp is happening
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = surface->uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            std::vector<VkDescriptorImageInfo> imageInfos(MAX_TEXTURES);

            for (uint32_t t = 0; t < MAX_TEXTURES; t++) {
                imageInfos[t].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                if (nextTexture > t + 1)
                    imageInfos[t].imageView = textureData[t].textureImageView;
                else
                    imageInfos[t].imageView = dummyImageView;

                imageInfos[t].sampler = textureSampler;
            }

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = surface->descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = surface->descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = MAX_TEXTURES;
            descriptorWrites[1].pImageInfo = imageInfos.data();

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void UpdateTextureDescriptor(int index, VkImageView textureView) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureView;
        imageInfo.sampler = textureSampler;

        for (auto &[window, windowData] : windows) {
            for (auto &[surface, surfaceData] : windowData->surfaces) {
                for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                    VkWriteDescriptorSet write{};
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = surfaceData.descriptorSets[i];
                    write.dstBinding = 1;
                    write.dstArrayElement = index;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    write.descriptorCount = 1;
                    write.pImageInfo = &imageInfo;

                    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
                }
            }
        }
    }

    // finding the best memory type based on the data and usage in our application
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    // helpers for Command Buffer recording
    VkCommandBuffer BeginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    // command pool creation
    void CreateCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }
    // command buffer creation
    void CreateCommandBuffers(std::vector<VkCommandBuffer> &commandBuffers) {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }
    // records command to commandbuffer, also need the image's index that you want to write to
    void RecordCommandBuffer(SurfaceVulkanData *surface, VkExtent2D extent, uint32_t imageIndex) {
        int frame = surface->currentFrame;
        VkCommandBuffer cmdBuffer = surface->commandBuffers[frame];

        // start the recording to a buffer with some specifications (if called on a buffer, it will reset it)
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;                   // Optional
        beginInfo.pInheritanceInfo = nullptr;  // Optional

        if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // begining the render pass on the framebuffer
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = surface->renderPass;
        renderPassInfo.framebuffer = surface->swapChainFramebuffers[imageIndex];

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.388235f, 0.643137f, 0.839216f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // commands to record
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, surface->pipeline);

        // the dynamic states need to be set
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        // binds the vertex buffers to the said bindings
        VkBuffer vertexBuffers[] = {surface->vertexBuffer};
        VkDeviceSize offsets[] = {0};  // set where to start reading vertex data from
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

        // binds the index buffer to the said binding
        vkCmdBindIndexBuffer(cmdBuffer, surface->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, surface->layout, 0, 1, &surface->descriptorSets[frame], 0, nullptr);

        // draw call
        vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(surface->indiceCount), 1, 0, 0, 0);

        // ending the render pass
        vkCmdEndRenderPass(cmdBuffer);

        // ending the command recording
        if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    // texture magic
    void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory) {
        VkImageCreateInfo imageInfo{};
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

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    void CreateTextureImage(std::string path, VkImage &image, VkDeviceMemory &memory) {
        int texWidth, texHeight, texChannels;
        stbi_uc *pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) throw std::runtime_error("failed to load texture image!");

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void *data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        stbi_image_free(pixels);

        CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

        TransitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

        TransitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void CreateTextureImageFromMemory(const uint8_t *rgba, uint32_t width, uint32_t height, VkImage &image, VkDeviceMemory &memory) {
        VkDeviceSize imageSize = width * height * 4;

        if (!rgba) throw std::runtime_error("failed to load texture image from memory!");

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void *data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, rgba, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        CreateImage(width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

        TransitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(stagingBuffer, image, width, height);
        TransitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        EndSingleTimeCommands(commandBuffer);
    }

    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        EndSingleTimeCommands(commandBuffer);
    }

    VkImageView CreateTextureImageView(VkImage textureImage) { return CreateImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT); }

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
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }

        return imageView;
    }

    void CreateTextureSampler() {
        VkSamplerCreateInfo samplerInfo{};

        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

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

        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    // depth testing
    void CreateDepthResources(SurfaceVulkanData *surface, VkExtent2D extent) {
        VkFormat depthFormat = findDepthFormat();

        CreateImage(extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, surface->depthImage, surface->depthImageMemory);
        surface->depthImageView = CreateImageView(surface->depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

        TransitionImageLayout(surface->depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }

            throw std::runtime_error("failed to find supported format!");
        }

        return candidates[0];
    }
    VkFormat findDepthFormat() { return findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT); }
    bool hasStencilComponent(VkFormat format) { return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT; }

    // framebuffer creation
    void CreateFramebuffers(std::vector<VkFramebuffer> &buffers, std::vector<VkImageView> &views, VkRenderPass renderPass, WindowVulkanData *window, VkImageView depthImageView) {
        buffers.resize(views.size());

        for (size_t i = 0; i < views.size(); i++) {
            std::array<VkImageView, 2> attachments = {views[i], depthImageView};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = window->swapChainExtent.width;
            framebufferInfo.height = window->swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &buffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void GetSwapChainData() {
        swapChainSupport = QuerySwapChainSupport(physicalDevice);

        swapChainImageFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
        swapChainPresentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    }
    // creating the swapchain
    void CreateSwapChain(VkSwapchainKHR &swapchain, std::vector<VkImage> &images, WindowVulkanData *windowData, GLFWwindow *window, VkSurfaceKHR curSurface) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, curSurface, &windowData->capabilities);
        windowData->swapChainExtent = ChooseSwapExtent(windowData->capabilities, window);
        // number of images in swapchain
        uint32_t imageCount = windowData->capabilities.minImageCount + 1;

        // not exceeding maximum image count
        if (windowData->capabilities.maxImageCount > 0 && imageCount > windowData->capabilities.maxImageCount) {
            imageCount = windowData->capabilities.maxImageCount;
        }

        // seting surface and other info for swapchain
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = curSurface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = swapChainImageFormat.format;
        createInfo.imageColorSpace = swapChainImageFormat.colorSpace;
        createInfo.imageExtent = windowData->swapChainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // set ownership/sharing of images between queues
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;      // Optional
            createInfo.pQueueFamilyIndices = nullptr;  // Optional
        }

        createInfo.preTransform = windowData->capabilities.currentTransform;

        // if possible makes the buffer able to be transparent
        if (windowData->capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
        else
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        createInfo.presentMode = swapChainPresentMode;

        // doesnt caring about pixels that are covered by something else,
        // maybe need to disable for transparency
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // retrieveing the images for the swapchain
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        images.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());
    }
    // create the view that the images can be viewed through
    void CreateImageViews(std::vector<VkImageView> &views, std::vector<VkImage> &images) {
        views.resize(images.size());

        for (size_t i = 0; i < images.size(); i++) {
            views[i] = CreateImageView(images[i], swapChainImageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }

    // creates the pipeline
    void CreateGraphicPipeline(WindowVulkanData *window, VkPipeline &pipeline, VkPipelineLayout &layout, VkRenderPass renderPass, CreateGraphicPipeLineInfo graphicPipeLineInfo, VkDescriptorSetLayout &decriptorSetLayout) {
        // reads in the binary shader data
        CompileShader(graphicPipeLineInfo.vertexShader, "vert");
        auto vertShaderCode = readFile("vert.spv");
        CompileShader(graphicPipeLineInfo.fragmentShader, "frag");
        auto fragShaderCode = readFile("frag.spv");

        std::filesystem::remove("vert.spv");
        std::filesystem::remove("frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        // shader data
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // vertex data specifications
        auto bindingDescription = GetVertexBindingDescription();
        auto attributeDescriptions = GetVertexAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        ;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        //!!! what type of data it uses (lines, triangles), and how (reusing vertecies or not)
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // sets where and in what size the frambuffer is visible
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)window->swapChainExtent.width;
        viewport.height = (float)window->swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // cuts off parts of the framebuffer from rendering
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = window->swapChainExtent;

        // handles what should be dinamic during runtime
        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // viewport create info
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // rasterizer data
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;

        // set how fragments are generated based on geometry (point, line, fill)
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;

        // culling
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;  // Optional
        rasterizer.depthBiasClamp = 0.0f;           // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f;     // Optional

        // multisampling - a.k.a. easy anti-alliasing
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;           // Optional
        multisampling.pSampleMask = nullptr;             // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE;  // Optional
        multisampling.alphaToOneEnable = VK_FALSE;       // Optional

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;  // Optional
        depthStencil.maxDepthBounds = 1.0f;  // Optional

        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};  // Optional
        depthStencil.back = {};   // Optional

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;                 // Optional
        pipelineLayoutInfo.pSetLayouts = &decriptorSetLayout;  // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0;         // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr;      // Optional

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;

        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;

        pipelineInfo.layout = layout;

        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        // can derive render passes from one-another so it can have a parent-child hierarchy, faster, easier
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }
    // create the render pass(es)
    void CreateRenderPass(VkRenderPass &renderPass, CreateRenderPassInfo infoIn) {
        CreateRenderPassInfoVKConvert info = RenderPassInfoToVK(infoIn);

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat.format;
        colorAttachment.samples = info.samples;

        // what to do with the data before and after rendering (applies to color and depth data)
        colorAttachment.loadOp = info.loadOp;    // clears framebuffer before rendering
        colorAttachment.storeOp = info.storeOp;  // leave the data in the buffer

        colorAttachment.stencilLoadOp = info.stencilLoadOp;
        colorAttachment.stencilStoreOp = info.stencilStoreOp;

        // what layout should the buffer have before and after the pass
        colorAttachment.initialLayout = info.initialLayout;
        colorAttachment.finalLayout = info.finalLayout;

        // depth tesint pass
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // subpass, good for post processing, need only one for rendering
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        // subpass dependency woodoo shit
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;

        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // creating the render pass
        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    // creates the structs for the vk shaders
    VkShaderModule createShaderModule(const std::vector<char> &code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surfaceIn = NULL) {
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
    // surface format (color depth, color format)
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
        for (const auto &availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }
    // presentation mode (when does the images get drawn to the surface)
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
        for (const auto &availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }
    // swapExtent, set the size of the swap chain image size
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow *window) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    // logic device creating
    void CreateLogicalDevice() {
        // get requested queues
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // features like what we requested from the physical device
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        VkPhysicalDeviceVulkan12Features features12{};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        features12.descriptorIndexing = VK_TRUE;

        // main device creation struct
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        createInfo.pNext = &features12;

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

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }
    // device picking
    void PickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

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
    // checking if device have the features that we need
    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = CheckDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    }
    // check for UPGRADES BROTHER, i mean physical device stuff we need
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
    // struct for the "features"
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() { return graphicsFamily.has_value(); }
    };
    // searching through the features if it has what we need to have to work on the stuff that we need to work on for the lib to work for the app to work for us
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    // debuging(black magic shit)
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
    void SetupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
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
};

Window Render::CreateAppWindow(int width, int height, const char *title, GLFWmonitor *screen, GLFWwindow *share) { return instance->CreateVulkanWindow(width, height, title, screen, share); }

int Render::CreateSurface(Window window, CreateGraphicPipeLineInfo graphicPipeLineInfo, CreateRenderPassInfo renderPassInfo) { return instance->CreateSurface(window, graphicPipeLineInfo, renderPassInfo); }

void Render::Draw(int surface) { instance->Draw(surface); }

void Render::Update() { instance->Update(); }

bool Render::IsValidSurface(int surfaceIndex) { return instance->IsValidSurface(surfaceIndex); }

int Render::AddUIElementData(UIRenderData &data) { return instance->AddUIElementData(data); }

int Render::CreateTexture(std::string path) { return instance->CreateTexture(path); }

void Render::Init(bool debugging) { instance = new Render::Vulkan(debugging); }

void Render::Clean() { delete instance; }

int Render::CreateFontPage(const std::vector<uint8_t> &rgbaData, uint32_t width, uint32_t height) { return instance->CreateFontPage(rgbaData, width, height); };

Render::Vulkan *Render::instance = nullptr;
}  // namespace Ignis
