#pragma once
#include <vulkan/vulkan.h>

// Read a compiled SPIR-V file and turn it into a shader module
VkShaderModule loadShader(VkDevice device, const char* path);