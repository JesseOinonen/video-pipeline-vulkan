#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

struct BenchResult {
    double firstMs  = 0.0;  // milliseconds for first run, closest to one-frame latency
    double medianMs = 0.0;  // steady-state median milliseconds for subsequent runs
    double minMs    = 0.0;
    double maxMs    = 0.0;
    std::vector<double> samples;
};

// Median of a sample set. Takes by value: it sorts
double medianMs(std::vector<double> samples);

// submiut an already-recorded command buffer 'iterations' times, waiting for each to complete and time each submission
BenchResult benchmarkCommandBuffer(VkDevice device, 
                                   VkQueue queue, 
                                   VkCommandBuffer cmd,
                                   VkFence fence, 
                                   uint32_t iterations);
