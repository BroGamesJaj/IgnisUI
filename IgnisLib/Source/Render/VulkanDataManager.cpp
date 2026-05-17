#include "../IgnisLib.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define MAX_FRAMES_IN_FLIGHT 2

#include <array>
#include <filesystem>

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

    std::vector<uint32_t> constants;
    std::vector<uint32_t> descriptorSetIds;

    std::vector<Render::VertexDataType> vertexDataLayout;
};

struct PipeLine {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    std::vector<uint32_t> constants;
    std::vector<uint32_t> descriptorIds;
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

    std::unordered_map<uint32_t, VulkanConstData> constantsData;
    std::unordered_map<uint32_t, PipeLine> pipelines;
    uint32_t nextPipeline = 0;
    std::unordered_map<uint32_t, DescriptorSet> descriptorSets;
    uint32_t nextDescriptorSetId = 0;

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

    VkFormat FindSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(*phyDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }

            throw std::runtime_error("failed to find supported format!");
        }

        return candidates[0];
    }
    VkFormat FindDepthFormat() { return FindSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT); }

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

    void GetNeededDescriptorSetLayouts(std::vector<VkDescriptorSetLayout> &outLayouts, std::vector<uint32_t> &descriptorSetIds) {
        for (auto &id : descriptorSetIds) {
            if (!descriptorSets.contains(id)) throw new std::runtime_error("there are no descriptorSetLayouts with set number");
            outLayouts.push_back(descriptorSets.at(id).descriptorSetLayout);
        };
        if (outLayouts.size() != descriptorSetIds.size()) throw new std::runtime_error("somehow not all layout present in outLayouts");  // it will indeed be somehow
    }

    // maytodo: do pipelines so other types can be created too, compute, raytracing
    int CreateGraphicPipelines(CreateGraphicPipeLineInfoVKConvert pipelineData) {
        PipeLine pipeline{};
        pipeline.constants = std::move(pipelineData.constants);
        pipeline.descriptorIds = pipelineData.descriptorSetIds;

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

        VkFormat depthFormat = FindDepthFormat();  // e.g., VK_FORMAT_D32_SFLOAT

        VkPipelineRenderingCreateInfo renderCreateInfo{};
        renderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderCreateInfo.colorAttachmentCount = 1;
        renderCreateInfo.pColorAttachmentFormats = &swapChainImageFormat.format;
        renderCreateInfo.depthAttachmentFormat = depthFormat;

        VkPushConstantRange constRange{};
        constRange.size = 0;
        constRange.offset = 0;
        for (size_t i = 0; i < pipeline.constants.size(); i++) {
            constRange.size += constantsData[pipeline.constants[i]].size;
            constRange.stageFlags = VK_SHADER_STAGE_ALL;
        }

        std::vector<VkDescriptorSetLayout> neededLayouts;
        GetNeededDescriptorSetLayouts(neededLayouts, pipelineData.descriptorSetIds);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = neededLayouts.size();
        pipelineLayoutInfo.pSetLayouts = neededLayouts.data();
        if (pipeline.constants.size() > 0) {
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &constRange;
        }

        if (vkCreatePipelineLayout(*device, &pipelineLayoutInfo, nullptr, &pipeline.layout) != VK_SUCCESS) {
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

        if (vkCreateGraphicsPipelines(*device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(*device, vertShaderStageInfo.module, nullptr);
        vkDestroyShaderModule(*device, fragShaderStageInfo.module, nullptr);

        pipelines[nextPipeline] = pipeline;
        return nextPipeline++;
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
