#include "IgnisLib.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#define NOMINMAX

#include <filesystem>
#include <array>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"


namespace Ignis {

// TODO: place it somewhere better
template <typename T>
concept hasPNext = requires(T obj) { obj.pNext; };

template <hasPNext T, hasPNext U>
void addPNext(T *addTo, const U *pNext) {
    auto *node = reinterpret_cast<VkBaseOutStructure *>(addTo);
    while (node->pNext) node = reinterpret_cast<VkBaseOutStructure *>(node->pNext);
    node->pNext = const_cast<VkBaseOutStructure *>(reinterpret_cast<const VkBaseOutStructure *>(pNext));
}

struct WindowUserPointer {
    void *vulkanData;
    void *inputData;
};

using Vertex = Render::Vertex;
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

struct CreateGraphicPipeLineInfoVKConvert {
    std::string vertexShader;
    std::string fragmentShader;

    VkPrimitiveTopology topology;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    float lineWidth;

    VkSampleCountFlagBits rasterizationSamples;
    VkBool32 sampleShadingEnable;
    float minSampleShading;

    VkBool32 blendEnable;
    VkBlendFactor srcColorBlendFactor;
    VkBlendFactor dstColorBlendFactor;
    VkBlendOp colorBlendOp;
    VkBlendFactor srcAlphaBlendFactor;
    VkBlendFactor dstAlphaBlendFactor;
    VkBlendOp alphaBlendOp;

    VkBool32 depthTestEnable;
    VkBool32 depthWriteEnable;

    float clearBit[3];
    float stencilBit[2];

    int constantsSize = 0;
    std::vector<Render::DescriptorSetInfo> descriptorSets;

    std::vector<Render::VertexDataType> vertexDataLayout;
};

struct CreateSamplerVKConvert {
    VkFilter filter;
    VkSamplerMipmapMode mipmapMode;
    VkSamplerAddressMode addressMode;
};

struct SurfaceVulkanData {
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    CreateGraphicPipeLineInfoVKConvert pipelineData;
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

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    VkDescriptorPool descriptorPool;
    std::vector<std::vector<VkDescriptorSet>> descriptorSets;

    std::unordered_map<int, std::vector<VkBuffer>> uniformBuffers;
    std::unordered_map<int, std::vector<VkDeviceMemory>> uniformBuffersMemory;
    std::unordered_map<int, std::vector<void *>> uniformBuffersMapped;

    std::unordered_map<int, std::vector<VkBuffer>> storageBuffers;
    std::unordered_map<int, std::vector<VkDeviceMemory>> storageBuffersMemory;
    std::unordered_map<int, std::vector<void*>> storageBuffersMapped;

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

static CreateGraphicPipeLineInfoVKConvert RenderPassInfoToVK(CreateGraphicPipeLineInfo info) {
    CreateGraphicPipeLineInfoVKConvert output;

    output.vertexShader = info.vertexShader;
    output.fragmentShader = info.fragmentShader;

    switch (info.topology) {
        case CreateGraphicPipeLineInfo::Topology::Triangle:
            output.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            break;
        case CreateGraphicPipeLineInfo::Topology::Line:
            output.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            break;
        case CreateGraphicPipeLineInfo::Topology::Point:
            output.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            break;
    }

    switch (info.culling) {
        case CreateGraphicPipeLineInfo::Culling::Back:
            output.cullMode = VK_CULL_MODE_BACK_BIT;
            break;
        case CreateGraphicPipeLineInfo::Culling::Front:
            output.cullMode = VK_CULL_MODE_FRONT_BIT;
            break;
        case CreateGraphicPipeLineInfo::Culling::None:
            output.cullMode = VK_CULL_MODE_NONE;
            break;
    }

    switch (info.frontFace) {
        case CreateGraphicPipeLineInfo::FrontFace::CounterClockwise:
            output.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            break;
        case CreateGraphicPipeLineInfo::FrontFace::Clockwise:
            output.frontFace = VK_FRONT_FACE_CLOCKWISE;
            break;
    }

    output.lineWidth = info.lineWidth;

    switch (info.samples) {
        case CreateGraphicPipeLineInfo::Samples::x1:
            output.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            break;
        case CreateGraphicPipeLineInfo::Samples::x2:
            output.rasterizationSamples = VK_SAMPLE_COUNT_2_BIT;
            break;
        case CreateGraphicPipeLineInfo::Samples::x4:
            output.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
            break;
        case CreateGraphicPipeLineInfo::Samples::x8:
            output.rasterizationSamples = VK_SAMPLE_COUNT_8_BIT;
            break;
    }

    output.sampleShadingEnable = info.sampleShading ? VK_TRUE : VK_FALSE;
    output.minSampleShading = info.minSampleShading;

    output.blendEnable = info.blendEnable ? VK_TRUE : VK_FALSE;

    auto ConvertBlendFactor = [](CreateGraphicPipeLineInfo::BlendFactor factor) -> VkBlendFactor {
        switch (factor) {
        case CreateGraphicPipeLineInfo::BlendFactor::SrcAlpha:        return VK_BLEND_FACTOR_SRC_ALPHA;
        case CreateGraphicPipeLineInfo::BlendFactor::DstAlpha:        return VK_BLEND_FACTOR_DST_ALPHA;
        case CreateGraphicPipeLineInfo::BlendFactor::OneMinusSrcAlpha:return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case CreateGraphicPipeLineInfo::BlendFactor::OneMinusDstAlpha:return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case CreateGraphicPipeLineInfo::BlendFactor::SrcColor:        return VK_BLEND_FACTOR_SRC_COLOR;
        case CreateGraphicPipeLineInfo::BlendFactor::DstColor:        return VK_BLEND_FACTOR_DST_COLOR;
        case CreateGraphicPipeLineInfo::BlendFactor::OneMinusSrcColor:return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case CreateGraphicPipeLineInfo::BlendFactor::OneMinusDstColor:return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        }
        return VK_BLEND_FACTOR_ONE; // fallback
    };

    auto ConvertBlendOp = [](CreateGraphicPipeLineInfo::BlendMode mode) -> VkBlendOp {
        switch (mode) {
        case CreateGraphicPipeLineInfo::BlendMode::Add: return VK_BLEND_OP_ADD;
        case CreateGraphicPipeLineInfo::BlendMode::Sub: return VK_BLEND_OP_SUBTRACT;
        case CreateGraphicPipeLineInfo::BlendMode::Max: return VK_BLEND_OP_MAX;
        case CreateGraphicPipeLineInfo::BlendMode::Min: return VK_BLEND_OP_MIN;
        }
        return VK_BLEND_OP_ADD; // fallback
    };

    output.srcColorBlendFactor = ConvertBlendFactor(info.scrColorBlend);
    output.dstColorBlendFactor = ConvertBlendFactor(info.dstColorBlend);
    output.colorBlendOp = ConvertBlendOp(info.colorBlendOp);

    output.srcAlphaBlendFactor = ConvertBlendFactor(info.scrAlphaBlend);
    output.dstAlphaBlendFactor = ConvertBlendFactor(info.dstAlphaBlend);
    output.alphaBlendOp = ConvertBlendOp(info.alphaBlendOp);

    output.depthTestEnable = info.depthTesting ? VK_TRUE : VK_FALSE;
    output.depthWriteEnable = info.depthWriting ? VK_TRUE : VK_FALSE;

    for (int i = 0; i < 3; i++) output.clearBit[i] = info.clearBit[i];
    for (int i = 0; i < 2; i++) output.stencilBit[i] = info.stencilBit[i];

    output.constantsSize = info.constantsSize;
    output.descriptorSets = info.descriptorSets;

    output.vertexDataLayout = std::move(info.vertexDataLayout);

    return output;
}

static CreateSamplerVKConvert SamplerInfoToVK(SamplerFilter filter, SamplerAddressing addressing, SamplerMipmapMode mipmapMode) {
    CreateSamplerVKConvert output;

    switch (filter) {
    case SamplerFilter::LINEAR:
        output.filter = VK_FILTER_LINEAR;
        break;
    case SamplerFilter::NEAREST:
        output.filter = VK_FILTER_NEAREST;
        break;
    }

    switch (addressing) {
    case SamplerAddressing::REPEAT:
        output.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;
    case SamplerAddressing::MIRRORED_REPEAT:
        output.addressMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        break;
    case SamplerAddressing::CLAMP_TO_EDGE:
        output.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;
    case SamplerAddressing::CLAMP_TO_BORDER:
        output.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        break;
    case SamplerAddressing::MIRROR_CLAMP_TO_EDGE:
        output.addressMode = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        break;
    }

    switch (mipmapMode) {
    case SamplerMipmapMode::LINEAR:
        output.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        break;
    case SamplerMipmapMode::NEAREST:
        output.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    }

    return output;
}

static VkDescriptorType DescriptorTypeToVK(Render::DescriptorInfo::DescriptorType type) {
    VkDescriptorType output;

    switch (type)
    {
    case Ignis::Render::DescriptorInfo::DescriptorType::UNIFORM:
        output = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        break;
    case Ignis::Render::DescriptorInfo::DescriptorType::STORAGE:
        output = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        break;
    case Ignis::Render::DescriptorInfo::DescriptorType::IMAGE:
        output = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        break;
    default:
        break;
    }

    return output;
}

static VkShaderStageFlags ShaderStageToVK(Render::ShaderStage stage) {
    VkShaderStageFlags output = 0;
    if (stage & Render::ShaderStage::VERTEX)   output |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stage & Render::ShaderStage::FRAGMENT) output |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stage & Render::ShaderStage::COMPUTE) output |= VK_SHADER_STAGE_COMPUTE_BIT;
    return output;
}

class Render::Vulkan {

   private:
    struct SwapChainSupportDetails {
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct SurfaceAccess {
        GLFWwindow* window;
        VkSurfaceKHR surface;
    };

    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

    bool enableValidationLayers = false;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME
    };

    const int MAX_FRAMES_IN_FLIGHT = 2;

    bool firstSurface = true;

    std::unordered_map<GLFWwindow*, std::unique_ptr<WindowVulkanData>> windows;
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
    unsigned int nextTexture = 1;
    uint32_t MAX_TEXTURES;

    VkImageView dummyImageView;
    VkImage dummyImage;
    VkDeviceMemory dummyImageMemory;

    std::unordered_map<int, UIRenderData> renderData;
    int nextElement = 0;

    std::unordered_map<int, SurfaceAccess> surfaceAccess;
    int nextSurface = 0;

    std::vector <CreateSamplerVKConvert> samplerCreateDatas;
    std::unordered_map<int, VkSampler> samplerAccess;
    int nextSamplerData = 0;
    int nextSampler = 0;

    std::unordered_map<VkSurfaceKHR, void*> constantsData;

    std::unordered_map<VkSurfaceKHR, std::vector<Render::VertexDataType>> vertexDataLayout;


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

        WindowUserPointer *ptr = new WindowUserPointer();
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
    int CreateSurface(Window windowIn, CreateGraphicPipeLineInfo graphicPipeLineInfo) {
        GLFWwindow *curWindow = windowIn.ptr;
        if (windows.find(curWindow) == windows.end()) throw std::runtime_error("failed to get the specified window");

        VkSurfaceKHR curSurface;

        glfwCreateWindowSurface(instance, curWindow, nullptr, &curSurface);

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

            CreateSamplerFromData();

            firstSurface = false;
        }

        windows[curWindow]->surfaces[curSurface] = SurfaceVulkanData{};

        SurfaceVulkanData *surface = &(windows[curWindow]->surfaces[curSurface]);

        surface->haveVertexData = false;

        constantsData[curSurface] = nullptr;

        // creates the swapchain, the images that are rendered onto the screen
        CreateSwapChain(surface->swapChain, surface->swapChainImages, windows[curWindow].get(), curWindow, curSurface);

        // creates the views for the imagese in the swapchain
        CreateImageViews(surface->swapChainImageViews, surface->swapChainImages);

        surface->pipelineData = RenderPassInfoToVK(graphicPipeLineInfo);

        CreateDescriptorSetLayout(surface);

        // create the rendering procedure that the data passes to be rendered
        CreateGraphicPipeline(windows[curWindow].get(), surface, curSurface);  // need a CreatePipelineInfo later

        // creates command buffer that can be used to submit commands to specific queues
        CreateCommandBuffers(surface->commandBuffers);

        // creates the fences & semaphores to handle cpu-gpu syncronization
        CreateSyncObjects(surface->imageAvailableSemaphores, surface->renderFinishedSemaphores, surface->inFlightFences);

        surfaceAccess[nextSurface] = { curWindow, curSurface };

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

    static void FramebufferResizeCallback(Window windowIn) {
        auto window = reinterpret_cast<WindowUserPointer *>(glfwGetWindowUserPointer(windowIn.ptr));
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

    bool IsValidSurface(int surfaceIndex) { return surfaceAccess.find(surfaceIndex) != surfaceAccess.end(); }

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

    void* GetWindowOfSurface(int surface) {
        return surfaceAccess[surface].window;
    }

    int CreateSampler(SamplerFilter filter, SamplerAddressing addressing, SamplerMipmapMode mipmapMode) {
        CreateSamplerVKConvert samplerVK = SamplerInfoToVK(filter, addressing, mipmapMode);
        samplerCreateDatas.push_back(samplerVK);
        return nextSamplerData++;
    }

    void PushConstants(int surface, void* data, uint32_t size) {
        if (constantsData[surfaceAccess[surface].surface] != nullptr)
            free(constantsData[surfaceAccess[surface].surface]);

        constantsData[surfaceAccess[surface].surface] = malloc(size);
        memcpy(constantsData[surfaceAccess[surface].surface], data, size);
    }

   private:

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

        for (auto& layout : data->descriptorSetLayouts)
            vkDestroyDescriptorSetLayout(device, layout, nullptr);

        vkDestroyDescriptorPool(device, data->descriptorPool, nullptr);

        vkDestroyBuffer(device, data->vertexBuffer, nullptr);
        vkFreeMemory(device, data->vertexBufferMemory, nullptr);

        vkDestroyBuffer(device, data->indexBuffer, nullptr);
        vkFreeMemory(device, data->indexBufferMemory, nullptr);

        for (int j = 0; j < data->uniformBuffers.size(); j++)
        {
            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyBuffer(device, data->uniformBuffers[j][i], nullptr);
                vkFreeMemory(device, data->uniformBuffersMemory[j][i], nullptr);
            }
        }


        vkDestroyPipeline(device, data->pipeline, nullptr);
        vkDestroyPipelineLayout(device, data->layout, nullptr);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
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
        for (auto& [surface, data] : constantsData) {
            if (data != nullptr) free(data);
        }

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
                delete static_cast<WindowUserPointer *>(exists);
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
        RecordCommandBuffer(data, windows[window]->swapChainExtent, imageIndex, *surface);

        // submiting it to the graphics family queue
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        // what semaphore to wait for
        VkSemaphore waitSemaphores[] = { data->imageAvailableSemaphores[data->currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        // assigning the command buffer
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &data->commandBuffers[data->currentFrame];
        // what semaphore to signal when the command buffer finished execution
        VkSemaphore signalSemaphores[] = { data->renderFinishedSemaphores[data->currentFrame] };
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

        VkSwapchainKHR swapChains[] = { data->swapChain };
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

        memcpy(surface->uniformBuffersMapped[0][surface->currentFrame], &ubo, sizeof(ubo));
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
            addPNext(&createInfo, &debugCreateInfo);

            // if (gpuAssistedEnabledValidation) {
            //     VkValidationFeatureEnableEXT enables[] = {
            //         VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
            //         VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
            //         VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT
            //     };
            //
            //     VkValidationFeaturesEXT validationFeatures{};
            //     validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            //     validationFeatures.enabledValidationFeatureCount =
            //         static_cast<uint32_t>(std::size(enables));
            //     validationFeatures.pEnabledValidationFeatures = enables;
            //     validationFeatures.pDisabledValidationFeatures = nullptr;
            //     validationFeatures.disabledValidationFeatureCount = 0;
            //
            //     addPNext(&createInfo, &validationFeatures);
            // }

        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

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

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS || vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS || vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    void CreateSamplerFromData() {
        for (auto& samplerVK : samplerCreateDatas) {

            VkSamplerCreateInfo samplerInfo{};

            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

            samplerInfo.magFilter = samplerVK.filter;
            samplerInfo.minFilter = samplerVK.filter;

            samplerInfo.addressModeU = samplerVK.addressMode;
            samplerInfo.addressModeV = samplerVK.addressMode;
            samplerInfo.addressModeW = samplerVK.addressMode;

            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(physicalDevice, &properties);

            samplerInfo.anisotropyEnable = VK_TRUE;
            samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;

            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

            samplerInfo.mipmapMode = samplerVK.mipmapMode;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 0.0f;

            if (vkCreateSampler(device, &samplerInfo, nullptr, &samplerAccess[nextSampler++]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create texture sampler!");
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
    void CreateUniformBuffers(SurfaceVulkanData *surface, int size, int index) {
        VkDeviceSize bufferSize = size;

        surface->uniformBuffers[index].resize(MAX_FRAMES_IN_FLIGHT);
        surface->uniformBuffersMemory[index].resize(MAX_FRAMES_IN_FLIGHT);
        surface->uniformBuffersMapped[index].resize(MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, surface->uniformBuffers[index][i], surface->uniformBuffersMemory[index][i]);

            vkMapMemory(device, surface->uniformBuffersMemory[index][i], 0, bufferSize, 0, &surface->uniformBuffersMapped[index][i]);
        }
    }

    void CreateStorageBuffers(SurfaceVulkanData* surface, int size, int index) {
        VkDeviceSize bufferSize = size;

        surface->storageBuffers[index].resize(MAX_FRAMES_IN_FLIGHT);
        surface->storageBuffersMemory[index].resize(MAX_FRAMES_IN_FLIGHT);
        surface->storageBuffersMapped[index].resize(MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            CreateBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, surface->storageBuffers[index][i], surface->storageBuffersMemory[index][i]);

            vkMapMemory(device, surface->storageBuffersMemory[index][i], 0, bufferSize, 0, &surface->storageBuffersMapped[index][i]);
        }
    }

    // makes the descriptors for "Uniform"(set) bindings, need to make all of them here
    void CreateDescriptorSetLayout(SurfaceVulkanData *surface) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        surface->descriptorSetLayouts.resize(surface->pipelineData.descriptorSets.size());

        for (size_t i = 0; i < surface->pipelineData.descriptorSets.size(); i++)
        {
            auto& item = surface->pipelineData.descriptorSets[i];
            std::vector<VkDescriptorBindingFlagsEXT> bindingFlags(item.descriptorInfo.size(), 0);
            std::vector<VkDescriptorSetLayoutBinding> bindings(item.descriptorInfo.size());

            bool hasImage = false;
            bool hasUBO = false;
            bool hasStorage = false;

            for (size_t j = 0; j < item.descriptorInfo.size(); j++)
            {
                auto& descriptor = item.descriptorInfo[j];

                VkDescriptorSetLayoutBinding descriptorBinding{};
                descriptorBinding.binding = descriptor.binding;
                descriptorBinding.descriptorCount = descriptor.count;
                descriptorBinding.descriptorType = DescriptorTypeToVK(descriptor.type);
                descriptorBinding.stageFlags = ShaderStageToVK(descriptor.stage);

                if (descriptor.type == Render::DescriptorInfo::DescriptorType::IMAGE) {
                    if (hasImage) throw std::runtime_error("cant have multiple image descriptor");
                    else hasImage = true;

                    uint32_t maxTextures = props.limits.maxPerStageDescriptorSamplers;
                    if (maxTextures < descriptor.count) throw std::runtime_error("asked texture amount not available on the GPU");

                    bindingFlags[j] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
                }
                else if(descriptor.type == Render::DescriptorInfo::DescriptorType::UNIFORM){
                    if (hasUBO) throw std::runtime_error("cant have multiple uniform buffer descriptor");
                    else hasUBO = true;
                }
                else if (descriptor.type == Render::DescriptorInfo::DescriptorType::STORAGE) {
                    if (hasStorage) throw std::runtime_error("cant have multiple storage buffer descriptor");
                    else hasStorage = true;
                }

                bindings[j] = descriptorBinding;
            }

            VkDescriptorSetLayoutBindingFlagsCreateInfoEXT flagsInfo{};
            flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
            flagsInfo.bindingCount = bindingFlags.size();
            flagsInfo.pBindingFlags = bindingFlags.data();

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            layoutInfo.pBindings = bindings.data();
            layoutInfo.pNext = &flagsInfo;

            if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &(surface->descriptorSetLayouts[i])) != VK_SUCCESS) {
                throw std::runtime_error("failed to create descriptor set layout!");
            }
        }
    }

    // creates the pool for the descriptor sets
    void CreateDescriptorPool(SurfaceVulkanData *surface) {
        std::unordered_map<VkDescriptorType, uint32_t> counts;

        for (auto& set : surface->pipelineData.descriptorSets) {
            for (auto& descriptor : set.descriptorInfo) {
                VkDescriptorType type = DescriptorTypeToVK(descriptor.type);
                counts[type] += MAX_FRAMES_IN_FLIGHT * descriptor.count;
            }
        }

        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(counts.size());

        for (auto& [type, count] : counts) {
            VkDescriptorPoolSize ps{};
            ps.type = type;
            ps.descriptorCount = count;
            poolSizes.push_back(ps);
        }

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * surface->pipelineData.descriptorSets.size());

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &surface->descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    // creates the actual descriptor sets
    void CreateDescriptorSets(SurfaceVulkanData* surface) {
        std::vector<uint32_t> variableCountsPerSet;

        for (auto& set : surface->pipelineData.descriptorSets) {
            uint32_t countForThisSet = 0;

            for (auto& descriptor : set.descriptorInfo) {
                if (descriptor.type == Render::DescriptorInfo::DescriptorType::IMAGE)
                    countForThisSet = descriptor.count;    
            }

            variableCountsPerSet.push_back(countForThisSet);
        }

        std::vector<uint32_t> counts;

        for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
            for (auto c : variableCountsPerSet) {
                counts.push_back(c);
            }
        }

        VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableCount{};
        variableCount.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
        variableCount.descriptorSetCount = counts.size();
        variableCount.pDescriptorCounts = counts.data();

        std::vector<VkDescriptorSetLayout> layouts;
        std::vector<VkDescriptorSet> flatLayouts;

        for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
            for (auto layout : surface->descriptorSetLayouts) {
                layouts.push_back(layout);
                flatLayouts.push_back(VK_NULL_HANDLE);
            }
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = surface->descriptorPool;
        allocInfo.descriptorSetCount = layouts.size();
        allocInfo.pSetLayouts = layouts.data();
        allocInfo.pNext = &variableCount;

        if (vkAllocateDescriptorSets(device, &allocInfo, flatLayouts.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        surface->descriptorSets.resize(surface->descriptorSetLayouts.size());
        for (size_t i = 0; i < surface->descriptorSetLayouts.size(); i++)
            surface->descriptorSets[i].resize(MAX_FRAMES_IN_FLIGHT);

        size_t index = 0;
        for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
            for (size_t s = 0; s < surface->descriptorSetLayouts.size(); s++) {
                surface->descriptorSets[s][f] = flatLayouts[index++];
            }
        }

        for (size_t i = 0; i < surface->pipelineData.descriptorSets.size(); i++)
        {
            auto& set = surface->pipelineData.descriptorSets[i];

            std::vector<VkWriteDescriptorSet> writes;
            std::vector<VkDescriptorImageInfo> imageInfos;
            VkDescriptorBufferInfo uniformBufferInfo{};
            VkDescriptorBufferInfo storageBufferInfo{};

            for (size_t j = 0; j < set.descriptorInfo.size(); j++)
            {
                auto& descriptor = set.descriptorInfo[j];
                if (descriptor.type == Render::DescriptorInfo::DescriptorType::UNIFORM)
                    CreateUniformBuffers(surface, descriptor.data, i);
                else if (descriptor.type == Render::DescriptorInfo::DescriptorType::STORAGE)
                    CreateStorageBuffers(surface, descriptor.data, i);

                for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {

                    VkWriteDescriptorSet write{};
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = surface->descriptorSets[i][f];
                    write.dstBinding = descriptor.binding;
                    write.dstArrayElement = 0;

                    if (descriptor.type == Render::DescriptorInfo::DescriptorType::UNIFORM) {
                        uniformBufferInfo.buffer = surface->uniformBuffers[i][f];
                        uniformBufferInfo.offset = 0;
                        uniformBufferInfo.range = descriptor.data;

                        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                        write.descriptorCount = 1;
                        write.pBufferInfo = &uniformBufferInfo;

                        writes.push_back(write);
                    }
                    else if (descriptor.type == Render::DescriptorInfo::DescriptorType::STORAGE) {
                        storageBufferInfo.buffer = surface->storageBuffers[i][f];
                        storageBufferInfo.offset = 0;
                        storageBufferInfo.range = descriptor.data;

                        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                        write.descriptorCount = 1;
                        write.pBufferInfo = &storageBufferInfo;

                        writes.push_back(write);
                    }
                    else { 
                        imageInfos.resize(descriptor.count);
                        for (auto& info : imageInfos) {
                            info.sampler = samplerAccess[descriptor.data];
                            info.imageView = VK_NULL_HANDLE;
                            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        }

                        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        write.descriptorCount = descriptor.count;
                        write.pImageInfo = imageInfos.data();

                        writes.push_back(write);
                    }
                }
            }

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    void UpdateTextureDescriptor(int index, VkImageView textureView) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureView;
        imageInfo.sampler = samplerAccess[0];

        for (auto &[window, windowData] : windows) {
            for (auto &[surface, surfaceData] : windowData->surfaces) {
                for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                    VkWriteDescriptorSet write{};
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = surfaceData.descriptorSets[0][i];
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
    void RecordCommandBuffer(SurfaceVulkanData *surface, VkExtent2D& extent, uint32_t& imageIndex, VkSurfaceKHR surfaceKey) {
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
         
        std::array<VkClearValue, 2> clearValues{}; // he???
        clearValues[0].color = { {0.388235f, 0.643137f, 0.839216f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };



        VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
        colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachmentInfo.imageView = surface->swapChainImageViews[imageIndex];
        colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentInfo.clearValue = clearValues[0];

        
        VkRenderingAttachmentInfoKHR depthAttachmentInfo{};
        depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachmentInfo.imageView = surface->depthImageView;
        depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentInfo.clearValue = clearValues[1];

        VkRect2D renderArea{};
        renderArea.offset = { 0, 0 };
        renderArea.extent = extent;

        VkRenderingInfoKHR renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderInfo.renderArea = renderArea;
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachmentInfo;
        renderInfo.pDepthAttachment = (surface->pipelineData.depthTestEnable) ? &depthAttachmentInfo : nullptr;


        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageMemoryBarrier.image = surface->swapChainImages[imageIndex];
        imageMemoryBarrier.subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
        };

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        vkCmdBeginRendering(cmdBuffer, &renderInfo);

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
        scissor.offset = { 0, 0 };
        scissor.extent = extent;
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        // binds the vertex buffers to the said bindings
        VkBuffer vertexBuffers[] = { surface->vertexBuffer };
        VkDeviceSize offsets[] = { 0 };  // set where to start reading vertex data from
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

        // binds the index buffer to the said binding
        vkCmdBindIndexBuffer(cmdBuffer, surface->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, surface->layout, 0, 1, &surface->descriptorSets[0][frame], 0, nullptr);

        if(surface->pipelineData.constantsSize > 0)
            vkCmdPushConstants(cmdBuffer, surface->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, surface->pipelineData.constantsSize, constantsData[surfaceKey]);

        // draw call
        vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(surface->indiceCount), 1, 0, 0, 0);

        // ending the render pass
        vkCmdEndRendering(cmdBuffer);

        imageMemoryBarrier = VkImageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        imageMemoryBarrier.image = surface->swapChainImages[imageIndex];
        imageMemoryBarrier.subresourceRange = {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
        };

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

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

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };

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
    VkFormat findDepthFormat() { return findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT); }
    bool hasStencilComponent(VkFormat format) { return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT; }

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
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

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
    void CreateGraphicPipeline(WindowVulkanData *window, SurfaceVulkanData* surface, VkSurfaceKHR surfaceRef) {

        CreateDescriptorSetLayout(surface);
        vertexDataLayout[surfaceRef] = surface->pipelineData.vertexDataLayout;

        if (surface->pipelineData.depthTestEnable || surface->pipelineData.depthWriteEnable)
            CreateDepthResources(surface, window->swapChainExtent);


        // reads in the binary shader data
        CompileShader(surface->pipelineData.vertexShader, "vert");
        auto vertShaderCode = readFile("vert.spv");
        CompileShader(surface->pipelineData.fragmentShader, "frag");
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

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // vertex data specifications
        auto bindingDescription = GetVertexBindingDescription(surfaceRef);
        auto attributeDescriptions = GetVertexAttributeDescriptions(surfaceRef);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        //!!! what type of data it uses (lines, triangles), and how (reusing vertecies or not)
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = surface->pipelineData.topology;                                    //feature
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
        scissor.offset = { 0, 0 };
        scissor.extent = window->swapChainExtent;

        // handles what should be dinamic during runtime
        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // viewport create info
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.pScissors = &scissor;

        // rasterizer data
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;

        // set how fragments are generated based on geometry (point, line, fill)
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;

        // culling
        rasterizer.cullMode = surface->pipelineData.cullMode;                                   //feature
        rasterizer.frontFace = surface->pipelineData.frontFace;                                 //feature

        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;  // Optional
        rasterizer.depthBiasClamp = 0.0f;           // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f;     // Optional

        // multisampling - a.k.a. easy anti-alliasing
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = surface->pipelineData.rasterizationSamples;        //feature
        multisampling.minSampleShading = 1.0f;           // Optional
        multisampling.pSampleMask = nullptr;             // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE;  // Optional
        multisampling.alphaToOneEnable = VK_FALSE;       // Optional

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = surface->pipelineData.blendEnable;                   //feature
        colorBlendAttachment.srcColorBlendFactor = surface->pipelineData.srcColorBlendFactor;   //feature
        colorBlendAttachment.dstColorBlendFactor = surface->pipelineData.dstColorBlendFactor;   //feature
        colorBlendAttachment.colorBlendOp = surface->pipelineData.colorBlendOp;                 //feature
        colorBlendAttachment.srcAlphaBlendFactor = surface->pipelineData.srcAlphaBlendFactor;   //feature
        colorBlendAttachment.dstAlphaBlendFactor = surface->pipelineData.dstAlphaBlendFactor;   //feature
        colorBlendAttachment.alphaBlendOp = surface->pipelineData.alphaBlendOp;                 //feature

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = surface->pipelineData.depthTestEnable;                   //feature
        depthStencil.depthWriteEnable = surface->pipelineData.depthWriteEnable;                 //feature
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;  // Optional
        depthStencil.maxDepthBounds = 1.0f;  // Optional

        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};  // Optional
        depthStencil.back = {};   // Optional

        VkFormat depthFormat = findDepthFormat(); // e.g., VK_FORMAT_D32_SFLOAT

        VkPipelineRenderingCreateInfo renderCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapChainImageFormat.format,
            .depthAttachmentFormat = depthFormat
        };

        VkPushConstantRange constRange{};
        constRange.offset = 0;
        constRange.size = surface->pipelineData.constantsSize;
        constRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; //tmp checkpoint

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = surface->descriptorSetLayouts.size();
        pipelineLayoutInfo.pSetLayouts = surface->descriptorSetLayouts.data();
        if (constRange.size > 0) {
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &constRange;
        }

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &surface->layout) != VK_SUCCESS) {
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

        pipelineInfo.layout = surface->layout;

        pipelineInfo.renderPass = nullptr;
        pipelineInfo.pNext = &renderCreateInfo;

        // can derive render passes from one-another so it can have a parent-child hierarchy, faster, easier
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &surface->pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);

        CreateDescriptorPool(surface);
        CreateDescriptorSets(surface);
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

            VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

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
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

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

    //vertex data layout
    uint32_t GetVertexDataSize(Render::VertexDataType type) {
        switch (type) {
        case FLOAT:
            return sizeof(float);
            break;
        case UINT:
            return sizeof(unsigned int);
            break;
        case VEC2:
            return sizeof(float)*2;
            break;
        case VEC3:
            return sizeof(float)*3;
            break;
        case VEC4:
            return sizeof(float)*4;
            break;
        }
    }

    VkFormat GetVertexDataFormat(Render::VertexDataType type) {
        switch (type) {
        case FLOAT:
            return VK_FORMAT_R32_SFLOAT;
            break;
        case UINT:
            return VK_FORMAT_R16_UINT;
            break;
        case VEC2:
            return VK_FORMAT_R32G32_SFLOAT;
            break;
        case VEC3:
            return VK_FORMAT_R32G32B32_SFLOAT;
            break;
        case VEC4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
            break;
        }
    }

    VkVertexInputBindingDescription GetVertexBindingDescription(VkSurfaceKHR surface) {
        VkVertexInputBindingDescription bindingDescription{};

        bindingDescription.binding = 0;

        uint32_t stride = 0;
        for (auto& item : vertexDataLayout[surface]) {
            stride += GetVertexDataSize(item);
        }

        bindingDescription.stride = stride;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    std::vector<VkVertexInputAttributeDescription> GetVertexAttributeDescriptions(VkSurfaceKHR surface) {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

        uint32_t stride = 0;
        int location = 0;
        for (auto& item : vertexDataLayout[surface]) {

            VkVertexInputAttributeDescription description{};

            description.binding = 0;
            description.location = location++;
            description.format = GetVertexDataFormat(item);
            description.offset = stride;

            stride += GetVertexDataSize(item);
            attributeDescriptions.push_back(description);
        }

        return attributeDescriptions;
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
        std::string msg = pCallbackData->pMessage;

        //we dont talk about the api version around here
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

int Render::CreateSurface(Window window, CreateGraphicPipeLineInfo graphicPipeLineInfo) { return instance->CreateSurface(window, graphicPipeLineInfo); }

void Render::Draw(int surface) { instance->Draw(surface); }

void Render::Update() { instance->Update(); }

bool Render::IsValidSurface(int surfaceIndex) { return instance->IsValidSurface(surfaceIndex); }

int Render::AddUIElementData(UIRenderData &data) { return instance->AddUIElementData(data); }

int Render::CreateTexture(std::string path) { return instance->CreateTexture(path); }

void Render::Init(bool debugging) { instance = new Render::Vulkan(debugging); }

void Render::Clean() { delete instance; }

int Render::CreateFontPage(const std::vector<uint8_t> &rgbaData, uint32_t width, uint32_t height) { return instance->CreateFontPage(rgbaData, width, height); };

void* Render::GetWindowOfSurface(int surface) {
    return instance->GetWindowOfSurface(surface);
}

int Render::CreateSampler(SamplerFilter filter, SamplerAddressing addressing, SamplerMipmapMode mipmapMode) {
    return instance->CreateSampler(filter, addressing, mipmapMode);
}

Render::DescriptorInfo Render::CreateUniformDescriptor(int binding, int size, int stage) {
    DescriptorInfo output;

    output.type = Render::DescriptorInfo::DescriptorType::UNIFORM;
    output.binding = binding;
    output.count = 1;
    output.stage = (Render::ShaderStage)stage;
    output.data = size;

    return output;
}
Render::DescriptorInfo Render::CreateStorageDescriptor(int binding, int size, int stage) {
    DescriptorInfo output;

    output.type = Render::DescriptorInfo::DescriptorType::STORAGE;
    output.binding = binding;
    output.count = 1;
    output.stage = (Render::ShaderStage)stage;
    output.data = size;

    return output;
}
Render::DescriptorInfo Render::CreateImageDescriptor(int binding, int count, int sampler, int stage) {
    DescriptorInfo output;

    output.type = Render::DescriptorInfo::DescriptorType::IMAGE;
    output.binding = binding;
    output.count = count;
    output.stage = (Render::ShaderStage)stage;
    output.data = sampler;

    return output;
}

void Render::PushConstants(int surface, void* data, uint32_t size) {
    instance->PushConstants(surface, data, size);
}

Render::Vulkan *Render::instance = nullptr;
}  // namespace Ignis
