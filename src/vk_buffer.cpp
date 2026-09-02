#include "vk_buffer.hpp"
#include <stdexcept>
#include <iostream>

uint32_t findMemoryType (VkPhysicalDevice physicalDevice, 
                         uint32_t typeFilter, 
                         VkMemoryPropertyFlags properties){
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        bool typeAllowed   = (typeFilter & (1u << i)) != 0;
        bool hasProperties = (memProps.memoryTypes[i].propertyFlags & properties) == properties; 

        if (typeAllowed && hasProperties) {
            return i;
        }
    }
    throw std::runtime_error("No suitable memory type found");
}

Buffer createBuffer(VkDevice device, 
                  VkPhysicalDevice physicalDevice, 
                  VkDeviceSize size, 
                  VkBufferUsageFlags usage, 
                  VkMemoryPropertyFlags properties) {
    Buffer buf{};

    // The description
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buf.buffer) != VK_SUCCESS) {throw std::runtime_error("Failed to create buffer");}

    // what the driver actually needs
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buf.buffer, &memRequirements);

    // pick a type, allocate
    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.allocationSize  = memRequirements.size;
    memAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice,
                                                  memRequirements.memoryTypeBits,
                                                  properties);

    std::cout << "  buffer: requested " << size
              << " B, driver wants " << memRequirements.size
              << " B, alignment " << memRequirements.alignment
              << ", memory type " << memAllocInfo.memoryTypeIndex << "\n";

    if (vkAllocateMemory(device, &memAllocInfo, nullptr, &buf.memory) != VK_SUCCESS) {throw std::runtime_error("Failed to allocate buffer memory");}

    // tie them together
    if (vkBindBufferMemory(device, buf.buffer, buf.memory, 0) != VK_SUCCESS) {throw std::runtime_error("Failed to bind buffer memory");}
    
    return buf;
}

void destroyBuffer(VkDevice device, Buffer& buf) {
    vkDestroyBuffer(device, buf.buffer, nullptr);
    vkFreeMemory(device, buf.memory, nullptr);
    buf.buffer = VK_NULL_HANDLE;
    buf.memory = VK_NULL_HANDLE;
}
