#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#include "../IgnisLib.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#define NOMINMAX

#include <array>
#include <filesystem>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define MAX_FRAMES_IN_FLIGHT 2
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
using RenderData = Render::RenderData;
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
struct PipeLine {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    std::vector<std::string> constants;
    std::vector<int> descriptorIds;
    bool needDepth;
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

    std::vector<std::string> constants;
    std::vector<int> descriptorSetIds;

    std::vector<Render::VertexDataType> vertexDataLayout;
};

struct CreateSamplerVKConvert {
    VkFilter filter;
    VkSamplerMipmapMode mipmapMode;
    VkSamplerAddressMode addressMode;
};

struct BindPlan {
    uint32_t pipelineIdx;
    std::vector<Render::DescriptorSetId> needsBinding;
};

struct SurfaceVulkanData {
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<BindPlan> pipelineBindOrdering;
    bool isOrderingValid{ false };

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

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    size_t currentFrame = 0;

    std::vector<int> pipelines;
};

struct WindowVulkanData {
    std::unordered_map<VkSurfaceKHR, SurfaceVulkanData> surfaces;
    VkExtent2D swapChainExtent;
    VkSurfaceCapabilitiesKHR capabilities;
    bool framebufferResized = false;
};

struct VulkanConstData {
    void *data;
    uint32_t size;
};

struct DrawData {
    SurfaceVulkanData *data;
    GLFWwindow *window;
    VkSurfaceKHR surface;
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

    switch (type) {
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
    if (stage & Render::ShaderStage::VERTEX) output |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stage & Render::ShaderStage::FRAGMENT) output |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stage & Render::ShaderStage::COMPUTE) output |= VK_SHADER_STAGE_COMPUTE_BIT;
    return output;
}

struct BufferData {
    VkBuffer buffer;
    VkDeviceMemory bufferMemory;
    void *bufferMapped;
};

struct DescriptorSet {
    // NOTE: maybe reuse layouts between sets
    VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSet{ VK_NULL_HANDLE };

    std::array<std::unique_ptr<BufferData>, MAX_FRAMES_IN_FLIGHT> uniformBuffer{ nullptr };
    std::array<std::unique_ptr<BufferData>, MAX_FRAMES_IN_FLIGHT> storageBuffer{ nullptr };

    int textureBinding = -1;
    int samplerId = -1;

    uint32_t setIdx;
};

class Render::Vulkan {
   private:
    struct SwapChainSupportDetails {
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct SurfaceAccess {
        GLFWwindow *window;
        VkSurfaceKHR surface;
    };

    const std::vector<const char *> validationLayers = { "VK_LAYER_KHRONOS_validation" };

    bool enableValidationLayers = false;

    const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME
    };

    bool firstSurface = true;

    std::unordered_map<GLFWwindow *, std::unique_ptr<WindowVulkanData>> windows;
    VkInstance instance;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    VkQueue transferQueue;

    VkSurfaceKHR surface;

    SwapChainSupportDetails swapChainSupport;
    VkSurfaceFormatKHR swapChainImageFormat;
    VkPresentModeKHR swapChainPresentMode;

    VkCommandPool commandPool;

    // mapped with set numbers
    std::unordered_map<int, DescriptorSet> descriptorSets;
    int nextDescriptorSetId = 0;
    std::vector<VkDescriptorPool> descriptorPools;

    std::unordered_map<int, TextureData> textureData;
    unsigned int nextTexture = 1;

    std::unordered_map<int, RenderData> renderData;

    std::unordered_map<int, SurfaceAccess> surfaceAccess;
    int nextSurface = 0;

    std::vector<CreateSamplerVKConvert> samplerCreateDatas;
    std::unordered_map<int, VkSampler> samplerAccess;
    int nextSamplerData = 0;
    int nextSampler = 0;

    std::unordered_map<int, PipeLine> pipelines;
    int nextPipeline = 0;

    std::unordered_map<std::string, VulkanConstData> constantsData;

    std::unordered_map<VkSurfaceKHR, std::vector<Render::VertexDataType>> vertexDataLayout;

    std::vector<int> drawQueue;

   public:
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

    inline DescriptorSet &DescriptorSetFromId(const DescriptorSetId id) {
        return descriptorSets.at(id.set);
    }

    std::vector<int> CreateDescSets(std::vector<DescriptorSetInfo> &descriptorSetInfos) {
        return CreateDescriptorSets(descriptorSetInfos);
    }

    int CreateFontPage(const std::vector<uint8_t> &rgbaData, uint32_t width, uint32_t height) {
        TextureData data;
        CreateTextureImageFromMemory(rgbaData.data(), width, height, data.textureImage, data.textureImageMemory);
        data.textureImageView = CreateTextureImageView(data.textureImage);
        textureData[nextTexture] = data;
        UpdateTextureDescriptor(descriptorSets[0], nextTexture, data.textureImageView);
        return nextTexture++;  // returns texture slot ID
    }

    int CreatePipeline(CreateGraphicPipeLineInfo graphicPipeLineInfo) {
        // create the rendering procedure that the data passes to be rendered
        return CreateGraphicPipelines(RenderPassInfoToVK(graphicPipeLineInfo));
    }

    Render::Texture CreateTexture(int descriptorSet, std::string path) {
        if (!descriptorSets.contains(descriptorSet))
            throw std::runtime_error("can't add texture to descriptor, descriptor id is invalid");

        TextureData data;
        CreateTextureImage(path, data.textureImage, data.textureImageMemory);
        data.textureImageView = CreateTextureImageView(data.textureImage);

        textureData[nextTexture] = data;

        UpdateTextureDescriptor(descriptorSets[descriptorSet], nextTexture, data.textureImageView);

        Render::Texture texture{};
        texture.id = nextTexture++;
        return texture;
    }

    static void FramebufferResizeCallback(Window windowIn) {
        auto window = reinterpret_cast<WindowUserPointer *>(glfwGetWindowUserPointer(windowIn.ptr));
        ((WindowVulkanData *)window->vulkanData)->framebufferResized = true;
    }

    void Update() {
#ifndef IGNIS_INPUT
        glfwPollEvents();
#endif
        vkDeviceWaitIdle(device);

        for (auto &window : windows) {
            if (glfwWindowShouldClose(window.first) || window.second->surfaces.size() == 0) {
                CloseWindow(window.first);
                break;  // Idk why, but we CANT remove this break, the world will fall into ruin...
            }
        }

        // if element changed update the vertex & index buffer for that surface
        UpdateElementBuffers();

        DrawFrame();
        drawQueue.clear();
    }

    bool IsValidSurface(int surfaceIndex) { return surfaceAccess.find(surfaceIndex) != surfaceAccess.end(); }

    void Draw(int surfaceIndex) {
        if (surfaceAccess.contains(surfaceIndex)) {
            drawQueue.push_back(surfaceIndex);
        } else
            throw std::runtime_error("invalid surface!");
    }

    void Clear(Window &window) {
        if (windows.contains(window.ptr)) {
            ClearBuffersForWindow(window.ptr);
        } else
            throw std::runtime_error("invalid window!");
    }

    void AddElementData(RenderData &data) {
        renderData[data.surface] = data;
        UpdateElementBuffers();
    }

    void *GetWindowOfSurface(int surface) {
        return surfaceAccess[surface].window;
    }

    int CreateSampler(SamplerFilter filter, SamplerAddressing addressing, SamplerMipmapMode mipmapMode) {
        CreateSamplerVKConvert samplerVK = SamplerInfoToVK(filter, addressing, mipmapMode);
        return CreateSamplerFromData(samplerVK);
    }

    void PushConstants(std::string &name, void *data, uint32_t size) {
        if (constantsData.contains(name) && constantsData[name].size == size) {
            memcpy(constantsData[name].data, data, size);
        }
    }

    void PublishConstants(std::vector<Render::ConstData> data) {
        for (size_t i = 0; i < data.size(); i++) {
            std::string name = data[i].name;
            uint32_t size = data[i].size;
            if (constantsData.contains(name)) {
                free(constantsData[name].data);
            }

            constantsData[name].size = data[i].size;
            constantsData[name].data = calloc(1, size);
        }
    }

   private:
    CreateGraphicPipeLineInfoVKConvert RenderPassInfoToVK(CreateGraphicPipeLineInfo info) {
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
                case CreateGraphicPipeLineInfo::BlendFactor::SrcAlpha:
                    return VK_BLEND_FACTOR_SRC_ALPHA;
                case CreateGraphicPipeLineInfo::BlendFactor::DstAlpha:
                    return VK_BLEND_FACTOR_DST_ALPHA;
                case CreateGraphicPipeLineInfo::BlendFactor::OneMinusSrcAlpha:
                    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                case CreateGraphicPipeLineInfo::BlendFactor::OneMinusDstAlpha:
                    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
                case CreateGraphicPipeLineInfo::BlendFactor::SrcColor:
                    return VK_BLEND_FACTOR_SRC_COLOR;
                case CreateGraphicPipeLineInfo::BlendFactor::DstColor:
                    return VK_BLEND_FACTOR_DST_COLOR;
                case CreateGraphicPipeLineInfo::BlendFactor::OneMinusSrcColor:
                    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
                case CreateGraphicPipeLineInfo::BlendFactor::OneMinusDstColor:
                    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            }
            return VK_BLEND_FACTOR_ONE;  // fallback
        };

        auto ConvertBlendOp = [](CreateGraphicPipeLineInfo::BlendMode mode) -> VkBlendOp {
            switch (mode) {
                case CreateGraphicPipeLineInfo::BlendMode::Add:
                    return VK_BLEND_OP_ADD;
                case CreateGraphicPipeLineInfo::BlendMode::Sub:
                    return VK_BLEND_OP_SUBTRACT;
                case CreateGraphicPipeLineInfo::BlendMode::Max:
                    return VK_BLEND_OP_MAX;
                case CreateGraphicPipeLineInfo::BlendMode::Min:
                    return VK_BLEND_OP_MIN;
            }
            return VK_BLEND_OP_ADD;  // fallback
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

        output.constants = std::move(info.constants);

        output.descriptorSetIds = info.descriptorSetIds;

        output.vertexDataLayout = std::move(info.vertexDataLayout);

        return output;
    }

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

        vkDestroyBuffer(device, data->vertexBuffer, nullptr);
        vkFreeMemory(device, data->vertexBufferMemory, nullptr);

        vkDestroyBuffer(device, data->indexBuffer, nullptr);
        vkFreeMemory(device, data->indexBufferMemory, nullptr);

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
        for (auto &[surface, consta] : constantsData) {
            if (consta.data != nullptr) free(consta.data);
        }

        for (auto &[id, pipeline] : pipelines) {
            vkDestroyPipeline(device, pipeline.pipeline, nullptr);
            vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
        }

        for (auto &[_, set] : descriptorSets) {
            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                if (set.storageBuffer[i]) {
                    vkDestroyBuffer(device, set.storageBuffer[i]->buffer, nullptr);
                    vkFreeMemory(device, set.storageBuffer[i]->bufferMemory, nullptr);
                }
                if (set.uniformBuffer[i]) {
                    vkDestroyBuffer(device, set.uniformBuffer[i]->buffer, nullptr);
                    vkFreeMemory(device, set.uniformBuffer[i]->bufferMemory, nullptr);
                }
            }
            vkDestroyDescriptorSetLayout(device, set.descriptorSetLayout, nullptr);
        }

        for (auto &pool : descriptorPools) {
            vkDestroyDescriptorPool(device, pool, nullptr);
        }

        for (auto &[window, windowData] : windows) {
            for (auto &[surface, surfaceData] : windowData->surfaces) {
                CleanUpSurface(&surfaceData);
                vkDestroySurfaceKHR(instance, surface, nullptr);
            }
        }

        for (auto &[samplerId, sampler] : samplerAccess) {
            vkDestroySampler(device, sampler, nullptr);
        }

        for (auto &[textureId, texture] : textureData) {
            vkDestroyImageView(device, texture.textureImageView, nullptr);
            vkDestroyImage(device, texture.textureImage, nullptr);
            vkFreeMemory(device, texture.textureImageMemory, nullptr);
        }

        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyDevice(device, nullptr);

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

    void ClearBuffersForWindow(GLFWwindow *window) {
        std::vector<DrawData> surfaces;

        for (auto &[surface, surfaceData] : windows[window]->surfaces) {
            DrawData data;
            data.window = window;
            data.surface = surface;
            data.data = &surfaceData;

            surfaces.push_back(data);
        }

        if (surfaces.empty()) return;

        for (size_t i = 0; i < surfaces.size(); i++) {
            uint32_t imageIndex;

            vkWaitForFences(device, 1, &surfaces[i].data->inFlightFences[surfaces[i].data->currentFrame], VK_TRUE, UINT64_MAX);

            VkResult result = vkAcquireNextImageKHR(device, surfaces[i].data->swapChain, UINT64_MAX, surfaces[i].data->imageAvailableSemaphores[surfaces[i].data->currentFrame], VK_NULL_HANDLE, &imageIndex);

            // check if swapchain recreation is necessary
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                RecreateSwapChain(surfaces[i].window, surfaces[i].surface);
                return;
            } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            vkResetFences(device, 1, &surfaces[i].data->inFlightFences[surfaces[i].data->currentFrame]);

            vkResetCommandBuffer(surfaces[i].data->commandBuffers[surfaces[i].data->currentFrame], 0);
            RecordClearCommand(surfaces[i].data, imageIndex);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &surfaces[i].data->commandBuffers[surfaces[i].data->currentFrame];

            if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, surfaces[i].data->inFlightFences[surfaces[i].data->currentFrame]) != VK_SUCCESS) {
                throw std::runtime_error("failed to submit draw command buffer!");
            }
        }
    }

    // update
    void DrawFrame() {
        // surfaceAccess[surfaceIndex].window, &surfaceAccess[surfaceIndex].surface

        std::vector<DrawData> surfaces;

        std::vector<VkFence> fences;
        std::vector<VkSemaphore> finishSemaphores;
        std::vector<uint32_t> images;
        std::vector<VkSwapchainKHR> swapChains;

        for (size_t i = 0; i < drawQueue.size(); i++) {
            int &index = drawQueue[i];
            if (!IsValidSurface(index)) continue;

            DrawData data;
            data.window = surfaceAccess[index].window;
            data.surface = surfaceAccess[index].surface;
            data.data = &windows[data.window]->surfaces[data.surface];
            if (data.data->haveVertexData) {
                surfaces.push_back(data);
                fences.push_back(data.data->inFlightFences[data.data->currentFrame]);
                finishSemaphores.push_back(data.data->renderFinishedSemaphores[data.data->currentFrame]);
                swapChains.push_back(data.data->swapChain);
            }
        }

        if (surfaces.empty()) return;

        images.resize(surfaces.size());
        // waits for last frame to complete (for the fence), then resets it
        vkWaitForFences(device, fences.size(), fences.data(), VK_TRUE, UINT64_MAX);

        for (size_t i = 0; i < surfaces.size(); i++) {
            // aquires the next available image, when it did, it signals the semaphore
            VkResult result = vkAcquireNextImageKHR(device, surfaces[i].data->swapChain, UINT64_MAX, surfaces[i].data->imageAvailableSemaphores[surfaces[i].data->currentFrame], VK_NULL_HANDLE, &images[i]);

            // check if swapchain recreation is necessary
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                RecreateSwapChain(surfaces[i].window, surfaces[i].surface);
                return;
            } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("failed to acquire swap chain image!");
            }
            vkResetFences(device, 1, &surfaces[i].data->inFlightFences[surfaces[i].data->currentFrame]);

            // resets and records the command buffer
            vkResetCommandBuffer(surfaces[i].data->commandBuffers[surfaces[i].data->currentFrame], 0);
            RecordCommandBuffer(surfaces[i].data, windows[surfaces[i].window]->swapChainExtent, images[i], surfaces[i].surface);

            // submiting it to the graphics family queue
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            // what semaphore to wait for
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &surfaces[i].data->imageAvailableSemaphores[surfaces[i].data->currentFrame];
            submitInfo.pWaitDstStageMask = waitStages;
            // assigning the command buffer
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &surfaces[i].data->commandBuffers[surfaces[i].data->currentFrame];
            // what semaphore to signal when the command buffer finished execution
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &surfaces[i].data->renderFinishedSemaphores[surfaces[i].data->currentFrame];

            if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, surfaces[i].data->inFlightFences[surfaces[i].data->currentFrame]) != VK_SUCCESS) {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            surfaces[i].data->currentFrame = (surfaces[i].data->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        // presents the drawn image to the swapchain/queue? idk
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = finishSemaphores.size();
        presentInfo.pWaitSemaphores = finishSemaphores.data();

        presentInfo.swapchainCount = swapChains.size();
        presentInfo.pSwapchains = swapChains.data();
        presentInfo.pImageIndices = images.data();
        presentInfo.pResults = nullptr;

        VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

        for (size_t i = 0; i < surfaces.size(); i++) {
            // check if swapchain recreation is necessary
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || windows[surfaces[i].window]->framebufferResized) {
                windows[surfaces[i].window]->framebufferResized = false;
                RecreateSwapChain(surfaces[i].window, surfaces[i].surface);
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("failed to present swap chain image!");
            }
        }
    }

    void UpdateUniformBufferSpin(BufferData *bufferData, WindowVulkanData *window, glm::vec3 dir) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f),
                                time * 2 * glm::radians(20.0f),
                                dir);  // Y-axis

        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        ubo.proj = glm::perspective(glm::radians(45.0f), window->swapChainExtent.width / (float)window->swapChainExtent.height, 0.1f, 10.0f);

        ubo.proj[1][1] *= 1;

        memcpy(bufferData->bufferMapped, &ubo, sizeof(ubo));
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

        // CreateSwapChain(data->swapChain, data->swapChainImages, windows[window].get(), window, surface);
        // CreateImageViews(data->swapChainImageViews, data->swapChainImages);
        CreateDepthResources(data, windows[window]->swapChainExtent);
    }

    // assumes one element per surface
    void UpdateElementBuffers() {
        for (auto &[surfaceId, element] : renderData) {
            if (element.changed) {
                SurfaceAccess acces = surfaceAccess[surfaceId];
                SurfaceVulkanData *surfaceData = &windows[acces.window]->surfaces[acces.surface];
                CreateVertexBuffer(surfaceData, element.vertecies);
                CreateIndexBuffer(surfaceData, element.indicies);
                surfaceData->indiceCount = element.indicies.size();
                element.changed = false;
            }
        }
    }

    int CreateSamplerFromData(CreateSamplerVKConvert &samplerVK) {
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

        if (vkCreateSampler(device, &samplerInfo, nullptr, &samplerAccess[nextSampler]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }

        return nextSampler++;
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
    void CreateUniformBuffer(std::array<std::unique_ptr<BufferData>, MAX_FRAMES_IN_FLIGHT> &bufferData, int size) {
        VkDeviceSize bufferSize = size;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            bufferData[i] = std::make_unique<BufferData>();

            CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, bufferData[i]->buffer, bufferData[i]->bufferMemory);

            vkMapMemory(device, bufferData[i]->bufferMemory, 0, bufferSize, 0, &bufferData[i]->bufferMapped);
        }
    }

    void CreateStorageBuffers(std::array<std::unique_ptr<BufferData>, MAX_FRAMES_IN_FLIGHT> &bufferData, int size) {
        VkDeviceSize bufferSize = size;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            bufferData[i] = std::make_unique<BufferData>();

            CreateBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, bufferData[i]->buffer, bufferData[i]->bufferMemory);

            vkMapMemory(device, bufferData[i]->bufferMemory, 0, bufferSize, 0, &bufferData[i]->bufferMapped);
        }
    }

    // makes the descriptors for "Uniform"(set) bindings, need to make all of them here
    int CreateDescriptorSetLayout(DescriptorSetInfo &descriptorSetInfo) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        std::vector<VkDescriptorBindingFlagsEXT> bindingFlags(descriptorSetInfo.descriptorInfo.size(), 0);
        std::vector<VkDescriptorSetLayoutBinding> bindings(descriptorSetInfo.descriptorInfo.size());

        bool hasImage = false;
        bool hasUBO = false;
        bool hasStorage = false;

        for (size_t j = 0; j < descriptorSetInfo.descriptorInfo.size(); j++) {
            auto &descriptor = descriptorSetInfo.descriptorInfo[j];

            VkDescriptorSetLayoutBinding descriptorBinding{};
            descriptorBinding.binding = descriptor.binding;
            descriptorBinding.descriptorCount = descriptor.count;
            descriptorBinding.descriptorType = DescriptorTypeToVK(descriptor.type);
            descriptorBinding.stageFlags = ShaderStageToVK(descriptor.stage);

            if (descriptor.type == Render::DescriptorInfo::DescriptorType::IMAGE) {
                if (hasImage)
                    throw std::runtime_error("cant have multiple image descriptor");
                else
                    hasImage = true;

                uint32_t maxTextures = props.limits.maxPerStageDescriptorSamplers;
                if (maxTextures < static_cast<uint32_t>(descriptor.count)) throw std::runtime_error("asked texture amount not available on the GPU");

                bindingFlags[j] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
            } else if (descriptor.type == Render::DescriptorInfo::DescriptorType::UNIFORM) {
                if (hasUBO)
                    throw std::runtime_error("cant have multiple uniform buffer descriptor");
                else
                    hasUBO = true;
            } else if (descriptor.type == Render::DescriptorInfo::DescriptorType::STORAGE) {
                if (hasStorage)
                    throw std::runtime_error("cant have multiple storage buffer descriptor");
                else
                    hasStorage = true;
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

        // NOTE: descriptorSets IS not initialized
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &(descriptorSets[nextDescriptorSetId].descriptorSetLayout)) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        return nextDescriptorSetId++;
    }

    // creates the pool for the descriptor sets
    void CreateDescriptorPool(std::vector<DescriptorSetInfo> &descriptorSetInfos) {
        std::unordered_map<VkDescriptorType, uint32_t> counts;

        for (auto &info : descriptorSetInfos) {
            for (auto &descriptor : info.descriptorInfo) {
                VkDescriptorType type = DescriptorTypeToVK(descriptor.type);
                counts[type] += MAX_FRAMES_IN_FLIGHT * descriptor.count;
            }
        }

        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(counts.size());

        for (auto &[type, count] : counts) {
            VkDescriptorPoolSize ps{};
            ps.type = type;
            ps.descriptorCount = count;
            poolSizes.push_back(ps);
        }

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * descriptorSetInfos.size());

        descriptorPools.push_back(VK_NULL_HANDLE);
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPools.back()) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void UpdateTextureDescriptor(DescriptorSet &descriptorSet, int index, VkImageView textureView) {
        if (descriptorSet.samplerId == -1 || descriptorSet.textureBinding == -1)
            throw std::runtime_error("can't update texture descriptor, sampler or texture binding is -1");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureView;
        imageInfo.sampler = samplerAccess[descriptorSet.samplerId];

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptorSet.descriptorSet[i];
            write.dstBinding = descriptorSet.textureBinding;
            write.dstArrayElement = index;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
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

    void CalculateBestOrdering(SurfaceVulkanData *surface) {
        /*
        surface->pipelineBindOrdering.clear();

        std::unordered_map<uint32_t, DescriptorSetId> bound;
        // remaining pipelines
        std::vector<uint32_t> remaining(surface->pipelineDatas.size());
        std::iota(remaining.begin(), remaining.end(),0);

        while (!remaining.empty()) {
            auto bestIt = remaining.begin();
            uint32_t minBinds = UINT32_MAX;
            std::vector<DescriptorSetId> bestToBind;

            // just find where bind count is min
            for (auto it = remaining.begin(); it != remaining.end(); ++it) {
                const auto &data = surface->pipelineDatas[*it];
                std::vector<DescriptorSetId> thisToBind;
                for (auto &id : data.descriptorSetIds) {
                    auto b = bound.find(id.set);
                    if (b == bound.end() || b->second.idx != id.idx) thisToBind.push_back(id);
                }
                if (thisToBind.size() < minBinds) {
                    minBinds = thisToBind.size();
                    bestIt = it;
                    bestToBind = thisToBind;
                }
            }

            surface->pipelineBindOrdering.push_back({ *bestIt, bestToBind });

            for (auto &id : bestToBind) bound.insert_or_assign(id.set, id);;

            remaining.erase(bestIt);
        }

        surface->isOrderingValid = true;
        */
    }

    void RecordClearCommand(SurfaceVulkanData *surface, uint32_t imageIndex) {
        int i = surface->currentFrame;
        VkCommandBuffer &cmdBuffer = surface->commandBuffers[i];

        // start the recording to a buffer with some specifications (if called on a buffer, it will reset it)
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;                   // Optional
        beginInfo.pInheritanceInfo = nullptr;  // Optional

        if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkClearColorValue clearColor{};
        clearColor.float32[0] = 0.2f;
        clearColor.float32[1] = 0.3f;
        clearColor.float32[2] = 0.4f;
        clearColor.float32[3] = 0.0f;

        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount = 1;
        range.layerCount = 1;

        vkCmdClearColorImage(
            cmdBuffer,
            surface->swapChainImages[imageIndex],
            VK_IMAGE_LAYOUT_GENERAL,
            &clearColor,
            1,
            &range);

        // ending the command recording
        if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    // records command to commandbuffer, also need the image's index that you want to write to
    void RecordCommandBuffer(SurfaceVulkanData *surface, VkExtent2D &extent, uint32_t &imageIndex, VkSurfaceKHR surfaceKey) {
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

        std::array<VkClearValue, 2> clearValues{};  // he???
        clearValues[0].color = { { 0.388235f, 0.643137f, 0.839216f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
        colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachmentInfo.imageView = surface->swapChainImageViews[imageIndex];
        colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentInfo.clearValue = clearValues[0];

        VkRect2D renderArea{};
        renderArea.offset = { 0, 0 };
        renderArea.extent = extent;

        VkRenderingInfoKHR renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderInfo.renderArea = renderArea;
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachmentInfo;

        bool needDepth = false;
        for (auto &id : surface->pipelines) {
            needDepth |= pipelines[id].needDepth;
        }
        if (needDepth) {
            CreateDepthResources(surface, extent);

            VkRenderingAttachmentInfoKHR depthAttachmentInfo{};
            depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachmentInfo.imageView = surface->depthImageView;
            depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachmentInfo.clearValue = clearValues[1];

            renderInfo.pDepthAttachment = &depthAttachmentInfo;
        }

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

        // if (!surface->isOrderingValid) CalculateBestOrdering(surface);
        /*maytodo
        // NOTE: this is only for graphics pipelines
        for (const auto &[pipelineIdx, descriptorSetsToBind] : surface->pipelineBindOrdering) {
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, surface->pipelines[pipelineIdx]);
            for (const auto &[set,idx] : descriptorSetsToBind) {
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, surface->layouts[pipelineIdx], set, 1, &descriptorSets[set][idx].descriptorSet.at(frame), 0, nullptr);
            }
            if (surface->pipelineDatas[pipelineIdx].constantsSize > 0)
                // TODO: layout should refer to the correct pipeline
                vkCmdPushConstants(cmdBuffer, surface->layouts[pipelineIdx], VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, surface->pipelineDatas[pipelineIdx].constantsSize, constantsData[surfaceKey]);

            // draw call
        }*/

        // only one pipeline can be used at once
        for (size_t i = 0; i < surface->pipelines.size(); i++) {
            int ppIndex = surface->pipelines[i];
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[ppIndex].pipeline);
            for (const auto &id : pipelines[ppIndex].descriptorIds) {
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[ppIndex].layout, descriptorSets[id].setIdx, 1, &descriptorSets[id].descriptorSet.at(frame), 0, nullptr);
            }
            if (pipelines[ppIndex].constants.size() > 0) {
                uint32_t offset = 0;
                for (size_t c = 0; c < pipelines[ppIndex].constants.size(); c++) {
                    std::string constName = pipelines[ppIndex].constants[c];
                    vkCmdPushConstants(cmdBuffer, pipelines[ppIndex].layout, VK_SHADER_STAGE_ALL, offset, constantsData[constName].size, constantsData[constName].data);

                    offset += constantsData[constName].size;
                }
            }
            vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(surface->indiceCount), 1, 0, 0, 0);
        }

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

    void getNeededDescriptorSetLayouts(std::vector<VkDescriptorSetLayout> &outLayouts, std::vector<uint32_t> &descriptorSetIds) {
        for (auto &id : descriptorSetIds) {
            if (!descriptorSets.contains(id)) throw new std::runtime_error("there are no descriptorSetLayouts with set number");
            outLayouts.push_back(descriptorSets.at(id).descriptorSetLayout);
        };
        if (outLayouts.size() != descriptorSetIds.size()) throw new std::runtime_error("somehow not all layout present in outLayouts");  // it will indeed be somehow
    }

    std::vector<int> CreateDescriptorSets(std::vector<DescriptorSetInfo> &descriptorSetInfos) {
        std::vector<int> descriptorIds;

        for (auto &info : descriptorSetInfos) {
            descriptorIds.push_back(CreateDescriptorSetLayout(info));
        }

        CreateDescriptorPool(descriptorSetInfos);

        std::vector<uint32_t> variableCountsPerSet;

        for (auto &info : descriptorSetInfos) {
            uint32_t countForThisSet = 0;

            for (auto &descriptor : info.descriptorInfo) {
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
        std::vector<VkDescriptorSet> flatSets;

        for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
            for (auto &id : descriptorIds) {
                if (descriptorSets[id].descriptorSetLayout == VK_NULL_HANDLE) {
                    throw std::runtime_error("descriptor set layout can't be null");
                }
                layouts.push_back(descriptorSets[id].descriptorSetLayout);
                flatSets.push_back(VK_NULL_HANDLE);
            }
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPools.back();
        allocInfo.descriptorSetCount = layouts.size();
        allocInfo.pSetLayouts = layouts.data();
        allocInfo.pNext = &variableCount;

        if (vkAllocateDescriptorSets(device, &allocInfo, flatSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        size_t index = 0;
        for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
            for (auto &id : descriptorIds) {
                descriptorSets[id].descriptorSet[f] = flatSets[index++];
            }
        }

        // TODO: this should happen separately
        for (size_t i = 0; i < descriptorSetInfos.size(); i++) {
            auto &setInfo = descriptorSetInfos[i];
            auto &descriptorSet = descriptorSets[descriptorIds[i]];
            descriptorSet.setIdx = setInfo.setIndex;
            std::vector<VkWriteDescriptorSet> writes;
            std::vector<VkDescriptorImageInfo> imageInfos;
            VkDescriptorBufferInfo uniformBufferInfo{};
            VkDescriptorBufferInfo storageBufferInfo{};

            for (size_t j = 0; j < setInfo.descriptorInfo.size(); j++) {
                auto &descriptor = setInfo.descriptorInfo[j];

                if (descriptor.type == Render::DescriptorInfo::DescriptorType::UNIFORM)
                    CreateUniformBuffer(descriptorSet.uniformBuffer, descriptor.data);
                else if (descriptor.type == Render::DescriptorInfo::DescriptorType::STORAGE)
                    CreateStorageBuffers(descriptorSet.storageBuffer, descriptor.data);

                for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
                    VkWriteDescriptorSet write{};
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = descriptorSet.descriptorSet[f];
                    write.dstBinding = descriptor.binding;
                    write.dstArrayElement = 0;

                    if (descriptor.type == Render::DescriptorInfo::DescriptorType::UNIFORM) {
                        uniformBufferInfo.buffer = descriptorSet.uniformBuffer[f]->buffer;
                        uniformBufferInfo.offset = 0;
                        uniformBufferInfo.range = descriptor.data;

                        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                        write.descriptorCount = 1;
                        write.pBufferInfo = &uniformBufferInfo;

                        writes.push_back(write);
                    } else if (descriptor.type == Render::DescriptorInfo::DescriptorType::STORAGE) {
                        storageBufferInfo.buffer = descriptorSet.storageBuffer[f]->buffer;
                        storageBufferInfo.offset = 0;
                        storageBufferInfo.range = descriptor.data;

                        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                        write.descriptorCount = 1;
                        write.pBufferInfo = &storageBufferInfo;

                        writes.push_back(write);
                    } else {
                        imageInfos.resize(descriptor.count);
                        for (auto &info : imageInfos) {
                            info.sampler = samplerAccess[descriptor.data];
                            info.imageView = VK_NULL_HANDLE;
                            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        }

                        descriptorSet.textureBinding = descriptor.binding;
                        descriptorSet.samplerId = descriptor.data;

                        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        write.descriptorCount = descriptor.count;
                        write.pImageInfo = imageInfos.data();

                        writes.push_back(write);
                    }
                }
            }

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }

        return descriptorIds;
    }

    // Create every pipeline for a surface
    int CreateGraphicPipelines(CreateGraphicPipeLineInfoVKConvert pipelineData) {
        PipeLine pipeline{};
        pipeline.constants = std::move(pipelineData.constants);
        pipeline.descriptorIds = pipelineData.descriptorSetIds;

        std::unordered_set<int> seen;
        for (auto &id : pipeline.descriptorIds) {
            if (!seen.insert(descriptorSets[id].setIdx).second) {
                throw std::runtime_error("two or more descriptors uses the same set index");
            }
        }

        if (pipelineData.depthTestEnable || pipelineData.depthWriteEnable)
            pipeline.needDepth = true;

        // reads in the binary shader data
        CompileShader(pipelineData.vertexShader, "vert");
        auto vertShaderCode = readFile("vert.spv");
        CompileShader(pipelineData.fragmentShader, "frag");
        auto fragShaderCode = readFile("frag.spv");

        std::filesystem::remove("vert.spv");
        std::filesystem::remove("frag.spv");

        // shader data
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = createShaderModule(vertShaderCode);
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = createShaderModule(fragShaderCode);
        fragShaderStageInfo.pName = "main";

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

        // vertex data specifications
        auto bindingDescription = GetVertexBindingDescription(pipelineData.vertexDataLayout);
        auto attributeDescriptions = GetVertexAttributeDescriptions(pipelineData.vertexDataLayout);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        //!!! what type of data it uses (lines, triangles), and how (reusing vertecies or not)
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = pipelineData.topology;  // feature
        inputAssembly.primitiveRestartEnable = VK_FALSE;

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

        // rasterizer data
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;

        // set how fragments are generated based on geometry (point, line, fill)
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;

        // culling
        rasterizer.cullMode = pipelineData.cullMode;    // feature
        rasterizer.frontFace = pipelineData.frontFace;  // feature

        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;  // Optional
        rasterizer.depthBiasClamp = 0.0f;           // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f;     // Optional

        // multisampling - a.k.a. easy anti-alliasing
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = pipelineData.rasterizationSamples;  // feature
        multisampling.minSampleShading = 1.0f;                                   // Optional
        multisampling.pSampleMask = nullptr;                                     // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE;                          // Optional
        multisampling.alphaToOneEnable = VK_FALSE;                               // Optional

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = pipelineData.blendEnable;                  // feature
        colorBlendAttachment.srcColorBlendFactor = pipelineData.srcColorBlendFactor;  // feature
        colorBlendAttachment.dstColorBlendFactor = pipelineData.dstColorBlendFactor;  // feature
        colorBlendAttachment.colorBlendOp = pipelineData.colorBlendOp;                // feature
        colorBlendAttachment.srcAlphaBlendFactor = pipelineData.srcAlphaBlendFactor;  // feature
        colorBlendAttachment.dstAlphaBlendFactor = pipelineData.dstAlphaBlendFactor;  // feature
        colorBlendAttachment.alphaBlendOp = pipelineData.alphaBlendOp;                // feature

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = pipelineData.depthTestEnable;    // feature
        depthStencil.depthWriteEnable = pipelineData.depthWriteEnable;  // feature
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;  // Optional
        depthStencil.maxDepthBounds = 1.0f;  // Optional

        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};  // Optional
        depthStencil.back = {};   // Optional

        VkFormat depthFormat = findDepthFormat();  // e.g., VK_FORMAT_D32_SFLOAT

        VkPipelineRenderingCreateInfo renderCreateInfo{};
        renderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderCreateInfo.colorAttachmentCount = 1;
        renderCreateInfo.pColorAttachmentFormats = &swapChainImageFormat.format;
        renderCreateInfo.depthAttachmentFormat = depthFormat;

        // TODO: maybe we should split the const into separate ones :P
        // i thought only one can be at the same time
        // with best regards, józsef

        VkPushConstantRange constRange{};
        constRange.size = 0;
        constRange.offset = 0;
        for (size_t i = 0; i < pipeline.constants.size(); i++) {
            constRange.size += constantsData[pipeline.constants[i]].size;
            constRange.stageFlags = VK_SHADER_STAGE_ALL;
        }

        std::vector<VkDescriptorSetLayout> neededLayouts;
        // getNeededDescriptorSetLayouts(neededLayouts, pipelineData.descriptorSetIds);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = neededLayouts.size();
        pipelineLayoutInfo.pSetLayouts = neededLayouts.data();
        if (pipeline.constants.size() > 0) {
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &constRange;
        }

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipeline.layout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = shaderStages.size();
        pipelineInfo.pStages = shaderStages.data();

        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;

        pipelineInfo.layout = pipeline.layout;

        pipelineInfo.renderPass = nullptr;
        pipelineInfo.pNext = &renderCreateInfo;

        // can derive render passes from one-another so it can have a parent-child hierarchy, faster, easier
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, vertShaderStageInfo.module, nullptr);
        vkDestroyShaderModule(device, fragShaderStageInfo.module, nullptr);

        pipelines[nextPipeline] = pipeline;
        return nextPipeline++;
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

    // vertex data layout
    uint32_t GetVertexDataSize(Render::VertexDataType type) {
        switch (type) {
            case FLOAT:
                return sizeof(float);
                break;
            case UINT:
                return sizeof(unsigned int);
                break;
            case VEC2:
                return sizeof(float) * 2;
                break;
            case VEC3:
                return sizeof(float) * 3;
                break;
            case VEC4:
                return sizeof(float) * 4;
                break;
            default:
                throw std::runtime_error("Invalid vertex type");
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
            default:
                throw std::runtime_error("Invalid vertex type");
                break;
        }
    }

    VkVertexInputBindingDescription GetVertexBindingDescription(std::vector<VertexDataType> dataLayout) {
        VkVertexInputBindingDescription bindingDescription{};

        bindingDescription.binding = 0;

        uint32_t stride = 0;
        for (auto &item : dataLayout) {
            stride += GetVertexDataSize(item);
        }

        bindingDescription.stride = stride;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    std::vector<VkVertexInputAttributeDescription> GetVertexAttributeDescriptions(std::vector<VertexDataType> dataLayout) {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

        uint32_t stride = 0;
        int location = 0;
        for (auto &item : dataLayout) {
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
};

void Render::Draw(Surface surface) { instance->Draw(surface.surface); }

void Render::Update() { instance->Update(); }

bool Render::IsValidSurface(Surface surface) { return instance->IsValidSurface(surface.surface); }

void Render::AddElementData(RenderData &data) { instance->AddElementData(data); }

// Render::Texture Render::CreateTexture(std::string path) { return instance->CreateTexture(path); }

void Render::Clean() { delete instance; }

int Render::CreateDescriptorSet(Render::DescriptorSetInfo &descriptorSetInfo) {
    std::vector<Render::DescriptorSetInfo> tmp = { descriptorSetInfo };
    return instance->CreateDescSets(tmp)[0];
}

std::vector<int> Render::CreateDescriptorSet(std::vector<Render::DescriptorSetInfo> &descriptorSetInfo) {
    return instance->CreateDescSets(descriptorSetInfo);
}

int Render::CreatePipeline(CreateGraphicPipeLineInfo &gpInfo) { return instance->CreatePipeline(gpInfo); }

int Render::CreateFontPage(const std::vector<uint8_t> &rgbaData, uint32_t width, uint32_t height) { return instance->CreateFontPage(rgbaData, width, height); };

void *Render::GetWindowOfSurface(int surface) {
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

void Render::PublishConstantsToVulkan(std::vector<Render::ConstData> &data) {
    instance->PublishConstants(data);
}

void Render::PushConstantsToVulkan(std::string &name, void *data, uint32_t size) {
    instance->PushConstants(name, data, size);
}

void Render::PushOn(VertexData &data, Surface &surface) {
    if (!IsValidSurface(surface))
        throw new std::runtime_error("can't push onto invalid surface");

    int id = surface.surface;

    if (!surfaceData.contains(id)) {
        surfaceData[id] = data;
    } else {
        VertexData &current = surfaceData[id];
        uint32_t indicieShift = current.vertecies.size();
        current.vertecies.insert(current.vertecies.end(), data.vertecies.begin(), data.vertecies.end());
        for (size_t i = 0; i < data.indicies.size(); i++) {
            current.indicies.push_back(indicieShift + data.indicies[i]);
        }
    }
}

void Render::Submit(Surface &surface) {
    if (!surfaceData.contains(surface.surface))
        throw new std::runtime_error("Cant submit the given surface");

    int id = surface.surface;
    RenderData output{
        .vertecies = surfaceData[id].vertecies,
        .indicies = surfaceData[id].indicies,
        .surface = id,
        .changed = true
    };

    instance->AddElementData(output);
}

void Render::Clear(Window &window) { instance->Clear(window); };

Render::Vulkan *Render::instance = nullptr;
std::unordered_map<int, Render::VertexData> Render::surfaceData;
}  // namespace Ignis
