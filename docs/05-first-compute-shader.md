# Stage 5 — First compute shader: grayscale

Status: **complete.** GPU output matches `gray_golden.hex` bit for bit —
0 mismatches across all 921 600 pixels.

    Loaded 921600 pixels from rgb565data.hex
    Dispatching 7200 workgroups x 64 = 460800 invocations for 460800 elements

    vs CPU reference: 0 mismatches
    vs golden model : 0 mismatches
    Grayscale PASSED

See [`grayscale-pipeline.drawio`](grayscale-pipeline.drawio) for the object
graph and the recording change in one picture.

Files: `shaders/grayscale.comp`, `src/vk_pipeline.cpp` (shader loading),
`src/hex_io.cpp` (test data), and the pipeline/descriptor block in `src/main.cpp`.

## The shader

The mental shift: **you write the body of one invocation, not a loop.** There is
no `for` over pixels — `vkCmdDispatch` decides how many times the body runs, and
each invocation works out which element it owns from `gl_GlobalInvocationID.x`.

Closest analogue from the RTL side is a `generate` statement: describe one
instance, let it be replicated. The difference is that on an FPGA the replication
is physical and permanent; on a GPU it is scheduling.

    #version 450
    layout(local_size_x_id = 0, local_size_x = 64) in;
    layout(std430, binding = 0) readonly  buffer InBuf  { uint data []; } inBuf;
    layout(std430, binding = 1) writeonly buffer OutBuf { uint data []; } outBuf;

| Piece | Why it is written that way |
|---|---|
| `local_size_x = 64` | workgroup size; should be a multiple of the hardware subgroup width (32 on Nvidia) or lanes are wasted |
| `local_size_x_id = 0` | also exposes it as specialization constant 0, so the host can sweep it at benchmark time without touching this file |
| `std430` | tight array packing; `std140` would pad every element to 16 bytes |
| `binding = 0 / 1` | a contract with the descriptor set layout in C++ — mismatching these produces silently wrong data, not an error |
| `uint`, not a 16-bit type | the packing decision: one element = two RGB565 pixels |
| `.length()` | comes from the bound buffer's size, so no push constant is needed to pass the element count |

### Why two pixels per `uint`

The filter is **memory-bound, not compute-bound**: about five integer operations
per pixel against 2 bytes in and 2 bytes out, so roughly 1 op/byte. On a 3060
(~360 GB/s, many integer TOPS) the arithmetic is free and only memory transaction
efficiency matters.

Packed, each invocation issues one aligned 32-bit load and gets two pixels:
460 800 loads instead of 921 600, perfectly coalesced.

The alternative — widening to 32 bits per pixel on the host — doubles both PCIe
traffic and device memory traffic, which is a straight 2x loss on the only cost
that matters here. A third option, `VK_KHR_16bit_storage` so the shader can index
`uint16_t` directly, is supported on both machines but buys nothing: the hardware
moves memory in 32-byte sectors regardless.

Packing also sets up the next step: for Sobel and Gaussian the natural load is a
`uvec4` — 16 bytes, 8 pixels, the ideal per-thread width on both vendors.

**Packing does not change the data contract.** `rgb565data.hex` is read
unmodified; two consecutive pixels simply share a 32-bit word in memory. It is a
reinterpretation, not a conversion — the same idea as the `static_cast<uint16_t*>`
on a mapped pointer.

### Bit-exactness

The arithmetic is integer only, mirroring `rtl/gray.vhdl`:

    r8 = (r5 << 3) | (r5 >> 2)      // MSB replication, not r5 * 255 / 31
    gray = (77*r8 + 150*g8 + 29*b8) >> 8

Floats here would produce scattered off-by-one mismatches that look exactly like
a real bug. `77 + 150 + 29 = 256`, so the result never exceeds 255 and no clamp
is needed.

Which half of the `uint` holds which pixel matters: on a little-endian host the
**even** pixel lands in the low 16 bits. Getting this backwards does not look
broken — it fails on every second pixel, which is why the verification counts
mismatches rather than stopping at the first.

## SIMT, and why it matters later

The hardware groups invocations into fixed-size bundles — a warp (32 on Nvidia)
or subgroup (8/16/32 on Intel Xe) — with **one instruction fetch driving N
execution lanes**. This is control logic amortised over many datapaths, which is
also the clearest way to state the difference being measured: the FPGA pipeline
is deep and narrow (1 px/clock), the GPU is wide and shallow (32 pixels per
instruction, many warps interleaved to hide memory latency).

The consequence is **divergence**: if invocations in a warp take different
branches, the hardware runs both paths serially with lanes masked off, so the
cost is the sum rather than the maximum.

Grayscale has no data-dependent branching at all — the ideal SIMT case. Sobel
will not: border pixels need different handling, so border strategy there is a
performance question as well as a correctness one.

## The six objects, and what they actually are

| Vulkan | Ordinary-code equivalent |
|---|---|
| `VkDescriptorSetLayout` | a function's **parameter list** — "two storage buffers, at slots 0 and 1" |
| `VkDescriptorPool` | where argument lists are allocated from |
| `VkDescriptorSet` | one concrete **argument list** |
| `vkUpdateDescriptorSets` | **filling in** the arguments — "slot 0 is this buffer" |
| `VkPipelineLayout` | the full **signature** (all sets plus push constants) |
| `vkCmdBindDescriptorSets` | **passing** the arguments |
| `vkCmdDispatch` | the **call** |

The layout describes shape, the set holds content, and one layout can serve many
sets — which is how the remaining filters will reuse it.

Details worth remembering:

- `VkDescriptorPool` has **two independent budgets**: `maxSets` (how many sets)
  and `descriptorCount` per type (how many descriptors in total). One set holding
  two buffers means `maxSets = 1`, `descriptorCount = 2`. Running out of either
  gives `VK_ERROR_OUT_OF_POOL_MEMORY` from `vkAllocateDescriptorSets`.
- Descriptor sets are **allocated**, not created, and are freed with the pool —
  the same exception as command buffers.
- `vkUpdateDescriptorSets` is **not** a `vkCmd*`. It runs immediately on the CPU;
  the command buffer only refers to the set.
- `VkDescriptorBufferInfo` has no `sType`; `VkWriteDescriptorSet` does. Parameter
  structs that never reach a `vkCreate*` do not carry one.
- `pName = "main"` in the shader stage is the SPIR-V entry point name, a parameter
  because one module can hold several.
- `vkCreateComputePipelines` is plural and takes a `VkPipelineCache` as its second
  argument (`VK_NULL_HANDLE` here; potentially interesting at benchmark time).
- The shader module can be destroyed as soon as the pipeline exists.

## The recording change

Only the middle of the round trip changed. The shader needs a barrier on each
side of it:

| | before | after |
|---|---|---|
| barrier 1 (`deviceIn`) | `TRANSFER_WRITE -> TRANSFER_READ`, stages `TRANSFER -> TRANSFER` | `TRANSFER_WRITE -> SHADER_READ`, stages `TRANSFER -> COMPUTE_SHADER` |
| dispatch | — | bind pipeline, bind descriptor set, `vkCmdDispatch(7200, 1, 1)` |
| barrier 2 (`deviceOut`) | did not exist | `SHADER_WRITE -> TRANSFER_READ`, stages `COMPUTE_SHADER -> TRANSFER` |
| final copy | read `deviceIn` | reads `deviceOut` |

A barrier answers two questions: *who wrote before me, at which stage* and *who
reads after me, at which stage*. The first says "the copy is done, the shader may
read"; the second says "the shader is done, the copy may read".

`vkCmdDispatch` takes **workgroups**, not invocations:
`(460800 + 63) / 64 = 7200`. The rounding-up form is what makes the bounds check
in the shader necessary, and it matters as soon as `local_size` is swept.

## Verifying: two checks, not one

A golden comparison alone tests three things at once — file loading, shader
arithmetic, and output format — so a failure would not say which. Keeping a CPU
reference (`grayRef()`, the same integer arithmetic in C++) splits it:

| CPU ref | golden | diagnosis |
|---|---|---|
| pass | pass | done |
| pass | fail | file loading or output format |
| fail | fail | shader, or the descriptor wiring |

The comparison counts mismatches instead of stopping at the first, because the
*pattern* localises the bug: every second pixel wrong means the packed halves are
swapped; everything off by one means floating point crept in; everything wrong
means the data never arrived.

Hand-checking one pixel against the golden before running was also worth the two
minutes: `0xbdf7` -> r5=23, g6=47, b5=23 -> r8=189, g8=190, b8=189 ->
`(77*189 + 150*190 + 29*189) >> 8 = 189 = 0xbd`, and the golden says `0x00bd`.
That confirmed the arithmetic independently, so any failure had to be plumbing.

## Build changes

Shader compilation is now a CMake rule rather than a manual step — the risk
otherwise is debugging a stale `.spv` after editing the source:

    add_custom_command(OUTPUT ${SPIRV} ... DEPENDS ${CMAKE_SOURCE_DIR}/${SHADER})
    add_custom_target(shaders ALL DEPENDS ${SPIRV_BINARIES})
    add_dependencies(vpv shaders)

Paths come in as compile-time constants so the program works from any working
directory:

    SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
    DATA_DIR="${CMAKE_SOURCE_DIR}/data"

used as `SHADER_DIR "/grayscale.spv"` — adjacent string literals concatenate.

`set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` plus a `compile_commands.json` symlink at
the repo root: without it the editor flags `SHADER_DIR` as undefined even though
the compiler is perfectly happy, because clangd has no idea what flags CMake
passes.

## Next

Continues in
[Stage 6 — Pipeline abstraction and the first measurements](06-pipeline-abstraction-and-benchmarking.md).
