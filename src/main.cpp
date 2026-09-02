#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include "vk_buffer.hpp"
#include "vk_context.hpp"

int main() {

    //////////////////////////
    // Vulkan context
    VulkanContext ctx;
    ctx.init();

    //////////////////////////
    // Buffers an memory

    // Fetch the whole memory map
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(ctx.physicalDevice, &memProps);

    std::cout << "\nMemory heaps: " << memProps.memoryHeapCount << "\n";
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++){
        std::cout << " - Heap " << i << ": "
                  << (memProps.memoryHeaps[i].size / (1024 * 1024)) << " MiB"
                  << ", flags=" << memProps.memoryHeaps[i].flags << "\n";
    }

    std::cout << "\nMemory types: " << memProps.memoryTypeCount << "\n";
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++){
        std::cout << " - Type " << i
                  << ": heap=" << memProps.memoryTypes[i].heapIndex
                  << ", flags=" << memProps.memoryTypes[i].propertyFlags << "\n";
    }

    // Find memory type
    std::cout << "\nfindMemoryType checks:\n";
    std::cout << "  HOST_VISIBLE|HOST_COHERENT -> type "
              << findMemoryType(ctx.physicalDevice, UINT32_MAX,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) << "\n";
    std::cout << "  DEVICE_LOCAL               -> type "
              << findMemoryType(ctx.physicalDevice, UINT32_MAX,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) << "\n";
    std::cout << "  DEVICE_LOCAL|HOST_VISIBLE  -> type "
              << findMemoryType(ctx.physicalDevice, UINT32_MAX,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) << "\n";

    //////////////////////////
    // Buffers
    const VkDeviceSize bufferSize = 1280 * 720 * sizeof(uint16_t);
    std::cout << "\nAllocating buffers (" << bufferSize << " B each):\n";

    Buffer stagingIn = createBuffer(ctx.device, ctx.physicalDevice, bufferSize,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    Buffer deviceBuffer = createBuffer(ctx.device, ctx.physicalDevice, bufferSize,
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer stagingOut = createBuffer(ctx.device, ctx.physicalDevice, bufferSize,
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data = nullptr;
    if(vkMapMemory(ctx.device, stagingIn.memory, 0, bufferSize, 0, &data) != VK_SUCCESS) {throw std::runtime_error("Failed to map memory");}
    uint16_t* pixels = static_cast<uint16_t*>(data);
    for (size_t i = 0; i < 1280 * 720; i++) {
        pixels[i] = static_cast<uint16_t>(i & 0xFFFF);
    }
    vkUnmapMemory(ctx.device, stagingIn.memory);
    std::cout << "Filled stagingIn with a test pattern.\n";

    ////////////////////////
    // Begin recording
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(ctx.commandBuffer, &beginInfo) != VK_SUCCESS) {throw std::runtime_error("Failed to begin command buffer");}

    // First copy from stagingIn to deviceBuffer
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;

    vkCmdCopyBuffer(ctx.commandBuffer, stagingIn.buffer, deviceBuffer.buffer, 1, &copyRegion);

    // BARRIER
    VkBufferMemoryBarrier bufferBarrier{};
    bufferBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufferBarrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer              = deviceBuffer.buffer;
    bufferBarrier.offset              = 0;
    bufferBarrier.size                = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        ctx.commandBuffer,               // ctx.commandBuffer
        VK_PIPELINE_STAGE_TRANSFER_BIT,  // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT,  // dstStageMask
        0,                               // dependencyFlags
        0, nullptr,                      // memory barriers
        1, &bufferBarrier,               // buffer memory barriers
        0, nullptr                       // image memory barriers
    );

    // Second copy from deviceBuffer to stagingOut
    vkCmdCopyBuffer(ctx.commandBuffer, deviceBuffer.buffer, stagingOut.buffer, 1, &copyRegion);

    // Stop recording
    if(vkEndCommandBuffer(ctx.commandBuffer) != VK_SUCCESS) {throw std::runtime_error("Failed to end command buffer");}

    // Fence
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(ctx.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {throw std::runtime_error("Failed to create fence");}

    // Submit
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &ctx.commandBuffer;

    if(vkQueueSubmit(ctx.computeQueue, 1, &submitInfo, fence) != VK_SUCCESS) {throw std::runtime_error("Failed to submit queue");}

    // Wait
    if(vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {throw std::runtime_error("Failed to wait for fences");}

    // Check
    if(vkMapMemory(ctx.device, stagingOut.memory, 0, bufferSize, 0, &data) != VK_SUCCESS) {throw std::runtime_error("Failed to map memory");}
    pixels = static_cast<uint16_t*>(data);
    bool success = true;
    for (size_t i = 0; i < 1280 * 720; i++) {
        if (pixels[i] != static_cast<uint16_t>(i & 0xFFFF)) {
            std::cerr << "Mismatch at index " << i << ": expected "<< (i & 0xFFFF) << ", got " << pixels[i] << "\n";
            success = false;
            break;
        }
    }
    vkUnmapMemory(ctx.device, stagingOut.memory);

    std::cout << (success ? "Copy test PASSED: stagingIn -> deviceBuffer -> stagingOut\n"
                          : "Copy test FAILED\n");

    ////////////////////////
    // Destroy/clean up
    vkDestroyFence(ctx.device, fence, nullptr);
    destroyBuffer(ctx.device, stagingOut);
    destroyBuffer(ctx.device, deviceBuffer);
    destroyBuffer(ctx.device, stagingIn);

    return 0;
}