# video-pipeline-vulkan

A Vulkan compute-shader implementation of the same video processing pipeline that already exists as RTL in a separate repo. The goal is to compare FPGA, CPU, and GPU implementations against each other (throughput/latency), while also building up Vulkan/GPU-architecture-level skills.

## Why this project exists

- Deepens GPU-pipeline and driver-level understanding on top of an IC-design background
- The video processing IP (ROI, grayscale, Sobel, Gaussian) is its **own separate GitHub project**, this repo is a GPU-compute port of that
- End result: a three-way (FPGA/CPU/GPU) performance comparison of the same algorithm

## Environment (verified working)

- GPU: NVIDIA GeForce RTX 3060, Vulkan 1.4 supported
- `vulkaninfo` runs and detects the device
- Shader compiler: `glslangValidator` (note: `glslc` is not installed)
- Nsight Graphics installed (for profiling later)
- CMake + `find_package(Vulkan)` as the build system

## Current status

Built so far:
1. Vulkan instance creation
2. Physical device enumeration and discrete-GPU selection
3. Compute queue family lookup
4. Logical device (`VkDevice`) and compute queue creation

Everything up to this point has been pure Vulkan API mechanics (instance -> device -> queue) -- no actual image processing yet.

## Next steps (in order)

1. **Command pool and command buffer** -- needed before any commands can be submitted to the GPU
2. **Memory allocation (buffer + memory)** -- a simple storage buffer for test data; look at `VkPhysicalDeviceMemoryProperties` (already seen in the `vulkaninfo` output: `MEMORY_PROPERTY_HOST_VISIBLE_BIT` is what's needed for host<->device copies)
3. **First shader: grayscale** -- write `shaders/grayscale.comp`, compile with `glslangValidator -V grayscale.comp -o grayscale.spv`
4. **Descriptor set + compute pipeline** -- binds the buffer to the shader
5. **Dispatch + read back the result** -- run the shader, copy the result back to the host
6. **Validate against the golden model** -- compare against the RTL project's Python golden model (`tests/compare_to_golden.py`)
7. Repeat steps 3-6 for the remaining four filters: ROI, resize, Sobel, Gaussian (in this order, easiest to hardest)
8. **Nsight Graphics profiling** once all filters work and are validated correct
9. **Benchmark comparison** across the FPGA/CPU/GPU trio

## Things to remember

- Real-Time Rendering (the book) is conceptual background for GPU architecture -- it doesn't cover Vulkan API mechanics itself, so there's no need to hunt for a tie-in at every step. It's most relevant when thinking about GPU-friendly algorithm design itself (e.g. using workgroup shared memory in convolution).
- Recurring Vulkan pattern: fill out a "create info" struct -> call a `vk*Create*` function -> check for `VK_SUCCESS`. This same pattern repeats at every stage.
- Another recurring pattern: "query the count first with `nullptr`, then fetch the data with a real buffer" (already seen when enumerating devices and queue families).
- Simics (the Intel contact via Risto) is a separate, lower-priority side project -- no need to keep pace with it alongside this one.