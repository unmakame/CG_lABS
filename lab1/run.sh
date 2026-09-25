#!/bin/zsh
set -e
LAB_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$HOME/VulkanSDK/1.4.321.0"
if [[ ! -f "$SDK_DIR/setup-env.sh" ]]; then
  echo "Vulkan SDK not found at $SDK_DIR" >&2
  exit 1
fi
source "$SDK_DIR/setup-env.sh" >/dev/null
cmake -S "$LAB_DIR" -B "$LAB_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$LAB_DIR/build" -j4
exec "$LAB_DIR/build/cg_lab1"
