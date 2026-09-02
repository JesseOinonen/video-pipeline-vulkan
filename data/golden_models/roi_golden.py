# Golden model for ROI module
# Input:  ../rgb565data.hex  (one uint16 hex word per line, raster order)
# Output: roi_golden.hex  (only valid ROI pixels, $readmemh compatible)
#         roi_golden.png  (visual reference)
# Matches roi.vhdl: pass-through pixels where x in [X_START, X_END] and y in [Y_START, Y_END]
import numpy as np
from PIL import Image

W, H = 1280, 720

ROI_X_START = 30
ROI_X_END   = 560
ROI_Y_START = 80
ROI_Y_END   = 650

raw = np.array([int(x, 16) for x in open("../rgb565data.hex")], dtype=np.uint16).reshape(H, W)

# Extract ROI region (inclusive bounds, matches roi.vhdl >= and <= comparisons)
roi = raw[ROI_Y_START : ROI_Y_END + 1, ROI_X_START : ROI_X_END + 1]

roi_h, roi_w = roi.shape  # 571 x 531

# Hex file: one 16-bit word per ROI pixel, raster order
# Use $readmemh("roi_golden.hex", mem) in testbench
with open("roi_golden.hex", "w") as f:
    for row in roi:
        for val in row:
            f.write(f"{val:04x}\n")

# PNG preview: full 1280x720 frame, black outside ROI (matches RTL tvalid=0 behaviour)
r5 = (raw >> 11) & 0x1F
g6 = (raw >>  5) & 0x3F
b5 =  raw        & 0x1F
r8 = ((r5 << 3) | (r5 >> 2)).astype(np.uint8)
g8 = ((g6 << 2) | (g6 >> 4)).astype(np.uint8)
b8 = ((b5 << 3) | (b5 >> 2)).astype(np.uint8)
frame = np.zeros((H, W, 3), dtype=np.uint8)
frame[ROI_Y_START : ROI_Y_END + 1, ROI_X_START : ROI_X_END + 1] = \
    np.stack([r8, g8, b8], axis=-1)[ROI_Y_START : ROI_Y_END + 1, ROI_X_START : ROI_X_END + 1]
Image.fromarray(frame).save("roi_golden.png")

print(f"ROI: x[{ROI_X_START}:{ROI_X_END}] y[{ROI_Y_START}:{ROI_Y_END}]  =>  {roi_w}x{roi_h} pixels")
print(f"Saved roi_golden.hex ({roi_w * roi_h} words) and roi_golden.png")
