#include "vk_context.hpp"
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cstring>

void VulkanContext::init() {
    ////////////////////////
    // Validation layer
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    bool validationAvailable = false;
    for (const auto& layer : availableLayers) {
        std::cout << "  - " << layer.layerName << " : " << layer.description << "\n";
        if (std::strcmp(layer.layerName, validationLayerName) == 0) {validationAvailable = true;}
    }

    ////////////////////////
    // Instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "video-pipeline-vulkan";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    if (validationAvailable) {
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &validationLayerName;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {throw std::runtime_error("Vulkan instance failed");}

    ////////////////////////
    // Physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {throw std::runtime_error("No Vulkan compatible devices found!");}

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::cout <<"Found devices:\n";
    VkPhysicalDevice fallback = VK_NULL_HANDLE;
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        std::cout << "  - " << props.deviceName
                  << " (type=" << props.deviceType << ")\n";

        // Choosing first discrete GPU
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){ physicalDevice = dev; break;}
        // Fallback of integrated GPU for example for laptops
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {fallback = dev;}
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        if (fallback != VK_NULL_HANDLE) {physicalDevice = fallback;}
        else{throw std::runtime_error("Discrete GPU not found!");}
    }

    VkPhysicalDeviceProperties physicalDeviceProps;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProps);
    std::cout << "\nphysicalDevice device: " << physicalDeviceProps.deviceName << "\n";

    ///////////////////////////////////
    // Queue family lookup
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {throw std::runtime_error("No queue families found!");}

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++){
        std::cout << "  - Queue family " << i << ": "
                  << queueFamilies[i].queueCount << " queues, flags=" << queueFamilies[i].queueFlags << "\n";
        if (computeFamily == UINT32_MAX && (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {computeFamily = i;}
    }

    if (computeFamily == UINT32_MAX) {throw std::runtime_error("No compute queue family found!");}

    //////////////////////////
    // Create info for device queue
    VkDeviceQueueCreateInfo queueCreateInfo{}; 
    queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = computeFamily;
    queueCreateInfo.queueCount       = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;


    //////////////////////////
    // Create info for device
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos    = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pEnabledFeatures     = nullptr;
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {throw std::runtime_error("Vulkan device failed");}

    //////////////////////////
    // Get device queue
    vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);

    //////////////////////////
    // Command pool
    VkCommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.queueFamilyIndex = computeFamily;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device, &poolCreateInfo, nullptr, &commandPool) != VK_SUCCESS) {throw std::runtime_error("Failed to create command pool");}

    //////////////////////////
    // Command buffer (allocated not created)
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer) != VK_SUCCESS) {throw std::runtime_error("Failed to allocate command buffer");}

}

VulkanContext::~VulkanContext() {
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}