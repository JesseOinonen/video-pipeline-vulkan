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
    for (auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        std::cout << "  - " << props.deviceName
                  << " (type=" << props.deviceType << ")\n";

        // Choosing first discrete GPU
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){ chosen = dev;}
    }

    if (chosen == VK_NULL_HANDLE) {throw std::runtime_error("Discrete GPU not found!");}

    VkPhysicalDeviceProperties chosenProps;
    vkGetPhysicalDeviceProperties(chosen, &chosenProps);
    std::cout << "\nChosen device: " << chosenProps.deviceName << "\n";

    vkDestroyInstance(instance, nullptr);
    return 0;
}