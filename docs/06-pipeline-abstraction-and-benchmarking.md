# Stage 6 — Pipeline abstraction and the first measurements

Status: **complete.** Adding a filter is now three lines instead of sixty, and
the project produces its first real numbers.

Files: `include/vk_pipeline.hpp` / `src/vk_pipeline.cpp` (`ComputePipeline`),
`include/bench.hpp` / `src/bench.cpp`.

## `ComputePipeline`

Stage 5 created six objects inline in `main()` for a single filter. With four
filters that would have been four copies of the same sixty lines, each adding
four more entries to a hand-maintained teardown list.

The extraction was deliberately done **after** grayscale worked, not before. At
that point what varies between filters is known rather than guessed: the shader
path and the number of bindings. Everything else is identical.

    void init(VkDevice device, const char* spirvPath, uint32_t bindingCount);
    void bindBuffers(const std::vector<VkBuffer>& buffers);

`init()` builds the descriptor set layout, pool, set, pipeline layout and
pipeline. The two hand-written `VkDescriptorSetLayoutBinding` structs became a
loop over `bindingCount` — that loop is the whole parameterisation.

The shader module never becomes a member: it is loaded at the top of `init()` and
destroyed at the bottom, because the pipeline does not reference it once created.
One less object in the lifetime.

`bindBuffers()` takes plain `VkBuffer` handles rather than the project's `Buffer`
struct, so `vk_pipeline` does not need to know about `vk_buffer`. It also does not
know what the buffers *mean* — binding 0 and binding 1 are just slots.

### Three decisions, and why

**`bindingCount` as a bare number.** Every binding is a `STORAGE_BUFFER` visible
to the compute stage, so a count is enough. A richer description becomes worth
adding when some filter actually needs a different descriptor type.

**One pool per pipeline.** A shared pool would be closer to how a real
application does it, but it couples the filters together and forces the budgets
to be worked out up front. Each pipeline owns its own, `maxSets = 1`.

**No push constants yet.** ROI will probably want its crop bounds and the
convolutions will want the image width. Those get added when it is clear whether
push constants or specialization constants fit better.

### The one real trap in `bindBuffers`

`VkWriteDescriptorSet::pBufferInfo` is a pointer, so the
`VkDescriptorBufferInfo` it points at has to still be alive when
`vkUpdateDescriptorSets` runs. Both vectors are therefore sized up front:

    std::vector<VkDescriptorBufferInfo> bufferInfos(buffers.size());
    std::vector<VkWriteDescriptorSet>   writes(buffers.size());

Growing `bufferInfos` with `push_back` while taking `&bufferInfos[i]` would leave
earlier pointers dangling after the first reallocation. It compiles, it runs, and
only the validation layer notices.

### What it bought

    ComputePipeline gray;
    gray.init(ctx.device, SHADER_DIR "/grayscale.spv", 2);
    gray.bindBuffers({ deviceIn.buffer, deviceOut.buffer });

Plus a `.comp` file and one line in `SHADER_SOURCES`. The teardown list in
`main()` no longer grows with each filter — the destructor handles it, and
declaration order (`ctx` first, pipelines last) makes the ordering correct by
construction.

## The benchmark harness

Built filter-agnostic on purpose: `benchmarkCommandBuffer` takes an
already-recorded command buffer, so it does not need rewriting for Sobel.

    BenchResult benchmarkCommandBuffer(VkDevice, VkQueue, VkCommandBuffer,
                                       VkFence, uint32_t iterations);

### Two changes the loop required

**`VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT` had to go.** It is a promise to
the driver that the recording is submitted exactly once. Submitting the same
buffer a thousand times makes that promise false, and the validation layer says
so. `flags = 0`.

**The fence has to be reset every iteration.** `vkWaitForFences` does not reset
it, so the second wait would return immediately and every subsequent measurement
would be meaningless. `vkResetFences` before each submit.

### Why the fence wait is inside the timed region

`vkQueueSubmit` returns as soon as the work is queued. Timing only the submit
would measure how fast the CPU can hand work to the driver, not how long the GPU
takes. The fence is the only thing that makes this a measurement of GPU work.

### Statistics

The first iteration is reported separately as `firstMs` and excluded from the
median. Median rather than mean, because individual samples pick up scheduler and
driver noise that a mean would smear into the result.

The CPU baseline uses the same `medianMs` helper so both legs are summarised the
same way. Its inner loop writes into a `std::vector` that is read afterwards —
without a visible result the compiler is free to delete the loop entirely and the
measurement becomes zero.

## First results (Intel Iris Xe, laptop)

| | ms/frame | fps | Mpx/s |
|---|---|---|---|
| GPU steady state (end-to-end) | 2.75 | 363 | 335 |
| GPU first iteration (cold) | 2.45 | 409 | 377 |
| CPU, single-threaded scalar | 5.97 | 167 | 154 |
| FPGA at 181.8 MHz (calculated) | 5.07 | 197 | 182 |

The FPGA figure is `921600 / 181.8e6`, from a design that meets a 5.5 ns clock
constraint at one pixel per clock.

### The cold run is faster than the steady state

That is backwards — a first iteration normally pays for cold caches. The likely
explanation is that the laptop's integrated GPU shares a power and thermal budget
with the CPU, boosts at the start and throttles under a thousand back-to-back
iterations.

Worth confirming by recording `minMs` / `maxMs` and comparing samples from the
start and end of the run, and worth re-checking on the 3060, where the power
budget is not shared the same way.

### What these numbers do not yet say

**They are laptop numbers.** Iris Xe has unified memory, so "end-to-end" does not
include a real PCIe transfer. On the 3060 the same measurement is a different
thing entirely, and the gap between it and a kernel-only figure is the whole
point.

**The CPU leg is single-threaded and scalar.** That is an honest starting point
but it flatters the GPU: the same loop with OpenMP and SIMD would be several
times faster. Either the baseline gets improved or the limitation is stated
plainly — leaving it unsaid would be the one thing that undermines the
comparison.

**Latency is not measured yet.** `firstMs` is a cold throughput sample, not a
latency figure. The measurement that matters is time-to-first-output-pixel, and
that is where the two architectures differ by orders of magnitude rather than by
a factor of two.

## Next

1. **Timestamp queries** — `VkQueryPool` of type `VK_QUERY_TYPE_TIMESTAMP`,
   `vkCmdWriteTimestamp` either side of the dispatch, results scaled by the
   device's own `limits.timestampPeriod`. This separates kernel time from
   transfer time.
2. **Run everything on the RTX 3060.** No code changes; this is where the numbers
   start meaning something.
3. **Latency** — one round trip including the host copies, measured once rather
   than in a loop.
4. Then ROI, Sobel and Gaussian, and Sobel's shared-memory optimisation.

Power is deliberately not measured from inside the program — Vulkan exposes no
portable way. It is an external procedure per platform: `nvidia-smi` sampling
during a long run for the 3060, RAPL for the CPU, and a Vivado post-implementation
estimate for the FPGA, labelled as an estimate.
