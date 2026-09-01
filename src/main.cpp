#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <stdexcept>

int main() {
    // Instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "video-pipeline-vulkan";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {throw std::runtime_error("Vulkan instance failed");}

    // Physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {throw std::runtime_error("No Vulkan compatible devices found!");}

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::cout <<"Found devices:\n";
    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    VkPhysicalDevice fallback = VK_NULL_HANDLE;
    for (auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        std::cout << "  - " << props.deviceName
                  << " (type=" << props.deviceType << ")\n";

        // Choosing first discrete GPU
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){ chosen = dev; break;}
        // Fallback of integrated GPU for example for laptops
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {fallback = dev;}
    }

    if (chosen == VK_NULL_HANDLE) {
        if (fallback != VK_NULL_HANDLE) {chosen = fallback;}
        else{throw std::runtime_error("Discrete GPU not found!");}
    }

    VkPhysicalDeviceProperties chosenProps;
    vkGetPhysicalDeviceProperties(chosen, &chosenProps);
    std::cout << "\nChosen device: " << chosenProps.deviceName << "\n";

    ///////////////////////////////////
    // Queue family lookup
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {throw std::runtime_error("No queue families found!");}

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &queueFamilyCount, queueFamilies.data());

    uint32_t computeFamily = UINT32_MAX;
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

    //////////////////////////
    // Create device
    VkDevice device = VK_NULL_HANDLE;

    if (vkCreateDevice(chosen, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {throw std::runtime_error("Vulkan device failed");}

    //////////////////////////
    // Get device queue
    VkQueue computeQueue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);

    ////////////////////////
    // Destroy
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}