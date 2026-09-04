#pragma once
#include <vulkan/vulkan.h>
#include <vector>

// Read a compiled SPIR-V file and turn it into a shader module
VkShaderModule loadShader(VkDevice device, const char* path);

struct ComputePipeline {
    VkDevice              device              = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSet       = VK_NULL_HANDLE;
    VkPipelineLayout      pipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            pipeline            = VK_NULL_HANDLE;

    ComputePipeline() = default;
    ~ComputePipeline();
    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;

    void init(VkDevice device, const char* spirvPath, uint32_t bindingCount);
    void bindBuffers(const std::vector<VkBuffer>& buffers);
};