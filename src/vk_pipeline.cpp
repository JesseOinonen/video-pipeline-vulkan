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