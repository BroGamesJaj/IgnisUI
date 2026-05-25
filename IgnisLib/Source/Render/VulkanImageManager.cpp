#include "../IgnisLib.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

namespace Ignis {

struct TextureData {
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
};

struct CreateSamplerVKConvert {
    VkFilter filter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
};

class Render::VulkanImageManager {
    VkDevice *device;
    VkPhysicalDevice *phyDevice;

    std::unordered_map<uint32_t, VkSampler> samplerAccess;
    uint32_t nextSampler = 1;

    std::unordered_map<uint32_t, TextureData> textureData;
    uint32_t nextTexture = 1;
    uint32_t mainSampler;

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

    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer *cmdPtr = (VkCommandBuffer *)BeginSingleTimeCommands();
        VkCommandBuffer commandBuffer = *cmdPtr;
        delete cmdPtr;

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
        // basic vertex buffer data
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

        if (vkCreateImage(*device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(*device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(*device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(*device, image, imageMemory, 0);
    }

    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer *cmdPtr = (VkCommandBuffer *)BeginSingleTimeCommands();
        VkCommandBuffer commandBuffer = *cmdPtr;
        delete cmdPtr;

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

    void CreateTextureImage(std::string path, VkImage &image, VkDeviceMemory &memory) {
        int texWidth, texHeight, texChannels;
        stbi_uc *pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) throw std::runtime_error("failed to load texture image!");

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void *data;
        vkMapMemory(*device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(*device, stagingBufferMemory);

        stbi_image_free(pixels);

        CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

        TransitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

        TransitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(*device, stagingBuffer, nullptr);
        vkFreeMemory(*device, stagingBufferMemory, nullptr);
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

    VkImageView CreateTextureImageView(VkImage textureImage) { return CreateImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT); }

    void UpdateTextureDescriptor(int textureIndex, VkImageView textureView) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureView;
        imageInfo.sampler = samplerAccess[mainSampler];

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = *(VkDescriptorSet *)GetBindlessSet();
        write.dstBinding = 0;
        write.dstArrayElement = textureIndex;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(*device, 1, &write, 0, nullptr);
    }

   public:
    VulkanImageManager() {
        CreateSamplerVKConvert data;
        mainSampler = CreateSamplerFromData(data);
    }

    Render::Texture CreateTexture(std::string path) {
        TextureData data;
        CreateTextureImage(path, data.textureImage, data.textureImageMemory);
        data.textureImageView = CreateTextureImageView(data.textureImage);

        textureData[nextTexture] = data;

        UpdateTextureDescriptor(nextTexture, data.textureImageView);

        Render::Texture texture{};
        texture.id = nextTexture++;
        return texture;
    }

    uint32_t CreateSamplerFromData(CreateSamplerVKConvert &samplerVK) {
        VkSamplerCreateInfo samplerInfo{};

        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        samplerInfo.magFilter = samplerVK.filter;
        samplerInfo.minFilter = samplerVK.filter;

        samplerInfo.addressModeU = samplerVK.addressMode;
        samplerInfo.addressModeV = samplerVK.addressMode;
        samplerInfo.addressModeW = samplerVK.addressMode;

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(*phyDevice, &properties);

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

        if (vkCreateSampler(*device, &samplerInfo, nullptr, &samplerAccess[nextSampler]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }

        return nextSampler++;
    }
};

void Render::InitVulkanImageManager() {
    vulkanImageManager = new VulkanImageManager();
}

}  // namespace Ignis