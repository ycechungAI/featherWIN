#!/bin/bash
set -e
cd "$(dirname "$0")"
echo "Building featherWIN..."
cmake --build build --config Release
echo "Build complete."
