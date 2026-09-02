#!/usr/bin/env bash
# Copy the input image and the golden outputs from the RTL project.
# The RTL repo is the single source of truth for both; the .hex files here are
# a local copy and are not committed (see .gitignore).
#
#   usage: scripts/sync-test-data.sh [path-to-video-pipeline-repo]

set -euo pipefail

RTL="${1:-$(dirname "$0")/../../video-pipeline}"
TB="$RTL/tb"

if [ ! -d "$TB" ]; then
    echo "RTL testbench directory not found: $TB" >&2
    echo "Pass the path to the video-pipeline repo as the first argument." >&2
    exit 1
fi

DEST="$(dirname "$0")/../data"
mkdir -p "$DEST/golden_models"

cp "$TB/rgb565data.hex"                "$DEST/"
cp "$TB/gray/gray_golden.hex"          "$DEST/"
cp "$TB/roi/roi_golden.hex"            "$DEST/"
cp "$TB/sobel/sobel_golden.hex"        "$DEST/"
cp "$TB/gaussian/gaussian_golden.hex"  "$DEST/"

cp "$TB"/*/*_golden.py "$TB/compare.py" "$DEST/golden_models/"

echo "Synced test data from $TB"
ls -la "$DEST"
