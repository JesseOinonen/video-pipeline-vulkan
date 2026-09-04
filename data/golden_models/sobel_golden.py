# Golden model for the Sobel operator (RTL verification)
#
# Input:  ../gray/gray_golden.npy  (produced by gray_golden.py)
# Output: sobel_golden.hex  - bit-exact expectation for the RTL output STREAM
#         sobel_golden.png  - the plain, un-shifted Sobel image (visual reference)
#         sobel_golden.npy  - same un-shifted image, for chaining another stage
#
# Arithmetic exactly as in sobel.vhdl:
#   Gx = (-p00 + p02) + 2*(-p10 + p12) + (-p20 + p22)
#   Gy = (-p00 - 2*p01 - p02) + (p20 + 2*p21 + p22)
#   G  = (|Gx| + |Gy|) >> 3        truncating shift, no rounding
# |Gx|, |Gy| <= 1020, so the sum is <= 2040 and G <= 255: the 8-bit result never
# wraps. The & 0xFF below mirrors the RTL's resize(..., 8) anyway.
import numpy as np
from PIL import Image

img = np.load("../gray/gray_golden.npy").astype(np.int64)
h, w = img.shape

# ---------------------------------------------------------------------------
# The filter itself - centre pixel at its own coordinates, no shifting
# ---------------------------------------------------------------------------
p00, p01, p02 = img[0:h-2, 0:w-2], img[0:h-2, 1:w-1], img[0:h-2, 2:w]
p10,      p12 = img[1:h-1, 0:w-2],                    img[1:h-1, 2:w]
p20, p21, p22 = img[2:h,   0:w-2], img[2:h,   1:w-1], img[2:h,   2:w]

gx = (-p00 + p02) + 2*(-p10 + p12) + (-p20 + p22)
gy = (-p00 - 2*p01 - p02) + (p20 + 2*p21 + p22)

sob = img.copy()                                   # border: gray pass-through
sob[1:h-1, 1:w-1] = ((np.abs(gx) + np.abs(gy)) >> 3) & 0xFF
sob = sob.astype(np.uint8)

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
#     out[y, x] = sob[y - LINE_DELAY, x - COL_DELAY]   inside the valid window
#     out[y, x] = gray[y, x]                        elsewhere (data_out pass-through)
# The valid window follows directly from
#   enable_s1 = (wptr > 1) and (wptr < W-2) and (y_cnt >= 2) and (y_cnt < H-2)
LINE_DELAY      = 1
COL_DELAY       = 1
Y_FIRST, Y_LAST = 2, h - 3   # stream rows carrying a filtered pixel (inclusive)
X_FIRST, X_LAST = 2, w - 3   # stream cols ...

stream = img.astype(np.uint8).copy()
stream[Y_FIRST:Y_LAST+1, X_FIRST:X_LAST+1] = \
    sob[Y_FIRST-LINE_DELAY:Y_LAST+1-LINE_DELAY,
        X_FIRST-COL_DELAY:X_LAST+1-COL_DELAY]

# Hex file: one 16-bit word per line (upper byte = 0x00, lower byte = pixel).
# This is what tvalid_out actually produces, in order -> $readmemh in the TB.
with open("sobel_golden.hex", "w") as f:
    for row in stream:
        for val in row:
            f.write(f"{val:04x}\n")

# The image files are the un-shifted result, so they can be looked at directly.
np.save("sobel_golden.npy", sob)
Image.fromarray(sob).save("sobel_golden.png")
print(f"Saved sobel_golden.hex (stream, {LINE_DELAY} line + {COL_DELAY} px late) and "
      f"sobel_golden.png/.npy (un-shifted)  ({w}x{h}, {h*w} words)")
print(f"Filtered stream window: y {Y_FIRST}..{Y_LAST}, x {X_FIRST}..{X_LAST}; "
      f"everything else is gray pass-through")
