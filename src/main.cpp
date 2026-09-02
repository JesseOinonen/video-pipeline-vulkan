#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include "vk_buffer.hpp"
#include "vk_context.hpp"
#include "vk_pipeline.hpp"
#include "hex_io.hpp"

// CPU reference for the shader, matching rtl/gray.vhdl and gray_golden.py.
static uint16_t grayRef(uint16_t px) {
    uint32_t r5 = (px >> 11) & 0x1Fu;
    uint32_t g6 = (px >>  5) & 0x3Fu;
    uint32_t b5 =  px        & 0x1Fu;
    uint32_t r8 = (r5 << 3) | (r5 >> 2);
    uint32_t g8 = (g6 << 2) | (g6 >> 4);
    uint32_t b8 = (b5 << 3) | (b5 >> 2);
    return static_cast<uint16_t>((77u * r8 + 150u * g8 + 29u * b8) >> 8);
}

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

    Buffer deviceIn = createBuffer(ctx.device, ctx.physicalDevice, bufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer deviceOut = createBuffer(ctx.device, ctx.physicalDevice, bufferSize,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer stagingOut = createBuffer(ctx.device, ctx.physicalDevice, bufferSize,
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    std::vector<uint16_t> input = readHex(DATA_DIR "/rgb565data.hex");
    if (input.size() != 1280 * 720) {throw std::runtime_error("Unexpected input size in rgb565data.hex");}
    void* data = nullptr;
    if(vkMapMemory(ctx.device, stagingIn.memory, 0, bufferSize, 0, &data) != VK_SUCCESS) {throw std::runtime_error("Failed to map memory");}
    std::memcpy(data, input.data(), bufferSize);
    vkUnmapMemory(ctx.device, stagingIn.memory);
    std::cout << "Loaded " << input.size() << " pixels from rgb565data.hex\n";

    std::vector<uint16_t> golden = readHex(DATA_DIR "/gray_golden.hex");
    if (golden.size() != input.size()) {throw std::runtime_error("Golden size does not match input");}

    ////////////////////////
    // Load shaders
    VkShaderModule shader = loadShader(ctx.device, SHADER_DIR "/grayscale.spv");


    ////////////////////////
    // Descriptor set layout
    VkDescriptorSetLayoutBinding  setLayoutBinding0{};
    setLayoutBinding0.binding            = 0;
    setLayoutBinding0.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    setLayoutBinding0.descriptorCount    = 1;
    setLayoutBinding0.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding  setLayoutBinding1{};
    setLayoutBinding1.binding            = 1;
    setLayoutBinding1.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    setLayoutBinding1.descriptorCount    = 1;
    setLayoutBinding1.stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = 2;
    VkDescriptorSetLayoutBinding bindings[] = {setLayoutBinding0, setLayoutBinding1};
    descriptorSetLayoutCreateInfo.pBindings    = bindings;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    if(vkCreateDescriptorSetLayout(ctx.device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {throw std::runtime_error("Failed to create descriptor set layout");}


    ////////////////////////
    // Descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 2;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes    = &poolSize;
    descriptorPoolCreateInfo.maxSets       = 1;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    if(vkCreateDescriptorPool(ctx.device, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS) {throw std::runtime_error("Failed to create descriptor pool");}


    ////////////////////////
    // Descriptor set
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
    descriptorSetAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocInfo.descriptorPool     = descriptorPool;
    descriptorSetAllocInfo.descriptorSetCount = 1;
    descriptorSetAllocInfo.pSetLayouts        = &descriptorSetLayout;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    vkAllocateDescriptorSets(ctx.device, &descriptorSetAllocInfo, &descriptorSet);


    ////////////////////////
    // Update descriptor sets
    VkDescriptorBufferInfo bufferInfo0{};
    bufferInfo0.buffer = deviceIn.buffer;
    bufferInfo0.offset = 0;
    bufferInfo0.range  = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo bufferInfo1{};
    bufferInfo1.buffer = deviceOut.buffer;
    bufferInfo1.offset = 0;
    bufferInfo1.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writeDescriptorSet0{};
    writeDescriptorSet0.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet0.dstSet          = descriptorSet;
    writeDescriptorSet0.dstBinding      = 0;
    writeDescriptorSet0.dstArrayElement = 0;
    writeDescriptorSet0.descriptorCount = 1;
    writeDescriptorSet0.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSet0.pBufferInfo     = &bufferInfo0;

    VkWriteDescriptorSet writeDescriptorSet1{};
    writeDescriptorSet1.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet1.dstSet          = descriptorSet;
    writeDescriptorSet1.dstBinding      = 1;
    writeDescriptorSet1.dstArrayElement = 0;
    writeDescriptorSet1.descriptorCount = 1;
    writeDescriptorSet1.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSet1.pBufferInfo     = &bufferInfo1;

    VkWriteDescriptorSet writes[] = { writeDescriptorSet0, writeDescriptorSet1 };
    vkUpdateDescriptorSets(ctx.device, 2, writes, 0, nullptr);


    ////////////////////////
    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts    = &descriptorSetLayout;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if(vkCreatePipelineLayout(ctx.device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {throw std::runtime_error("Failed to create pipeline layout");}


    ////////////////////////
    // Compute pipeline
    VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
    shaderStageCreateInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCreateInfo.module = shader;
    shaderStageCreateInfo.pName  = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.stage  = shaderStageCreateInfo;
    computePipelineCreateInfo.layout = pipelineLayout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    if(vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {throw std::runtime_error("Failed to create compute pipeline");}


    ////////////////////////
    // Begin recording
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(ctx.commandBuffer, &beginInfo) != VK_SUCCESS) {throw std::runtime_error("Failed to begin command buffer");}

    // First copy from stagingIn to deviceIn
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;

    vkCmdCopyBuffer(ctx.commandBuffer, stagingIn.buffer, deviceIn.buffer, 1, &copyRegion);

    // BARRIER 1: the copy into deviceIn must finish before the shader reads it
    VkBufferMemoryBarrier beforeDispatch{};
    beforeDispatch.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    beforeDispatch.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    beforeDispatch.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    beforeDispatch.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    beforeDispatch.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    beforeDispatch.buffer              = deviceIn.buffer;
    beforeDispatch.offset              = 0;
    beforeDispatch.size                = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        ctx.commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,        // srcStageMask: the copy
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask: the shader
        0,                                     // dependencyFlags
        0, nullptr,                            // memory barriers
        1, &beforeDispatch,                    // buffer memory barriers
        0, nullptr                             // image memory barriers
    );

    // Run the shader
    vkCmdBindPipeline(ctx.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(ctx.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    const uint32_t elementCount = static_cast<uint32_t>(bufferSize / 4);  // uints, 2 pixels each
    const uint32_t localSize    = 64;                                     // must match local_size_x
    const uint32_t groupCount   = (elementCount + localSize - 1) / localSize;
    std::cout << "\nDispatching " << groupCount << " workgroups x " << localSize
              << " = " << groupCount * localSize << " invocations for "
              << elementCount << " elements\n";
    vkCmdDispatch(ctx.commandBuffer, groupCount, 1, 1);

    // BARRIER 2: the shader must finish writing deviceOut before it is copied out
    VkBufferMemoryBarrier afterDispatch{};
    afterDispatch.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    afterDispatch.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    afterDispatch.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
    afterDispatch.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    afterDispatch.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    afterDispatch.buffer              = deviceOut.buffer;
    afterDispatch.offset              = 0;
    afterDispatch.size                = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        ctx.commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        1, &afterDispatch,
        0, nullptr
    );

    // Copy the result out of deviceOut, not deviceIn
    vkCmdCopyBuffer(ctx.commandBuffer, deviceOut.buffer, stagingOut.buffer, 1, &copyRegion);

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
    const uint16_t* pixels = static_cast<const uint16_t*>(data);

    size_t refMismatches = 0, goldenMismatches = 0;
    for (size_t i = 0; i < 1280 * 720; i++) {
        if (pixels[i] != grayRef(input[i])) {
            refMismatches++;
        }
        if (pixels[i] != golden[i]) {
            goldenMismatches++;
            if (goldenMismatches <= 5) {
                std::cerr << "  [" << i << "] in=0x" << std::hex << input[i]
                          << " gpu=0x" << pixels[i]
                          << " golden=0x" << golden[i] << std::dec << "\n";
            }
        }
    }
    vkUnmapMemory(ctx.device, stagingOut.memory);

    std::cout << "\nvs CPU reference: " << refMismatches    << " mismatches\n";
    std::cout <<   "vs golden model : " << goldenMismatches << " mismatches\n";
    std::cout << ((refMismatches == 0 && goldenMismatches == 0)
                      ? "Grayscale PASSED\n" : "Grayscale FAILED\n");


    ////////////////////////
    // Destroy/clean up
    vkDestroyPipeline(ctx.device, pipeline, nullptr);
    vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
    vkDestroyDescriptorPool(ctx.device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, descriptorSetLayout, nullptr);
    vkDestroyShaderModule(ctx.device, shader, nullptr);
    vkDestroyFence(ctx.device, fence, nullptr);
    destroyBuffer(ctx.device, stagingOut);
    destroyBuffer(ctx.device, deviceOut);
    destroyBuffer(ctx.device, deviceIn);
    destroyBuffer(ctx.device, stagingIn);

    return 0;
}