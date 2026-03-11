#!/bin/bash
# Convert any video (MOV, MKV, AVI, HEVC, ProRes, etc.) to a
# fractal_stream-compatible H264/MP4 file.
#
# Usage:
#   ./tools/convert_video.sh input.mov
#   ./tools/convert_video.sh input.mov output.mp4   (optional custom output name)

set -e

INPUT="$1"
if [ -z "$INPUT" ]; then
    echo "Usage: $0 <input_video> [output.mp4]"
    exit 1
fi

if [ ! -f "$INPUT" ]; then
    echo "Error: file not found: $INPUT"
    exit 1
fi

# Output name: same location, _converted.mp4 suffix if not specified
if [ -n "$2" ]; then
    OUTPUT="$2"
else
    BASENAME="${INPUT%.*}"
    OUTPUT="${BASENAME}_converted.mp4"
fi

echo "Converting: $INPUT → $OUTPUT"

ffmpeg -y \
    -i "$INPUT" \
    -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" \
    -c:v libx264 \
    -preset fast \
    -crf 18 \
    -pix_fmt yuv420p \
    -movflags +faststart \
    -an \
    "$OUTPUT"

echo ""
echo "Done: $OUTPUT"
echo "Open in fractal_stream with:"
echo "  ./build/fractal_stream \"$OUTPUT\""
