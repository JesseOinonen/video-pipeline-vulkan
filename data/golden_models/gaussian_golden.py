# Golden model for the separable 3x3 Gaussian blur (RTL verification)
#
# Input:  ../gray/gray_golden.npy  (produced by gray_golden.py)
# Output: gaussian_golden.hex  - bit-exact expectation for the RTL output STREAM
#         gaussian_golden.png  - the plain, un-shifted blurred image
#         gaussian_golden.npy  - same un-shifted image, for chaining another stage
#
# Integer arithmetic exactly as in gaussian.vhdl:
#   Horizontal:  H = (a + 2b + c) >> 2      (a,b,c = left, centre, right pixel)
#   Vertical:    Y = (Ht + 2Hm + Hb) >> 2   (H of row above, row, row below)
# which is the separable form of 1/16 * [1 2 1; 2 4 2; 1 2 1].
# Both shifts truncate (no rounding) and both stages stay inside 8 bits:
# (255 + 2*255 + 255) >> 2 = 255.
import numpy as np
from PIL import Image

img = np.load("../gray/gray_golden.npy").astype(np.uint32)
h, w = img.shape

ROUND = 0               # 0 = truncate (shift only). Set to 2 for round-half-up
                        # if the RTL ever adds a +2 constant before each >> 2.

# ---------------------------------------------------------------------------
# The filter itself - centre pixel at its own coordinates, no shifting
# ---------------------------------------------------------------------------
hor = np.zeros((h, w), dtype=np.uint32)     # columns 0 / w-1 have no neighbour
hor[:, 1:w-1] = (img[:, 0:w-2] + (img[:, 1:w-1] << 1) + img[:, 2:w] + ROUND) >> 2

gau = img.astype(np.uint8).copy()           # border: gray pass-through
gau[1:h-1, 1:w-1] = (((hor[0:h-2, 1:w-1] + (hor[1:h-1, 1:w-1] << 1)
                       + hor[2:h, 1:w-1] + ROUND) >> 2)).astype(np.uint8)

# ---------------------------------------------------------------------------
# RTL output alignment - verified bit-exact against xsim
# ---------------------------------------------------------------------------
# The output stream lags the frame it belongs to by exactly one line and one
# column, and both parts are deliberate:
#   LINE_DELAY - inherent to a 3-line buffer, the centre row can only be
#                filtered once the row below it has arrived.
#   COL_DELAY  - the sideband chain (tvalid/tlast/...) is one register deeper
#                than the datapath, so the AXIS position tag arrives at
#                data_out one beat ahead of the window centre. It could be
#                removed with one more sideband stage; it is kept so that the
#                offset stays symmetric (1,1) and is corrected once, further
#                down the pipeline (crop / resize / DMA start address).
# As a stream:
#     out[y, x] = gau[y - LINE_DELAY, x - COL_DELAY]   inside the valid window
#     out[y, x] = gray[y, x]                        elsewhere (data_out pass-through)
# The valid window follows directly from
#   enable_s1 = (wptr > 1) and (wptr < W-2) and (y_cnt >= 2) and (y_cnt < H-2)
LINE_DELAY      = 1
COL_DELAY       = 1
Y_FIRST, Y_LAST = 2, h - 3   # stream rows carrying a filtered pixel (inclusive)
X_FIRST, X_LAST = 2, w - 3   # stream cols ...

stream = img.astype(np.uint8).copy()
stream[Y_FIRST:Y_LAST+1, X_FIRST:X_LAST+1] = \
    gau[Y_FIRST-LINE_DELAY:Y_LAST+1-LINE_DELAY,
        X_FIRST-COL_DELAY:X_LAST+1-COL_DELAY]

# Hex file: one 16-bit word per line (upper byte = 0x00, lower byte = pixel).
# This is what tvalid_out actually produces, in order -> $readmemh in the TB.
with open("gaussian_golden.hex", "w") as f:
    for row in stream:
        for val in row:
            f.write(f"{val:04x}\n")

# The image files are the un-shifted result, so a later stage (e.g. sobel after
# gaussian) can be modelled on clean data instead of on a shifted frame.
np.save("gaussian_golden.npy", gau)
Image.fromarray(gau).save("gaussian_golden.png")

# Sanity check: how much the two truncating >> 2 stages deviate from an exact
# 1/16 kernel. Informational only - the RTL must match `gau`, not this.
exact = (img[0:h-2, 0:w-2] + 2*img[0:h-2, 1:w-1] +   img[0:h-2, 2:w] +
       2*img[1:h-1, 0:w-2] + 4*img[1:h-1, 1:w-1] + 2*img[1:h-1, 2:w] +
         img[2:h,   0:w-2] + 2*img[2:h,   1:w-1] +   img[2:h,   2:w]) / 16.0
err = np.abs(gau[1:h-1, 1:w-1].astype(np.float64) - exact)
print(f"Truncation vs exact 1/16 kernel: max {err.max():.3f} LSB, mean {err.mean():.3f} LSB")
print(f"Saved gaussian_golden.hex (stream, {LINE_DELAY} line + {COL_DELAY} px late) and "
      f"gaussian_golden.png/.npy (un-shifted)  ({w}x{h}, {h*w} words)")
print(f"Filtered stream window: y {Y_FIRST}..{Y_LAST}, x {X_FIRST}..{X_LAST}; "
      f"everything else is gray pass-through")
