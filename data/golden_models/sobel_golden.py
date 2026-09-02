# Golden model for Sobel operator for RTL verification
# Input must be gray_golden.npy produced by gray_golden.py
# Output: sobel_golden.hex  (one 16-bit hex word per line, $readmemh compatible)
#         sobel_golden.png  (visual reference)
import numpy as np
from PIL import Image

img = np.load("../gray/gray_golden.npy").astype(np.int16)

h, w = img.shape

# Border pixels are passed through from gray input (matches RTL data_out pass-through)
out = img.clip(0, 255).astype(np.uint8)

# RTL enable_s1 fires at y_cnt in [2, H-3] and wptr in [3, W-3].
# Due to BRAM row latency + pipeline alignment, the Sobel result for center (y-1, x-2)
# is output at stream position (y, x). Loop bounds match the RTL enable condition exactly.
for y in range(2, h-2):
    for x in range(3, w-2):
        cy, cx = y - 1, x - 2  # RTL Sobel center

        p00 = img[cy-1, cx-1]
        p01 = img[cy-1, cx]
        p02 = img[cy-1, cx+1]

        p10 = img[cy, cx-1]
        p11 = img[cy, cx]
        p12 = img[cy, cx+1]

        p20 = img[cy+1, cx-1]
        p21 = img[cy+1, cx]
        p22 = img[cy+1, cx+1]

        gx = (-p00 + p02) \
           + 2*(-p10 + p12) \
           + (-p20 + p22)

        gy = (-p00 - 2*p01 - p02) \
           + ( p20 + 2*p21 + p22)

        g = (abs(gx) + abs(gy)) >> 3

        if g > 255:
            g = 255

        out[y, x] = g  # write Sobel result at RTL output position (y, x)

# Hex file: one 16-bit word per line (upper byte = 0x00, lower byte = pixel)
# Use $readmemh("sobel_golden.hex", mem) in testbench
with open("sobel_golden.hex", "w") as f:
    for row in out:
        for val in row:
            f.write(f"{val:04x}\n")

Image.fromarray(out).save("sobel_golden.png")
print(f"Saved sobel_golden.hex and sobel_golden.png  ({w}x{h}, {h*w} words)")