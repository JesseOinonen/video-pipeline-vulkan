# Golden model for grayscaling module
# Input:  rgb565data.hex  (one uint16 hex word per line, raster order)
# Output: gray_golden.npy / .png / .hex  +  rgb565_preview.png
# Matches gray.vhdl: MSB-replication expand + 77/150/29 weighted average >> 8
import numpy as np
from PIL import Image

W, H = 1280, 720

raw = np.array([int(x, 16) for x in open("../rgb565data.hex")], dtype=np.uint16).reshape(H, W)

# Extract RGB565 channels
r5 = (raw >> 11) & 0x1F   # 5 bits
g6 = (raw >>  5) & 0x3F   # 6 bits
b5 =  raw        & 0x1F   # 5 bits

# Expand to 8 bits by replicating MSBs (matches gray.vhdl signal assignments)
# r <= r5[4:0] & r5[4:2]  =>  (r5 << 3) | (r5 >> 2)
# g <= g6[5:0] & g6[5:4]  =>  (g6 << 2) | (g6 >> 4)
# b <= b5[4:0] & b5[4:2]  =>  (b5 << 3) | (b5 >> 2)
r8 = (r5 << 3) | (r5 >> 2)
g8 = (g6 << 2) | (g6 >> 4)
b8 = (b5 << 3) | (b5 >> 2)

# Save RGB565 preview so the conversion can be visually verified
rgb_preview = np.stack([r8, g8, b8], axis=-1).astype(np.uint8)
Image.fromarray(rgb_preview).save("../rgb565_preview.png")
print(f"Saved rgb565_preview.png  ({W}x{H})")

# gray = (77*R + 150*G + 29*B) >> 8  (matches gray.vhdl process)
gray = ((77 * r8.astype(np.uint32) + 150 * g8.astype(np.uint32) + 29 * b8.astype(np.uint32)) >> 8).astype(np.uint8)

np.save("gray_golden.npy", gray)
Image.fromarray(gray).save("gray_golden.png")

# Hex file: one 16-bit word per line (upper byte = 0x00, lower byte = pixel)
# Use $readmemh("gray_golden.hex", mem) in testbench
with open("gray_golden.hex", "w") as f:
    for row in gray:
        for val in row:
            f.write(f"{val:04x}\n")

print(f"Saved gray_golden.npy/.png/.hex  ({W}x{H})")
