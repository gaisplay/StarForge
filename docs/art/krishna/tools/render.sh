#!/usr/bin/env bash
# Regenerates the Krishna blueprint and blockout renders from tools/geometry.py.
# Run from anywhere in Git Bash:  bash docs/art/krishna/tools/render.sh
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
BLUEPRINT="$ROOT/blueprint"
BLOCKOUT="$ROOT/blockout"
mkdir -p "$BLUEPRINT" "$BLOCKOUT"

EDGE="/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"
BLENDER="/c/Program Files/Blender Foundation/Blender 5.2/blender.exe"

# 1. Blueprint: SVG from the geometry table, then a PNG through headless Edge (the SVG stays the source).
python "$HERE/threeview.py" "$BLUEPRINT"
win_svg="$(cygpath -m "$BLUEPRINT/krishna_threeview.svg")"
win_png="$(cygpath -w "$BLUEPRINT/krishna_threeview.png")"
"$EDGE" --headless=new --disable-gpu --hide-scrollbars --window-size=1560,1320 \
  --screenshot="$win_png" "file:///$win_svg" >/dev/null 2>&1 || true
ls -la "$BLUEPRINT"

# 2. Blockout: three Workbench renders and a .blend (the .blend is regenerable and not committed).
"$BLENDER" -b -P "$HERE/blockout.py" -- "$(cygpath -w "$BLOCKOUT")" 2>&1 | grep -E "rendered|saved|Error|Traceback" || true
ls -la "$BLOCKOUT"
