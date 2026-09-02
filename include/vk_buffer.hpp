#pragma once
#include <vulkan/vulkan.h>

struct Buffer{
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

uint32_t findMemoryType (VkPhysicalDevice physicalDevice, 
                         uint32_t typeFilter, 
                         VkMemoryPropertyFlags properties);

Buffer createBuffer(VkDevice device, 
                  VkPhysicalDevice physicalDevice, 
                  VkDeviceSize size, 
                  VkBufferUsageFlags usage, 
                  VkMemoryPropertyFlags properties);

void destroyBuffer(VkDevice device, Buffer& buf);
