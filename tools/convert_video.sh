#!/bin/bash
# Convert any video (MOV, MKV, AVI, HEVC, ProRes, etc.) to a
# fractal_stream-compatible H264/MP4 file.
#
# Single file:
#   ./tools/convert_video.sh input.mov
#   ./tools/convert_video.sh input.mov output.mp4
#
# Whole folder (converts every .mov in-place, outputs alongside originals):
#   ./tools/convert_video.sh /path/to/folder

set -e

convert_one() {
    local INPUT="$1"
    local OUTPUT="$2"
    echo "  Converting: $INPUT → $OUTPUT"
    ffmpeg -y -loglevel error \
        -i "$INPUT" \
        -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" \
        -c:v libx264 \
        -preset fast \
        -crf 18 \
        -pix_fmt yuv420p \
        -movflags +faststart \
        -an \
        "$OUTPUT"
    echo "  Done: $OUTPUT"
}

ARG="$1"

if [ -z "$ARG" ]; then
    echo "Usage:"
    echo "  $0 <input_video> [output.mp4]   # single file"
    echo "  $0 <folder>                      # all .mov files in folder"
    exit 1
fi

# --- Folder mode ---
if [ -d "$ARG" ]; then
    FOLDER="$ARG"
    shopt -s nullglob
    FILES=("$FOLDER"/*.{mov,MOV,mp4,MP4,mkv,MKV,avi,AVI,m4v,M4V})
    if [ ${#FILES[@]} -eq 0 ]; then
        echo "No video files found in: $FOLDER"
        exit 1
    fi
    echo "Found ${#FILES[@]} file(s) in $FOLDER"
    DONE=0
    SKIP=0
    for INPUT in "${FILES[@]}"; do
        BASENAME="${INPUT%.*}"
        OUTPUT="${BASENAME}_converted.mp4"
        # Skip if already a converted mp4 or output already exists
        if [[ "$INPUT" == *_converted.mp4 ]]; then
            ((SKIP++)) || true
            continue
        fi
        if [ -f "$OUTPUT" ]; then
            echo "  Skipping (already exists): $OUTPUT"
            ((SKIP++)) || true
            continue
        fi
        convert_one "$INPUT" "$OUTPUT"
        ((DONE++)) || true
    done
    echo ""
    echo "Converted: $DONE  Skipped: $SKIP"

# --- Single file mode ---
elif [ -f "$ARG" ]; then
    INPUT="$ARG"
    if [ -n "$2" ]; then
        OUTPUT="$2"
    else
        BASENAME="${INPUT%.*}"
        OUTPUT="${BASENAME}_converted.mp4"
    fi
    convert_one "$INPUT" "$OUTPUT"
    echo ""
    echo "Open in fractal_stream with:"
    echo "  ./build/fractal_stream \"$OUTPUT\""

else
    echo "Error: not a file or directory: $ARG"
    exit 1
fi
