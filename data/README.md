# Test data

Input image and golden outputs, copied from the RTL project
(`video-pipeline/tb/`), which is the single source of truth for both.

Refresh with:

    scripts/sync-test-data.sh [path-to-video-pipeline-repo]

The `.hex` files are **not committed** — 20 MB of duplicated data that could
silently drift from the RTL repo. `golden_models/` *is* committed: those Python
scripts are the exact specification of each filter's arithmetic, and the shaders
must reproduce them bit for bit.

## Format

One 16-bit hex word per line, raster order, no header.

| File | Size | Contents |
|---|---|---|
| `rgb565data.hex` | 1280x720 | input, RGB565 |
| `gray_golden.hex` | 1280x720 | grayscale, `0x00XX` (8-bit value in the low byte) |
| `roi_golden.hex` | 531x571 | cropped region, RGB565 |
| `sobel_golden.hex` | 1280x720 | edge magnitude |
| `gaussian_golden.hex` | 1280x720 | blurred |

## Grayscale reference

From `golden_models/gray_golden.py`, matching `rtl/gray.vhdl`. Integer only —
using floats in the shader produces scattered off-by-one mismatches:

    r8 = (r5 << 3) | (r5 >> 2)      // MSB replication, not r5 * 255 / 31
    g8 = (g6 << 2) | (g6 >> 4)
    b8 = (b5 << 3) | (b5 >> 2)
    gray = (77*r8 + 150*g8 + 29*b8) >> 8

77 + 150 + 29 = 256 exactly, so the result is always <= 255 and needs no clamp.
