#include "bench.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>

double medianMs(std::vector<double> samples) {
    if (samples.empty()){
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    const size_t mid = samples.size() / 2;
    if (samples.size() % 2 == 1) {return samples[mid];}
    return (samples[mid - 1] + samples[mid]) / 2.0;
}

BenchResult benchmarkCommandBuffer(VkDevice device, 
                                   VkQueue queue, 
                                   VkCommandBuffer cmd,
                                   VkFence fence, 
                                   uint32_t iterations){
    if (iterations < 2) {throw std::runtime_error("benchmarkCommandBuffer requires at least 2 iterations");}

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    std::vector<double> samples;
    samples.reserve(iterations);

    for (uint32_t i = 0; i < iterations; i++){
        vkResetFences(device, 1, &fence);

        const auto t0= std::chrono::steady_clock::now();

        if (vkQueueSubmit(queue, 1, &submitInfo, fence) != VK_SUCCESS) {throw std::runtime_error("Failed to submit queue");}
        if (vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {throw std::runtime_error("Failed to wait for fences");}

        const auto t1 = std::chrono::steady_clock::now();

        samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    std::vector<double> steady(samples.begin() + 1, samples.end());

    BenchResult result;
    result.firstMs  = samples.front();
    result.medianMs = medianMs(steady);

    const auto mm = std::minmax_element(steady.begin(), steady.end());
    result.minMs = *mm.first;
    result.maxMs = *mm.second;

    result.samples = std::move(samples);
    return result;
}