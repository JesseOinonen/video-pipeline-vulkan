#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>

uint32_t findMemoryType (VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties){
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

void createBuffer(VkDevice device, 
                  VkPhysicalDevice physicalDevice, 
                  VkDeviceSize size, 
                  VkBufferUsageFlags usage, 
                  VkMemoryPropertyFlags properties, 
                  VkBuffer& buffer, 
                  VkDeviceMemory& memory) {
    // The description
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {throw std::runtime_error("Failed to create buffer");}

    // what the driver actually needs
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

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

    if (vkAllocateMemory(device, &memAllocInfo, nullptr, &memory) != VK_SUCCESS) {throw std::runtime_error("Failed to allocate buffer memory");}

    // tie them together
    if (vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS) {throw std::runtime_error("Failed to bind buffer memory");}
}

int main() {

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

    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {throw std::runtime_error("Vulkan instance failed");}

    ////////////////////////
    // Physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {throw std::runtime_error("No Vulkan compatible devices found!");}

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::cout <<"Found devices:\n";
    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    VkPhysicalDevice fallback = VK_NULL_HANDLE;
    for (const auto& dev : devices) {
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

    VkDevice device = VK_NULL_HANDLE;

    if (vkCreateDevice(chosen, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {throw std::runtime_error("Vulkan device failed");}

    //////////////////////////
    // Get device queue
    VkQueue computeQueue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);

    //////////////////////////
    // Command pool
    VkCommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.queueFamilyIndex = computeFamily;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool commandPool = VK_NULL_HANDLE;

    if (vkCreateCommandPool(device, &poolCreateInfo, nullptr, &commandPool) != VK_SUCCESS) {throw std::runtime_error("Failed to create command pool");}

    //////////////////////////
    // Command buffer (allocated not created)
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer) != VK_SUCCESS) {throw std::runtime_error("Failed to allocate command buffer");}

    //////////////////////////
    // Buffers an memory

    // Fetch the whole memory map
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(chosen, &memProps);

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
              << findMemoryType(chosen, UINT32_MAX,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) << "\n";
    std::cout << "  DEVICE_LOCAL               -> type "
              << findMemoryType(chosen, UINT32_MAX,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) << "\n";
    std::cout << "  DEVICE_LOCAL|HOST_VISIBLE  -> type "
              << findMemoryType(chosen, UINT32_MAX,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) << "\n";

    //////////////////////////
    // Buffers
    const VkDeviceSize bufferSize = 1280 * 720 * sizeof(uint16_t);
    std::cout << "\nAllocating buffers (" << bufferSize << " B each):\n";

    VkBuffer       stagingIn       = VK_NULL_HANDLE;
    VkDeviceMemory stagingInMemory = VK_NULL_HANDLE;
    createBuffer(device, chosen, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingIn, stagingInMemory);

    VkBuffer       deviceBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory deviceBufferMemory = VK_NULL_HANDLE;
    createBuffer(device, chosen, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 deviceBuffer, deviceBufferMemory);

    VkBuffer       stagingOut       = VK_NULL_HANDLE;
    VkDeviceMemory stagingOutMemory = VK_NULL_HANDLE;
    createBuffer(device, chosen, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingOut, stagingOutMemory);

    void* data = nullptr;
    if(vkMapMemory(device, stagingInMemory, 0, bufferSize, 0, &data) != VK_SUCCESS) {throw std::runtime_error("Failed to map memory");}
    uint16_t* pixels = static_cast<uint16_t*>(data);
    for (size_t i = 0; i < 1280 * 720; i++) {
        pixels[i] = static_cast<uint16_t>(i & 0xFFFF);
    }
    vkUnmapMemory(device, stagingInMemory);
    std::cout << "Filled stagingIn with a test pattern.\n";

    ////////////////////////
    // Begin recording
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {throw std::runtime_error("Failed to begin command buffer");}

    // First copy from stagingIn to deviceBuffer
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;

    vkCmdCopyBuffer(commandBuffer, stagingIn, deviceBuffer, 1, &copyRegion);

    // BARRIER
    VkBufferMemoryBarrier bufferBarrier{};
    bufferBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufferBarrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer              = deviceBuffer;
    bufferBarrier.offset              = 0;
    bufferBarrier.size                = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        commandBuffer,                   // commandBuffer
        VK_PIPELINE_STAGE_TRANSFER_BIT,  // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT,  // dstStageMask
        0,                               // dependencyFlags
        0, nullptr,                      // memory barriers
        1, &bufferBarrier,               // buffer memory barriers
        0, nullptr                       // image memory barriers
    );

    // Second copy from deviceBuffer to stagingOut
    vkCmdCopyBuffer(commandBuffer, deviceBuffer, stagingOut, 1, &copyRegion);

    // Stop recording
    if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {throw std::runtime_error("Failed to end command buffer");}

    // Fence
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {throw std::runtime_error("Failed to create fence");}

    // Submit
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if(vkQueueSubmit(computeQueue, 1, &submitInfo, fence) != VK_SUCCESS) {throw std::runtime_error("Failed to submit queue");}

    // Wait
    if(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {throw std::runtime_error("Failed to wait for fences");}

    // Check
    if(vkMapMemory(device, stagingOutMemory, 0, bufferSize, 0, &data) != VK_SUCCESS) {throw std::runtime_error("Failed to map memory");}
    pixels = static_cast<uint16_t*>(data);
    bool success = true;
    for (size_t i = 0; i < 1280 * 720; i++) {
        if (pixels[i] != static_cast<uint16_t>(i & 0xFFFF)) {
            std::cerr << "Mismatch at index " << i << ": expected "<< (i & 0xFFFF) << ", got " << pixels[i] << "\n";
            success = false;
            break;
        }
    }
    vkUnmapMemory(device, stagingOutMemory);

    std::cout << (success ? "Copy test PASSED: stagingIn -> deviceBuffer -> stagingOut\n"
                          : "Copy test FAILED\n");

    ////////////////////////
    // Destroy/clean up
    vkDestroyFence(device, fence, nullptr);
    vkDestroyBuffer(device, stagingOut, nullptr);
    vkFreeMemory(device, stagingOutMemory, nullptr);
    vkDestroyBuffer(device, deviceBuffer, nullptr);
    vkFreeMemory(device, deviceBufferMemory, nullptr);
    vkDestroyBuffer(device, stagingIn, nullptr);
    vkFreeMemory(device, stagingInMemory, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}