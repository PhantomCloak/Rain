#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/linux-debug}"
REPORT_DIR="cc-report"

# Use native CodeChecker if python <= 3.12, otherwise use pipx with python3.12
PY_COMPAT=$(python3 -c 'import sys; print(sys.version_info[:2] <= (3,12))' 2>/dev/null || echo "False")
if [ "$PY_COMPAT" = "True" ]; then
  if ! command -v CodeChecker &>/dev/null; then
    read -rp "CodeChecker not found. Install via pip? [y/N] " ans
    if [[ "$ans" =~ ^[Yy]$ ]]; then
      pip install codechecker
    else
      echo "Aborting."
      exit 1
    fi
  fi
  CC() { CodeChecker "$@"; }
else
  if ! command -v pipx &>/dev/null; then
    read -rp "pipx not found. Install via pip? [y/N] " ans
    if [[ "$ans" =~ ^[Yy]$ ]]; then
      pip install pipx
    else
      echo "Aborting."
      exit 1
    fi
  fi
  CC() { pipx run --python python3.12 CodeChecker "$@"; }
fi
RUN_NAME="WebEngine"
SERVER_PORT=8001
SERVER_WORKSPACE="$HOME/.codechecker"
SERVER_URL="http://localhost:${SERVER_PORT}/Default"

if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
  echo "Error: $BUILD_DIR/compile_commands.json not found. Run CMake first."
  exit 1
fi

CC analyze \
  "$BUILD_DIR/compile_commands.json" \
  --output "$REPORT_DIR" \
  --skip .codechecker-skip \
  --clean

CC parse "$REPORT_DIR" || true

if ! curl -s -o /dev/null http://localhost:${SERVER_PORT} 2>/dev/null; then
  echo "starting CodeChecker server on port ${SERVER_PORT}..."
  CC server \
    --workspace "$SERVER_WORKSPACE" \
    --port "$SERVER_PORT" &
  sleep 2
fi

CC store "$REPORT_DIR" \
  --name "$RUN_NAME" \
  --url "$SERVER_URL"

echo "serving reports at ${SERVER_URL}"
