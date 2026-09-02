# Stage 4 — Project structure

Status: **complete.** `main.cpp` went from 297 to 162 lines across five files,
with identical output and no warnings under `-Wall -Wextra`.

No Vulkan was learned in this stage. It exists because the shader stage adds six
more objects around a single dispatch, and every one of them would otherwise land
on a hand-maintained teardown list at the bottom of `main`.

## Why here, specifically

The split was done **after** the round trip passed and **before** the shader
work. That order matters: the code being restructured is known to work, so the
only correct test for the refactor is that the output does not change. Splitting
while also adding new behaviour would have meant debugging both at once.

## Layout

```
include/vk_buffer.hpp     what a caller needs to know
include/vk_context.hpp
src/vk_buffer.cpp         how it works
src/vk_context.cpp
src/main.cpp              orchestration + the round-trip test
```

A header contains declarations (signature plus `;`) and complete struct
definitions — the caller needs the struct's size, so it cannot be hidden. Bodies
go in the `.cpp`.

**A header includes only what the header itself needs.** `vk_buffer.hpp` includes
`<vulkan/vulkan.h>` because its declarations use `VkDevice`; `<iostream>` and
`<stdexcept>` are implementation details and stay in the `.cpp`. Every include in
a header is forced on every file that includes it.

## `Buffer`

    struct Buffer {
        VkBuffer       buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

`createBuffer` returns one of these instead of writing to two out-parameters, and
`destroyBuffer` destroys both halves and resets the handles. Six cleanup lines
became three; two variables per buffer became one.

The real payoff is **type safety**, and it was collected immediately:
`vkCmdCopyBuffer(..., stagingIn, ...)` no longer compiles, because `Buffer` is
not `VkBuffer`. With two loose variables that mix-up was silent.

## `VulkanContext`

Seven handles that are created once and always travel together: `instance`,
`physicalDevice`, `device`, `computeFamily`, `computeQueue`, `commandPool`,
`commandBuffer`.

Buffers are deliberately **not** members. They are work, not infrastructure, and
more of them arrive every stage.

### `init()` instead of a constructor

If a constructor throws, **the destructor is never called** — an object is not
considered to exist until its constructor completes. A failure in
`vkCreateCommandPool`, the last step, would leak the instance and the device.

With a defaulted constructor plus a separate `init()`, the object already exists
before `init()` runs. If `init()` throws, the destructor still runs during stack
unwinding and cleans up whatever was built.

### Deleted copies

    VulkanContext(const VulkanContext&)            = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

A copy would leave two objects holding the same handles, and both destructors
would run — a double `vkDestroyDevice` on the same handle. The accidental copy is
passing the context to a function by value instead of by reference. `= delete`
turns that into a compile error.

### `VulkanContext() = default;` is not optional

Declaring **any** constructor — including a *deleted* copy constructor —
suppresses the implicitly generated default constructor. Without that line
`VulkanContext ctx;` does not compile, and the error message talks about a
deleted constructor, which reads as though copying were the problem.

### Destructor

    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

No `VK_NULL_HANDLE` checks are needed: the spec says every `vkDestroy*` accepts
`VK_NULL_HANDLE` as the object handle and does nothing in that case. A half-built
context therefore tears down correctly — which is the reason every member has an
initialiser.

### Declaration order in `main`

`VulkanContext ctx;` is declared **before** the buffers, so it is destroyed
**last** — locals are destructed in reverse declaration order. The teardown order
is now correct by construction rather than by remembering to keep a list sorted.

## CMake

    add_executable(vpv
        src/main.cpp
        src/vk_buffer.cpp
        src/vk_context.cpp
    )
    target_include_directories(vpv PRIVATE include)
    target_compile_options(vpv PRIVATE -Wall -Wextra)

Only `.cpp` files are listed; headers need no entry.

`target_include_directories` was not optional. `#include "vk_buffer.hpp"` from
`src/vk_buffer.cpp` failed until `include/` was on the path:

| Form | Search order |
|---|---|
| `#include "x.hpp"` | the including file's own directory first, then the include path |
| `#include <x.hpp>` | the include path only (`-I` flags, system directories) |

The header was in `include/`, the including file in `src/`, and `include/` was on
neither list.

`PRIVATE` means the path applies to this target only; `PUBLIC` / `INTERFACE`
would propagate it to anything linking against it, which matters once there are
libraries.

Forgetting a `.cpp` in `add_executable` produces a **linker** error
(`undefined reference to createBuffer`), not a compile error. Worth recognising:
the cause is the build configuration, not the code.

## Three C++ mistakes worth remembering

1. **`struct` and `class` definitions end with `};`.** A bare `}` closes the
   struct early, and everything after it lands at namespace scope, producing
   errors that point at the wrong problem — `operator=` "must be a non-static
   member function", for instance.
2. **Member function bodies need the `VulkanContext::` prefix** in the `.cpp`.
   Without it you define an unrelated free function and get a linker error about
   the missing member.
3. **`= default`** for the default constructor once any other constructor is
   declared.

None of these are Vulkan. All three cost time in this stage.

## Verification

The only correct test for a refactor is unchanged behaviour: same output, byte
for byte, plus zero warnings once `-Wall -Wextra` was finally switched on.

`-Wall -Wextra` had been off for the whole project until now. It would have
caught two earlier bugs unaided — the `VkDeviceQueueCreateInfo` that was filled
but never linked into `VkDeviceCreateInfo`, and the `success` flag that was
computed but never printed.

## What was deliberately not done

Buffers and the fence are still destroyed by hand in `main`. The same RAII trick
would work on them, but wrapping every Vulkan object in its own class hides the
API this project exists to learn. Revisit only if the list becomes unwieldy.

## Next

Continues in [Stage 5 — First compute shader](05-first-compute-shader.md).
