#include "vk_pipeline.hpp"
#include <fstream>
#include <vector>
#include <stdexcept>
#include <string>

VkShaderModule loadShader(VkDevice device, const char* path){
    // Open as binary and seek to end to get size
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file) {throw std::runtime_error(std::string("Failed to open shader file: ") + path);}

    // Size and seek back to beginning
    std::streamsize size = file.tellg();
    file.seekg(0);

    if(size % 4 != 0) {throw std::runtime_error(std::string("Shader file size is not a multiple of 4: ") + path);}

    // Read to vector
    std::vector<char> code(static_cast<size_t>(size));
    file.read(code.data(), size);

    // Create info
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {throw std::runtime_error(std::string("Failed to create shader module: ") + path);}

    return shaderModule;
}

void ComputePipeline::init(VkDevice device, const char* spirvPath, uint32_t bindingCount){
    this->device = device;

    ////////////////////////
    // Load shaders
    VkShaderModule shader = loadShader(device, spirvPath);


    ////////////////////////
    // Descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> bindings(bindingCount);
    for (uint32_t i = 0; i < bindingCount; ++i) {
        bindings[i].binding         = i;
        bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
    descriptorSetLayoutCreateInfo.pBindings    = bindings.data();
    if(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {throw std::runtime_error("Failed to create descriptor set layout");}

    ////////////////////////
    // Descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = bindingCount;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes    = &poolSize;
    descriptorPoolCreateInfo.maxSets       = 1;
    if(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS) {throw std::runtime_error("Failed to create descriptor pool");}


    ////////////////////////
    // Descriptor set
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
    descriptorSetAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocInfo.descriptorPool     = descriptorPool;
    descriptorSetAllocInfo.descriptorSetCount = 1;
    descriptorSetAllocInfo.pSetLayouts        = &descriptorSetLayout;
    if (vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &descriptorSet) != VK_SUCCESS) {throw std::runtime_error("Failed to allocate descriptor set");}

    ////////////////////////
    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts    = &descriptorSetLayout;
    if(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {throw std::runtime_error("Failed to create pipeline layout");}


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
    if(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {throw std::runtime_error("Failed to create compute pipeline");}

    vkDestroyShaderModule(device, shader, nullptr);
}

void ComputePipeline::bindBuffers(const std::vector<VkBuffer>& buffers){
    ////////////////////////
    // Update descriptor sets
    std::vector<VkDescriptorBufferInfo> bufferInfos(buffers.size());
    std::vector<VkWriteDescriptorSet>   writes(buffers.size());

    for (size_t i = 0; i < buffers.size(); i++) {
        bufferInfos[i].buffer = buffers[i];
        bufferInfos[i].offset = 0;
        bufferInfos[i].range  = VK_WHOLE_SIZE;

        writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet          = descriptorSet;
        writes[i].dstBinding      = static_cast<uint32_t>(i);
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo     = &bufferInfos[i];
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

ComputePipeline::~ComputePipeline(){
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
}