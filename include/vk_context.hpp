#pragma once
#include <vulkan/vulkan.h>

struct VulkanContext {
    VkInstance instance             = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device                 = VK_NULL_HANDLE;
    VkQueue computeQueue            = VK_NULL_HANDLE;
    uint32_t computeFamily          = UINT32_MAX;
    VkCommandPool commandPool       = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer   = VK_NULL_HANDLE;

    void init();
    ~VulkanContext();

    VulkanContext(const VulkanContext&)            = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VulkanContext() = default;
};
