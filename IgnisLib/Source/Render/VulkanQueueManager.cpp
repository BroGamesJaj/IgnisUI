#include "../IgnisLib.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace Ignis {

class Render::VulkanQueueManager {
    VkDevice *device;
    VkPhysicalDevice *phyDevice;

   public:
    VkCommandPool graphicPool;

   private:
    VkCommandPool presentPool;
    VkCommandPool computePool;
    VkCommandPool transferPool;

    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    VkQueue transferQueue;

    void CreateCommandPools() {
        QueueFamilyIndices queueFamilyIndices = GetQueueFamilies();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphics.family;

        if (vkCreateCommandPool(*device, &poolInfo, nullptr, &graphicPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }

        poolInfo.queueFamilyIndex = queueFamilyIndices.present.family;

        if (vkCreateCommandPool(*device, &poolInfo, nullptr, &presentPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }

        poolInfo.queueFamilyIndex = queueFamilyIndices.compute.family;

        if (vkCreateCommandPool(*device, &poolInfo, nullptr, &computePool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }

        poolInfo.queueFamilyIndex = queueFamilyIndices.transfer.family;

        if (vkCreateCommandPool(*device, &poolInfo, nullptr, &transferPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void CreateQueues() {
        QueueFamilyIndices indicies = GetQueueFamilies();

        std::vector<QueueFamilyIndices::QueueInfo *> queueInfos = {
            &indicies.graphics,
            &indicies.present,
            &indicies.compute,
            &indicies.transfer
        };

        std::vector<VkDeviceQueueInfo2> queueGetInfos;
        queueGetInfos.reserve(queueInfos.size());

        for (auto &qI : queueInfos) {
            queueGetInfos.push_back({ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
                                      nullptr,
                                      0,
                                      qI->family,
                                      qI->index });
        }

        vkGetDeviceQueue2(*device, &queueGetInfos[0], &graphicsQueue);
        vkGetDeviceQueue2(*device, &queueGetInfos[1], &presentQueue);
        vkGetDeviceQueue2(*device, &queueGetInfos[2], &computeQueue);
        vkGetDeviceQueue2(*device, &queueGetInfos[3], &transferQueue);
    }

   public:
    VulkanQueueManager() {
        device = (VkDevice *)GetDevice();
        phyDevice = (VkPhysicalDevice *)GetPhyDevice();

        CreateCommandPools();
        CreateQueues();
    }

    // TODO: when we want different queues,
    // we just need to add a input for selecting which we want to use
    VkCommandBuffer BeginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = graphicPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(*device, &allocInfo, &commandBuffer);

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

        vkFreeCommandBuffers(*device, graphicPool, 1, &commandBuffer);
    }

    void SubmitToGraphicQueue(VkSubmitInfo *info, VkFence *fence) {
        if (vkQueueSubmit(graphicsQueue, 1, info, *fence) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }
    }

    void PresentOnPresentQueue(VkPresentInfoKHR *info) {
        VkResult result = vkQueuePresentKHR(presentQueue, info);
    }

    void CleanUp() {
        vkDestroyCommandPool(*device, graphicPool, nullptr);
        vkDestroyCommandPool(*device, presentPool, nullptr);
        vkDestroyCommandPool(*device, computePool, nullptr);
        vkDestroyCommandPool(*device, transferPool, nullptr);
    }
};

void Render::InitVulkanQueueManager() {
    vulkanQueueManager = new VulkanQueueManager();
}

void *Render::BeginSingleTimeCommands() {
    return vulkanQueueManager->BeginSingleTimeCommands();
}

void Render::EndSingleTimeCommands(void *buffer) {
    vulkanQueueManager->EndSingleTimeCommands((VkCommandBuffer)buffer);
}

void *Render::GetGraphicPool() {
    return &vulkanQueueManager->graphicPool;
}

void Render::SubmitToGraphicQueue(void *info, void *fence) {
    vulkanQueueManager->SubmitToGraphicQueue((VkSubmitInfo *)info, (VkFence *)fence);
}

void Render::VulkanQueueManagerCleanUp() {
    vulkanQueueManager->CleanUp();
}

void Render::PresentOnPresentQueue(void *info) {
    vulkanQueueManager->PresentOnPresentQueue((VkPresentInfoKHR *)info);
}

Render::VulkanQueueManager *Render::vulkanQueueManager = nullptr;

}  // namespace Ignis
