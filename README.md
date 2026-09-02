# video-pipeline-vulkan

A Vulkan compute implementation of a video processing pipeline that also exists
as synthesizable RTL, built to compare the **same algorithm on three
architectures**: FPGA, CPU and GPU.

The RTL version lives in a separate repo
([video-pipeline](https://github.com/JesseOinonen/video-pipeline)) and is a
streaming AXI-Stream design — ROI → grayscale → Gaussian → Sobel, one pixel per
clock at 188 MHz. This repo is the GPU-compute port of the same filters, and the
two are validated against a shared golden model so the comparison is between
architectures rather than between implementations that happen to disagree.

The interesting question is not which is faster. It is **what each architecture
is actually good at**: the FPGA pipeline is deep and narrow (one pixel per clock,
a few clocks of latency, deterministic), while the GPU is wide and shallow (32
pixels per instruction, thousands of invocations in flight, but a whole frame has
to be resident before anything comes out).

## Status

| Stage | State |
|---|---|
| Vulkan setup: instance, device, compute queue, command pool | done |
| Buffers, memory type selection, staging → device-local path | done |
| Recording, pipeline barriers, submit, fence | done |
| First compute shader: **grayscale** | done — **0 mismatches vs the golden model**, all 921 600 pixels |
| ROI, Gaussian, Sobel | not started |
| Benchmark harness (timestamp queries, steady-state loop) | not started |
| CPU reference implementation | partial (`grayRef()` in `src/main.cpp`) |

Current output:

    Loaded 921600 pixels from rgb565data.hex
    Dispatching 7200 workgroups x 64 = 460800 invocations for 460800 elements

    vs CPU reference: 0 mismatches
    vs golden model : 0 mismatches
    Grayscale PASSED

## Method

Comparing architectures fairly means being strict about what is held constant
and deliberately free about what is not.

**Fixed for all three implementations:**

- the same input file and image size (1280x720 RGB565)
- **bit-exact** output — not "visually similar", identical to the golden model
- the same arithmetic definition, including integer rounding. Grayscale is
  `(77*R + 150*G + 29*B) >> 8` with MSB-replication channel expansion in every
  implementation; using floats on the GPU would produce scattered off-by-one
  differences and would mean comparing rounding rather than hardware.

**Free per implementation:**

- memory layout, word width, packing
- parallelisation strategy — 1 pixel/clock streaming vs 460 800 invocations
- tiling, vectorisation, shared memory, unrolling

So the GPU port packs **two RGB565 pixels into one 32-bit word**, because the
filter is memory-bound (~1 integer op per byte) and that halves the number of
memory transactions. That is a layout decision inside the GPU implementation; the
bytes on disk are unchanged, so it does not weaken the comparison.

**What will be measured**, separately rather than as one headline number:

- **throughput** — steady state, frames/s
- **latency** — one frame, first pixel in to last pixel out
- **GPU end-to-end vs kernel-only** — kernel-only flatters the GPU, end-to-end is
  what a real system pays. The FPGA has no equivalent split because it streams,
  which is exactly why both GPU numbers are reported
- **performance per watt** — without it the comparison is decided in advance

## Build and run

Requires the Vulkan SDK (or `libvulkan-dev` + `vulkan-validationlayers`) and
`glslangValidator`.

    cmake -S . -B build
    cmake --build build
    ./build/vpv

Shaders are compiled to SPIR-V as part of the build, so editing a `.comp` file is
enough. Validation layers are enabled automatically when present. Synchronisation
validation is not on by default and is worth enabling, since it catches missing
barriers that happen to work on one machine:

    VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT ./build/vpv

Test data (input image and golden outputs) is not committed — it is a copy of the
RTL project's testbench data, which is the single source of truth:

    scripts/sync-test-data.sh [path-to-video-pipeline-repo]

## Layout

    shaders/        compute shaders (.comp -> .spv at build time)
    include/        vk_context, vk_buffer, vk_pipeline, hex_io
    src/            implementation + main.cpp orchestration
    data/           input image, golden outputs, golden model scripts
    docs/           one document per stage, with diagrams
    scripts/        test data sync

## Documentation

Each stage is written up as it is finished: what was built, why it is shaped
that way, and which parts of the API are easy to get subtly wrong.

| | |
|---|---|
| [01 — Instance, device, queue, command buffer](docs/01-instance-device-queue.md) | the object chain, the two recurring API patterns, teardown discipline |
| [02 — Buffers and memory](docs/02-buffers-and-memory.md) | why a buffer has no memory, heaps vs types, staging vs device-local on both machines |
| [03 — Recording, barriers and submission](docs/03-recording-and-submission.md) | recording order is not execution order; barriers, fences, synchronisation validation |
| [04 — Project structure](docs/04-project-structure.md) | splitting the code, RAII for Vulkan handles, CMake and include paths |
| [05 — First compute shader](docs/05-first-compute-shader.md) | GLSL, SIMT and divergence, the six pipeline objects, bit-exact validation |

Diagrams (draw.io): [init](docs/vulkan-init.drawio),
[buffers](docs/buffer-memory.drawio),
[recording](docs/recording-submission.drawio),
[compute pipeline](docs/grayscale-pipeline.drawio).

## Roadmap

1. Extract the pipeline objects into a reusable struct now that one filter works
   and what varies between filters is known rather than guessed
2. Benchmark harness — timestamp queries scaled by the device's own
   `timestampPeriod`, a steady-state loop, and a CPU baseline. Built
   filter-agnostically so it is not shaped around grayscale
3. ROI, Sobel, Gaussian, each validated against its own golden model
4. Sobel from a naive implementation to a shared-memory tiled one, measured both
   ways — a naive 3x3 convolution reads every pixel nine times
5. The full chain ROI → grayscale → Gaussian → Sobel, as separate dispatches and
   then fused. This measures what the FPGA gets for free by streaming and what
   the GPU pays for every round trip through global memory
6. Nsight profiling and the final three-way comparison

## Hardware

| | |
|---|---|
| Benchmarks | NVIDIA RTX 3060 — 6 queue families, VRAM and system memory on separate heaps, so staging copies are real PCIe transfers |
| Development | Intel Iris Xe (TGL GT2) — one universal queue family, one unified memory heap |
| FPGA | the RTL project, 125 MHz, 1 pixel/clock |

Device selection prefers a discrete GPU and falls back to an integrated one, so
the same binary runs on both. Nothing branches on vendor: the code queries device
properties and reacts to what it finds. Benchmark numbers come from the 3060
only; the laptop is for correctness.
