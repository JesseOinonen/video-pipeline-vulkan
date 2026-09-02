#!/usr/bin/env python3
# Visual comparison tool: RTL output vs golden model
# Usage (run from tb/):
#   python3 compare.py gray
#   python3 compare.py roi
#   python3 compare.py gaussian
#   python3 compare.py sobel
#
# Expects RTL-produced hex file at {module}/rtl_out.hex
# (one 16-bit hex word per line, tvalid=1 pixels in raster order)

import sys
import numpy as np
from PIL import Image
import matplotlib.pyplot as plt

W, H = 1280, 720
ROI_X_START, ROI_X_END = 30, 560
ROI_Y_START, ROI_Y_END = 80, 650
ROI_W = ROI_X_END - ROI_X_START + 1  # 531
ROI_H = ROI_Y_END - ROI_Y_START + 1  # 571

def load_hex(path, expected=None):
    data = np.array([int(x, 16) for x in open(path)], dtype=np.uint16)
    if expected is not None and len(data) != expected:
        print(f"WARNING: {path} has {len(data)} pixels, expected {expected} — truncating/padding")
        if len(data) > expected:
            data = data[:expected]
        else:
            data = np.pad(data, (0, expected - len(data)))
    return data

def rgb565_to_rgb888(pixels2d):
    r8 = (((pixels2d >> 11) & 0x1F) << 3 | ((pixels2d >> 11) & 0x1F) >> 2).astype(np.uint8)
    g8 = (((pixels2d >>  5) & 0x3F) << 2 | ((pixels2d >>  5) & 0x3F) >> 4).astype(np.uint8)
    b8 = (( pixels2d        & 0x1F) << 3 | ( pixels2d        & 0x1F) >> 2).astype(np.uint8)
    return np.stack([r8, g8, b8], axis=-1)

def gray_image(module):
    rtl_raw  = load_hex(f"{module}/rtl_out.hex", W*H)
    gold_img = np.array(Image.open(f"{module}/{module}_golden.png"))

    rtl_img = (rtl_raw & 0xFF).astype(np.uint8).reshape(H, W)
    return rtl_img, gold_img, "gray"

def roi_image(module):
    rtl_raw  = load_hex(f"{module}/rtl_out.hex", ROI_W*ROI_H)   # ROI_W*ROI_H valid pixels
    gold_img = np.array(Image.open(f"{module}/{module}_golden.png"))  # 1280x720 RGB

    # Place ROI pixels back into full frame (black outside)
    rtl_roi = rgb565_to_rgb888(rtl_raw.reshape(ROI_H, ROI_W))
    rtl_frame = np.zeros((H, W, 3), dtype=np.uint8)
    rtl_frame[ROI_Y_START:ROI_Y_END+1, ROI_X_START:ROI_X_END+1] = rtl_roi
    return rtl_frame, gold_img, "rgb"

def gaussian_image(module):
    rtl_raw  = load_hex(f"{module}/rtl_out.hex", W*H)
    gold_img = np.array(Image.open(f"{module}/{module}_golden.png"))

    rtl_img = (rtl_raw & 0xFF).astype(np.uint8).reshape(H, W)
    return rtl_img, gold_img, "gray"

def sobel_image(module):
    rtl_raw  = load_hex(f"{module}/rtl_out.hex", W*H)
    gold_img = np.array(Image.open(f"{module}/{module}_golden.png"))

    rtl_img = (rtl_raw & 0xFF).astype(np.uint8).reshape(H, W)
    return rtl_img, gold_img, "gray"

handlers = {
    "gray":     gray_image,
    "roi":      roi_image,
    "gaussian": gaussian_image,
    "sobel":    sobel_image,
}

if len(sys.argv) != 2 or sys.argv[1] not in handlers:
    print(f"Usage: python3 compare.py [{'|'.join(handlers)}]")
    sys.exit(1)

module = sys.argv[1]
rtl_img, gold_img, cmap = handlers[module](module)

fig, axes = plt.subplots(1, 2, figsize=(18, 6))
fig.suptitle(f"{module.upper()} — RTL output vs Golden model", fontsize=14)

axes[0].imshow(rtl_img, cmap="gray" if cmap == "gray" else None)
axes[0].set_title("RTL output  (rtl_out.hex)")
axes[0].axis("off")

axes[1].imshow(gold_img, cmap="gray" if cmap == "gray" else None)
axes[1].set_title("Golden model")
axes[1].axis("off")

plt.tight_layout()
plt.show()
