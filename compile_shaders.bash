#!/usr/bin/env bash

# Check for required compiler
if ! command -v glslc &> /dev/null; then
    echo "Error: glslc not found in PATH."
    exit 1
fi

# Set source directory or use current directory
SOURCE_DIR="${1:-.}"

# Find all .frag, .comp, .vert files excluding 'build' directories
find "$SOURCE_DIR" \
    \( -path "*/build/*" -prune \) -o \
    \( -type f \( -name "*.frag" -o -name "*.comp" -o -name "*.vert" \) -print \) |
while read -r shader; do
    dir="$(dirname "$shader")"
    base="$(basename "$shader")"
    output_dir="$dir/bin"
    output_file="$output_dir/$base.spv"

    # Create output bin directory if needed
    mkdir -p "$output_dir"

    echo "Compiling $shader -> $output_file"
    if ! glslc "$shader" -o "$output_file"; then
        echo "❌ Failed to compile: $shader" >&2
    fi
done

echo "✅ Shader compilation finished!"
