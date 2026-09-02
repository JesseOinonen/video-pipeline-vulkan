# Golden model for separable 3x3 Gaussian blur module
# Input must be gray_golden.npy produced by gray_golden.py
# Output: gaussian_golden.hex  (one 16-bit hex word per line, $readmemh compatible)
#         gaussian_golden.png  (visual reference)
#         gaussian_golden.npy  (input for a later stage, e.g. sobel after gaussian)
#
# Separable 3x3 Gaussian, integer arithmetic exactly as in hardware:
#   Horizontal:  H = (a + 2b + c) >> 2      (a,b,c = left, center, right pixel)
#   Vertical:    Y = (Ht + 2Hm + Hb) >> 2   (Ht,Hm,Hb = H of row above, row, row below)
# which is the separable form of
#   1/16 * [1 2 1; 2 4 2; 1 2 1]
# Multiply by 2 is a shift left, divide by 4 is shift_right(x,2) => truncation, no rounding.
# Both stages stay inside 8 bits: (255 + 2*255 + 255) >> 2 = 255.
import numpy as np
from PIL import Image

img = np.load("../gray/gray_golden.npy").astype(np.uint32)

h, w = img.shape

# ---------------------------------------------------------------------------
# Pipeline alignment - MUST match the RTL
# ---------------------------------------------------------------------------
# Same convention as sobel_golden.py: because of the BRAM row latency and the
# pipeline registers, the filter result for window center (y+DY, x+DX) leaves
# the module at stream position (y, x), and tvalid_out is only asserted for the
# window given by X_FIRST..X_LAST / Y_FIRST..Y_LAST.
#
# Defaults below are the sobel line-buffer/pipeline scheme:
#   enable = (wptr > 2) and (wptr < W-2) and (y_cnt >= 2) and (y_cnt < H-2)
#   window center = (y-1, x-2)
# If the Gaussian pipeline has a different depth or a different rptr offset,
# change these five constants - nothing else in this file depends on them.
DY, DX  = -1, -2        # window center relative to the current stream position
X_FIRST = 3             # first x with tvalid_out carrying a filtered pixel
X_LAST  = w - 3         # last  x  ... (inclusive)
Y_FIRST = 2             # first y ...
Y_LAST  = h - 3         # last  y  ... (inclusive)

ROUND = 0               # 0 = truncate (shift only). Set to 2 for round-half-up
                        # if the RTL adds a +2 constant before each >> 2.

# ---------------------------------------------------------------------------
# Stage A: horizontal 1D filter  H = (a + 2b + c) >> 2
# ---------------------------------------------------------------------------
# Columns 0 and w-1 have no left/right neighbour, so H is undefined there.
hor = np.zeros((h, w), dtype=np.uint32)
hor[:, 1:w-1] = (img[:, 0:w-2] + (img[:, 1:w-1] << 1) + img[:, 2:w] + ROUND) >> 2

# ---------------------------------------------------------------------------
# Stage B: vertical 1D filter  Y = (Ht + 2Hm + Hb) >> 2
# ---------------------------------------------------------------------------
# Rows 0 and h-1 have no row above/below, so Y is undefined there.
ver = np.zeros((h, w), dtype=np.uint32)
ver[1:h-1, :] = (hor[0:h-2, :] + (hor[1:h-1, :] << 1) + hor[2:h, :] + ROUND) >> 2

# ---------------------------------------------------------------------------
# Output frame: border pixels are passed through from the gray input
# (matches RTL data_out pass-through when the 3x3 window is not enabled)
# ---------------------------------------------------------------------------
out = img.astype(np.uint8)

# Write the blurred pixel at the RTL output position (y, x)
out[Y_FIRST:Y_LAST+1, X_FIRST:X_LAST+1] = \
    ver[Y_FIRST+DY:Y_LAST+1+DY, X_FIRST+DX:X_LAST+1+DX].astype(np.uint8)

# Hex file: one 16-bit word per line (upper byte = 0x00, lower byte = pixel)
# Use $readmemh("gaussian_golden.hex", mem) in testbench
with open("gaussian_golden.hex", "w") as f:
    for row in out:
        for val in row:
            f.write(f"{val:04x}\n")

np.save("gaussian_golden.npy", out)
Image.fromarray(out).save("gaussian_golden.png")

# Sanity check: how much the two truncating >> 2 stages deviate from an exact
# 1/16 kernel. Informational only - the RTL must match `out`, not this.
exact = (img[0:h-2, 0:w-2] +   2*img[0:h-2, 1:w-1] +   img[0:h-2, 2:w] +
       2*img[1:h-1, 0:w-2] +   4*img[1:h-1, 1:w-1] + 2*img[1:h-1, 2:w] +
         img[2:h,   0:w-2] +   2*img[2:h,   1:w-1] +   img[2:h,   2:w]) / 16.0
err = np.abs(ver[1:h-1, 1:w-1].astype(np.float64) - exact)
print(f"Truncation vs exact 1/16 kernel: max {err.max():.3f} LSB, mean {err.mean():.3f} LSB")
print(f"Saved gaussian_golden.hex/.png/.npy  ({w}x{h}, {h*w} words)")
