#include "../IgnisLib.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define MAX_FRAMES_IN_FLIGHT 2
#define TEXTURE_COUNT 1024

#include <array>
#include <filesystem>
#include <unordered_set>

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
    bool framebufferResized = false;

    std::vector<VkCommandBuffer> commandBuffers;

    VkExtent2D swapChainExtent;
    VkSurfaceCapabilitiesKHR capabilities;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indiceCount;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    VkBuffer drawBuffer;
    VkDeviceMemory drawBufferMemory;

    std::vector<int> pipelines;

    std::vector<VkDrawIndexedIndirectCommand> drawCommands;
};

struct CreateGraphicPipeLineInfo {
    std::string vertexShader;
    std::string fragmentShader;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    float lineWidth = 1.0f;

    VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkBool32 sampleShadingEnable = VK_FALSE;
    float minSampleShadingb = 1.0f;

    VkBool32 blendEnable = VK_TRUE;
    VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
    VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;

    VkBool32 depthTestEnable = VK_FALSE;
    VkBool32 depthWriteEnable = VK_FALSE;

    std::vector<Render::VertexDataType> vertexDataLayout = { Render::VertexDataType::VEC3, Render::VertexDataType::FLOAT };
};

enum class PipelineType { Graphics,
                          Compute };

struct PipeLine {
    VkPipeline pipeline;
    // VkPipelineLayout layout;
    PipelineType type = PipelineType::Graphics;
    bool needDepth;
};

struct VulkanConstData {
    void *data;
    uint32_t size;
};

struct BufferData {
    VkBuffer buffer;
    VkDeviceMemory bufferMemory;
    void *bufferMapped;
};

struct DescriptorSet {
    // TODO: maybe reuse layouts between sets
    VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSet{ VK_NULL_HANDLE };

    std::array<std::unique_ptr<BufferData>, MAX_FRAMES_IN_FLIGHT> uniformBuffer{ nullptr };
    std::array<std::unique_ptr<BufferData>, MAX_FRAMES_IN_FLIGHT> storageBuffer{ nullptr };

    int textureBinding = -1;
    int samplerId = -1;

    uint32_t setIdx;
};

struct TextureData {
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
};

////////////////////////
//////    Data    //////
////////////////////////

class Render::VulkanDataManager {
   private:
    size_t currentFrame = 0;

    VkPipelineLayout graphicPipelineLayout;

    SwapChainSupportDetails swapChainSupport;
    VkSurfaceFormatKHR swapChainImageFormat;
    VkPresentModeKHR swapChainPresentMode;

    std::unordered_map<GLFWwindow *, WindowData> windows;

    VkInstance *instance;
    VkDevice *device;
    VkPhysicalDevice *phyDevice;
    VkSurfaceKHR *windowManagerSurface;

    std::unordered_map<uint32_t, VulkanConstData> constantsData;
    std::unordered_map<uint32_t, PipeLine> pipelines;
    uint32_t nextPipeline = 0;
    int mainGraphicPipeline;

    // std::unordered_map<uint32_t, DescriptorSet> descriptorSets;
    // uint32_t nextDescriptorSetId = 0;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout bindlessLayout;

   public:
    VkDescriptorSet globalData;

   private:
    VkDescriptorSetLayout perFrameLayout;
    VkDescriptorSet perFrameData;

    std::unordered_map<GLFWwindow *, RenderData> basicRenderData;
    std::unordered_map<GLFWwindow *, RenderData> uiRenderData;

    std::vector<Window> basicDrawQueue;
    std::vector<Window> uiDrawQueue;

    ////////////////////////
    ////   Functions   /////
    ////////////////////////

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
        allocInfo.commandPool = *(VkCommandPool *)GetGraphicPool();
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

    VkShaderModule createShaderModule(const std::vector<char> &code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(*device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

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

    /*
    void GetNeededDescriptorSetLayouts(std::vector<VkDescriptorSetLayout> &outLayouts, std::vector<uint32_t> &descriptorSetIds) {
        for (auto &id : descriptorSetIds) {
            if (!descriptorSets.contains(id)) throw new std::runtime_error("there are no descriptorSetLayouts with set number");
            outLayouts.push_back(descriptorSets.at(id).descriptorSetLayout);
        };
        if (outLayouts.size() != descriptorSetIds.size()) throw new std::runtime_error("somehow not all layout present in outLayouts");  // it will indeed be somehow
    }*/

    void CreateDescriptorPool() {
        std::array<VkDescriptorPoolSize, 3> poolSizes{ {
            // UBOs — FrameData + PassData
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = MAX_FRAMES_IN_FLIGHT  // set1.binding0
            },
            // SSBOs — transforms + draw commands
            {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = MAX_FRAMES_IN_FLIGHT  // set1.binding1
            },
            // Bindless textures — just one global set
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = TEXTURE_COUNT },
        } };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = 3 * MAX_FRAMES_IN_FLIGHT;  // not exact but we have more wiggleroom later
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        if (vkCreateDescriptorPool(*device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void SetupGraphicDescriptors() {
        CreateDescriptorPool();

        // set 1
        VkDescriptorSetLayoutBinding textureBinding{};
        textureBinding.binding = 0;
        textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureBinding.descriptorCount = 1024;
        textureBinding.stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorBindingFlags flags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
        flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flagsInfo.bindingCount = 1;
        flagsInfo.pBindingFlags = &flags;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &flagsInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &textureBinding;

        vkCreateDescriptorSetLayout(*device, &layoutInfo, nullptr, &bindlessLayout);

        // set 2
        VkDescriptorSetLayoutBinding UboBinding{};
        UboBinding.binding = 0;
        UboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        UboBinding.descriptorCount = 1;
        UboBinding.stageFlags = VK_SHADER_STAGE_ALL;

        // ssbo needed
        VkDescriptorSetLayoutBinding SsboBinding{};
        SsboBinding.binding = 1;
        SsboBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        SsboBinding.descriptorCount = 1;
        SsboBinding.stageFlags = VK_SHADER_STAGE_ALL;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { UboBinding, SsboBinding };

        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings.data();
        layoutInfo.pNext = nullptr;

        vkCreateDescriptorSetLayout(*device, &layoutInfo, nullptr, &perFrameLayout);

        // creation
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &bindlessLayout;
        vkAllocateDescriptorSets(*device, &allocInfo, &globalData);

        allocInfo.pSetLayouts = &perFrameLayout;
        vkAllocateDescriptorSets(*device, &allocInfo, &perFrameData);
    }

    // for the library owned stuff, it wont change, for the user thingies, it will be auto generated
    void SetupPipelineLayout() {
        std::vector<VkDescriptorSetLayout> layout = { bindlessLayout, perFrameLayout };
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = layout.size();
        pipelineLayoutInfo.pSetLayouts = layout.data();

        /*const isnt used just yet (there is nothing in it)
        if (pipeline.constants.size() > 0) {
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &constRange;
        }*/

        if (vkCreatePipelineLayout(*device, &pipelineLayoutInfo, nullptr, &graphicPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    void SetupMainGraphicPipeline() {
        CreateGraphicPipeLineInfo gpInfo{};
        gpInfo.vertexShader = "../Resources/Shaders/shader.vert";
        gpInfo.fragmentShader = "../Resources/Shaders/shader.frag";
        mainGraphicPipeline = CreateGraphicPipeline(gpInfo);
    }

    struct CreateComputePipeLineInfo {
        std::string computeShader;
    };

    int CreateComputePipeline(CreateComputePipeLineInfo info) {
        PipeLine pipeline{};
        pipeline.type = PipelineType::Compute;

        FileSystem::CompileShader(info.computeShader, "compute");
        auto computeShaderCode = FileSystem::ReadFile("compute.spv");

        std::filesystem::remove("compute.spv");

        VkPipelineShaderStageCreateInfo compShaderStageInfo{};
        compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        compShaderStageInfo.module = createShaderModule(computeShaderCode);
        compShaderStageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = compShaderStageInfo;
        pipelineInfo.layout = graphicPipelineLayout;  // TODO: we need a pipeline for this when we use it

        vkCreateComputePipelines(*device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline);
    }

    // maytodo: do pipelines so other types can be created too, compute, raytracing
    int CreateGraphicPipeline(CreateGraphicPipeLineInfo pipelineData) {
        PipeLine pipeline{};

        if (pipelineData.depthTestEnable || pipelineData.depthWriteEnable)
            pipeline.needDepth = true;

        // reads in the binary shader data
        FileSystem::CompileShader(pipelineData.vertexShader, "vert");
        auto vertShaderCode = FileSystem::ReadFile("vert.spv");
        FileSystem::CompileShader(pipelineData.fragmentShader, "frag");
        auto fragShaderCode = FileSystem::ReadFile("frag.spv");

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

        VkFormat *ptr = (VkFormat *)FindDepthFormat();
        VkFormat depthFormat = *ptr;  // e.g., VK_FORMAT_D32_SFLOAT
        delete ptr;

        VkPipelineRenderingCreateInfo renderCreateInfo{};
        renderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderCreateInfo.colorAttachmentCount = 1;
        renderCreateInfo.pColorAttachmentFormats = &swapChainImageFormat.format;
        renderCreateInfo.depthAttachmentFormat = depthFormat;

        /*
        VkPushConstantRange constRange{};
        constRange.size = 0;
        constRange.offset = 0;
        for (size_t i = 0; i < pipeline.constants.size(); i++) {
            constRange.size += constantsData[pipeline.constants[i]].size;
            constRange.stageFlags = VK_SHADER_STAGE_ALL;
        }*/

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

        pipelineInfo.layout = graphicPipelineLayout;

        pipelineInfo.renderPass = nullptr;
        pipelineInfo.pNext = &renderCreateInfo;

        // can derive render passes from one-another so it can have a parent-child hierarchy, faster, easier
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(*device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(*device, vertShaderStageInfo.module, nullptr);
        vkDestroyShaderModule(*device, fragShaderStageInfo.module, nullptr);

        pipelines[nextPipeline] = pipeline;
        return nextPipeline++;
    }

    void CleanupSwapChain(GLFWwindow *window) {
        vkDestroyImageView(*device, windows[window].depthImageView, nullptr);
        vkDestroyImage(*device, windows[window].depthImage, nullptr);
        vkFreeMemory(*device, windows[window].depthImageMemory, nullptr);

        for (auto imageView : windows[window].swapChainImageViews) {
            vkDestroyImageView(*device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(*device, windows[window].swapChain, nullptr);
    }

    void RecreateSwapChain(GLFWwindow *window) {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(*device);

        CleanupSwapChain(window);

        CreateSwapChain(window);
        CreateImageViews(window);

        TextureData image;
        image.textureImage = windows[window].depthImage;
        image.textureImageMemory = windows[window].depthImageMemory;
        image.textureImageView = windows[window].depthImageView;

        CreateDepthResources((void *)&image, (void *)&windows[window].swapChainExtent);
    }

    // finding the best memory type based on the data and usage in our application
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(*phyDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory) {
        // basic buffer data
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(*device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        // getting the memory specs that the buffer need
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(*device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

        // allocating the memory for the buffer
        if (vkAllocateMemory(*device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        //"If the offset is non-zero, then it is required to be divisible by memRequirements.alignment."
        vkBindBufferMemory(*device, buffer, bufferMemory, 0);
    }

    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = (VkCommandBuffer)BeginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        EndSingleTimeCommands(commandBuffer);
    }

    uint32_t CreateVertexBuffer(Window windowIn) {
        WindowData &window = windows[windowIn.ptr];

        if (window.vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(*device, window.vertexBuffer, nullptr);
            vkFreeMemory(*device, window.vertexBufferMemory, nullptr);
            window.vertexBuffer = VK_NULL_HANDLE;
        }

        // this does not work, it will crop off vertex data and only save instance data
        // should use variant or idk
        uint32_t baseVertexCount = basicRenderData[windowIn.ptr].vertecies.size();

        uint32_t baseVertexSize = 0;
        if (baseVertexCount > 0)
            baseVertexSize = basicRenderData[windowIn.ptr].vertecies[0].type == VertexType::Base ? sizeof(Vertex) : sizeof(InstanceVertex);

        uint32_t uiVertexSize = 0;
        if (uiRenderData[windowIn.ptr].vertecies.size() > 0)
            uiVertexSize = sizeof(InstanceVertex);

        // ui always uses instancing and only 4 vertecies
        VkDeviceSize bufferSize = baseVertexSize * baseVertexCount + 4 * uiVertexSize;

        if (bufferSize <= 0) return 0;

        // stage buffer so we can move the data to the GPU
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        // mapping the memory so it can be written by the cpu
        // and then copies the data, and unmaps it
        void *data;
        vkMapMemory(*device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, basicRenderData[windowIn.ptr].vertecies.data(), (size_t)baseVertexSize * baseVertexCount);
        memcpy((char *)data + baseVertexSize * baseVertexCount, uiRenderData[windowIn.ptr].vertecies.data(), (size_t)(4 * uiVertexSize));
        vkUnmapMemory(*device, stagingBufferMemory);

        // creates the vertex buffer on the GPU where the CPU cant interact with it but its more efficient, and then transfers the data to it
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, window.vertexBuffer, window.vertexBufferMemory);

        CopyBuffer(stagingBuffer, window.vertexBuffer, bufferSize);

        // cleans up stuff we dont need anymore
        vkDestroyBuffer(*device, stagingBuffer, nullptr);
        vkFreeMemory(*device, stagingBufferMemory, nullptr);

        return baseVertexCount;
    }

    uint32_t CreateIndexBuffer(Window windowIn) {
        WindowData &window = windows[windowIn.ptr];
        if (window.indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(*device, window.indexBuffer, nullptr);
            vkFreeMemory(*device, window.indexBufferMemory, nullptr);
            window.indexBuffer = VK_NULL_HANDLE;
        }

        uint32_t baseIndicieCount = basicRenderData[windowIn.ptr].indicies.size();
        uint32_t baseIndicieSize = sizeof(uint32_t) * baseIndicieCount;
        // ui always has only 4 indicie
        VkDeviceSize bufferSize = sizeof(uint32_t) * 4 + baseIndicieSize;

        window.indiceCount = baseIndicieCount + 4;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        std::vector<uint32_t> &indis = basicRenderData[windowIn.ptr].indicies;

        for (size_t i = 0; i < indis.size(); i++) {
            indis[i] = indis[i] + baseIndicieCount;
        }

        void *data;
        vkMapMemory(*device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, basicRenderData[windowIn.ptr].indicies.data(), (size_t)(baseIndicieCount * sizeof(uint32_t)));
        memcpy((char *)data + baseIndicieSize, uiRenderData[windowIn.ptr].indicies.data(), (size_t)(sizeof(uint32_t) * 4));
        vkUnmapMemory(*device, stagingBufferMemory);

        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, window.indexBuffer, window.indexBufferMemory);

        CopyBuffer(stagingBuffer, window.indexBuffer, bufferSize);

        vkDestroyBuffer(*device, stagingBuffer, nullptr);
        vkFreeMemory(*device, stagingBufferMemory, nullptr);

        return baseIndicieCount;
    }

    void CreateIndirectDrawBuffer(Window windowIn) {
        WindowData &window = windows[windowIn.ptr];

        VkDeviceSize bufferSize = window.drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand);
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void *data;
        vkMapMemory(*device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, window.drawCommands.data(), (size_t)(bufferSize));
        vkUnmapMemory(*device, stagingBufferMemory);

        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, window.drawBuffer, window.drawBufferMemory);

        CopyBuffer(stagingBuffer, window.drawBuffer, bufferSize);

        vkDestroyBuffer(*device, stagingBuffer, nullptr);
        vkFreeMemory(*device, stagingBufferMemory, nullptr);
    }

    std::unordered_set<GLFWwindow *> UpdateElementBuffers() {
        std::unordered_set<GLFWwindow *> needReCreation;

        for (auto &window : basicDrawQueue) {
            if (basicRenderData[window.ptr].changed) {
                needReCreation.insert(window.ptr);
                basicRenderData[window.ptr].changed = false;
            }
        }

        for (auto &window : uiDrawQueue) {
            if (uiRenderData[window.ptr].changed) {
                needReCreation.insert(window.ptr);
                uiRenderData[window.ptr].changed = false;
            }
        }

        for (auto &window : needReCreation) {
            uint32_t vertexOffset = CreateVertexBuffer(Window{ window });
            uint32_t indicieOffset = CreateIndexBuffer(Window{ window });

            if (vertexOffset > 0 && indicieOffset > 0) {
                VkDrawIndexedIndirectCommand base{};
                base.indexCount = indicieOffset;
                base.instanceCount = 1;
                base.firstIndex = 0;
                base.vertexOffset = 0;
                base.firstInstance = 0;
                windows[window].drawCommands.push_back(base);
            }

            if (windows[window].indiceCount > 0) {
                VkDrawIndexedIndirectCommand ui{};
                ui.indexCount = 4;
                ui.instanceCount = uiRenderData[window].instances.size();
                ui.firstIndex = indicieOffset;
                ui.vertexOffset = vertexOffset;
                ui.firstInstance = 0;
                windows[window].drawCommands.push_back(ui);

                CreateIndirectDrawBuffer(Window{ window });
            }
        }

        return needReCreation;
    }

    void RecordDraw(WindowData &window, uint32_t imageIndex) {
        VkCommandBuffer cmdBuffer = window.commandBuffers[currentFrame];

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
        colorAttachmentInfo.imageView = window.swapChainImageViews[imageIndex];
        colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentInfo.clearValue = clearValues[0];

        VkRect2D renderArea{};
        renderArea.offset = { 0, 0 };
        renderArea.extent = window.swapChainExtent;

        VkRenderingInfoKHR renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderInfo.renderArea = renderArea;
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachmentInfo;

        bool needDepth = false;
        for (auto &id : window.pipelines) {
            needDepth |= pipelines[id].needDepth;
        }

        if (needDepth) {
            TextureData image;
            image.textureImage = window.depthImage;
            image.textureImageMemory = window.depthImageMemory;
            image.textureImageView = window.depthImageView;

            CreateDepthResources((void *)&image, (void *)&window.swapChainExtent);

            VkRenderingAttachmentInfoKHR depthAttachmentInfo{};
            depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachmentInfo.imageView = window.depthImageView;
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
        imageMemoryBarrier.image = window.swapChainImages[imageIndex];
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

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(window.swapChainExtent.width);
        viewport.height = static_cast<float>(window.swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = window.swapChainExtent;
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[mainGraphicPipeline].pipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelineLayout,
                                0, 1, &globalData, 0, nullptr);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelineLayout,
                                1, 1, &perFrameData, 0, nullptr);

        VkBuffer vertexBuffers[] = { window.vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, window.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexedIndirect(cmdBuffer, window.drawBuffer, 0, window.drawCommands.size(), sizeof(VkDrawIndexedIndirectCommand));

        vkCmdEndRendering(cmdBuffer);

        imageMemoryBarrier = VkImageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        imageMemoryBarrier.image = window.swapChainImages[imageIndex];
        imageMemoryBarrier.subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void DrawFrame(std::vector<GLFWwindow *> &windowsIn) {
        std::vector<VkFence> fences;
        std::vector<VkSemaphore> finishSemaphores;
        std::vector<VkSwapchainKHR> swapChains;
        std::vector<uint32_t> images;

        images.resize(windowsIn.size());

        for (auto &window : windowsIn) {
            if (!IsValidWindow(window)) continue;

            WindowData &windowData = windows[window];

            fences.push_back(windowData.inFlightFences[currentFrame]);
            finishSemaphores.push_back(windowData.renderFinishedSemaphores[currentFrame]);
            swapChains.push_back(windowData.swapChain);
        }

        vkWaitForFences(*device, fences.size(), fences.data(), VK_TRUE, UINT64_MAX);

        for (size_t i = 0; i < windowsIn.size(); i++) {
            WindowData &windowData = windows[windowsIn[i]];

            // aquires the next available image, when it did, it signals the semaphore
            VkResult result = vkAcquireNextImageKHR(*device, windowData.swapChain, UINT64_MAX,
                                                    windowData.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &images[i]);

            // check if swapchain recreation is necessary
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                RecreateSwapChain(windowsIn[i]);
                return;
            } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("failed to acquire swap chain image!");
            }
            vkResetFences(*device, 1, &windowData.inFlightFences[currentFrame]);

            // resets and records the command buffer
            vkResetCommandBuffer(windowData.commandBuffers[currentFrame], 0);
            RecordDraw(windowData, images[i]);

            // submiting it to the graphics family queue
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            // what semaphore to wait for
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &windowData.imageAvailableSemaphores[currentFrame];
            submitInfo.pWaitDstStageMask = waitStages;
            // assigning the command buffer
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &windowData.commandBuffers[currentFrame];
            // what semaphore to signal when the command buffer finished execution
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &windowData.renderFinishedSemaphores[currentFrame];

            SubmitToGraphicQueue(&submitInfo, &windowData.inFlightFences[currentFrame]);

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }
    }

   public:
    VulkanDataManager() {
        instance = (VkInstance *)GetInstance();
        device = (VkDevice *)GetDevice();
        phyDevice = (VkPhysicalDevice *)GetPhyDevice();
        windowManagerSurface = (VkSurfaceKHR *)GetSurface();

        GetSwapChainData();
        SetupGraphicDescriptors();
        SetupPipelineLayout();
        SetupMainGraphicPipeline();
    }

    void CreateSurface(GLFWwindow *window) {
        windows[window] = WindowData{};

        glfwCreateWindowSurface(*instance, window, nullptr, &windows[window].surface);

        if (*windowManagerSurface == VK_NULL_HANDLE)
            *windowManagerSurface = windows[window].surface;

        CreateSwapChain(window);

        CreateImageViews(window);

        CreateCommandBuffers(window);

        CreateSyncObjects(window);
    }

    bool IsValidWindow(GLFWwindow *window) {
        return windows.contains(window);
    }

    bool IsOpen() {
        return !windows.empty();
    }

    void AddUIRenderData(RenderData &data) {
        uiDrawQueue.push_back(data.window);
        uiRenderData[data.window.ptr] = std::move(data);
    }

    void Update() {
        std::unordered_set<GLFWwindow *> needDraw = UpdateElementBuffers();

        if (needDraw.size() > 0) {
            std::vector<GLFWwindow *> v(needDraw.begin(), needDraw.end());
            DrawFrame(v);
        }

        uiDrawQueue.clear();
        basicDrawQueue.clear();
    }

    void CleanUpWindowData(GLFWwindow *windowIn) {
        CleanupSwapChain(windowIn);

        WindowData &window = windows[windowIn];

        vkDeviceWaitIdle(*device);
        vkFreeCommandBuffers(*device, *(VkCommandPool *)GetGraphicPool(),
                             window.commandBuffers.size(), window.commandBuffers.data());

        vkDestroyBuffer(*device, window.vertexBuffer, nullptr);
        vkFreeMemory(*device, window.vertexBufferMemory, nullptr);

        vkDestroyBuffer(*device, window.indexBuffer, nullptr);
        vkFreeMemory(*device, window.indexBufferMemory, nullptr);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(*device, window.imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(*device, window.renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(*device, window.inFlightFences[i], nullptr);
        }

        vkDestroyImageView(*device, window.depthImageView, nullptr);
        vkDestroyImage(*device, window.depthImage, nullptr);
        vkFreeMemory(*device, window.depthImageMemory, nullptr);

        vkDestroyBuffer(*device, window.drawBuffer, nullptr);
        vkFreeMemory(*device, window.drawBufferMemory, nullptr);

        vkDestroySurfaceKHR(*instance, window.surface, nullptr);

        windows.erase(windowIn);
    }

    void CleanUp() {
        for (auto &[index, data] : constantsData) {
            if (data.data != nullptr) free(data.data);
        }

        for (auto &[index, data] : pipelines) {
            vkDestroyPipeline(*device, data.pipeline, nullptr);
        }

        vkDestroyPipelineLayout(*device, graphicPipelineLayout, nullptr);

        // descriptor buffers go here

        vkDestroyDescriptorSetLayout(*device, bindlessLayout, nullptr);
        vkDestroyDescriptorSetLayout(*device, perFrameLayout, nullptr);
        vkDestroyDescriptorPool(*device, descriptorPool, nullptr);

        for (auto &[window, windowData] : windows) {
            CleanUpWindowData(window);
        }

        VulkanImageManagerCleanUp();

        VulkanQueueManagerCleanUp();

        WindowManagerCleanUp();
    }
};

void Render::InitVulkanDataManager() {
    vulkanDataManager = new VulkanDataManager();
}

void Render::CreateSurface(void *window) {
    vulkanDataManager->CreateSurface((GLFWwindow *)window);
}

void *Render::GetBindlessSet() {
    return &vulkanDataManager->globalData;
}

bool Render::IsValidWindow(Window window) {
    return vulkanDataManager->IsValidWindow(window.ptr);
}

void Render::AddUIRenderData(RenderData &data) {
    vulkanDataManager->AddUIRenderData(data);
}

bool Render::IsOpen() {
    return vulkanDataManager->IsOpen();
}

void Render::UpdateVulkanDataManager() {
    vulkanDataManager->Update();
}

void Render::VulkanDataManagerCleanUpWindowData(GLFWwindow *window) {
    vulkanDataManager->CleanUpWindowData(window);
}

void Render::VulkanDataManagerCleanUp() {
    vulkanDataManager->CleanUp();
}

Render::VulkanDataManager *Render::vulkanDataManager = nullptr;

}  // namespace Ignis
