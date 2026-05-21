#!/usr/bin/env bash

# Bash strict mode
set -euo pipefail
IFS=$'\n\t'

sdkpath="${N64_INST:-$HOME/.local/n64}"
workpath="$HOME/.cache/pyrite64-tmp"

export N64_INST="$sdkpath"

mkdir -p "$workpath"
cd "$workpath"

echo "=== Pyrite64 macOS Toolchain Installer ==="
echo "Install path: $N64_INST"
echo ""

# --- Prerequisites via Homebrew ---
if ! command -v brew &>/dev/null; then
  echo "ERROR: Homebrew is not installed."
  echo "Please install it from https://brew.sh and re-run the installer."
  exit 1
fi

echo "Installing build prerequisites..."
brew install git make python3 libpng pkg-config --quiet

# --- N64 MIPS64 cross-compiler toolchain (built from source via vendored script) ---
# The libdragon project provides build-toolchain.sh with full macOS/Homebrew support.
# It installs its own Homebrew deps (gmp, mpfr, etc.) and builds GCC 14 + binutils + newlib.
echo "Installing Homebrew build dependencies for toolchain..."
brew install -q gmp mpfr libmpc gsed isl make python3 texinfo ninja

# TOOLCHAIN_BUILDER may be injected by Pyrite64 (absolute path, works from .app bundle).
# Otherwise resolve it relative to this script's location, trying two locations:
#   1. Resources/vendor/build-toolchain.sh  — when run from inside a .app bundle
#   2. <project-root>/vendored/libdragon/tools/build-toolchain.sh  — dev source tree
if [ -z "${TOOLCHAIN_BUILDER:-}" ]; then
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  TOOLCHAIN_BUILDER="$SCRIPT_DIR/../../vendor/build-toolchain.sh"
  if [ ! -f "$TOOLCHAIN_BUILDER" ]; then
    TOOLCHAIN_BUILDER="$SCRIPT_DIR/../../../vendored/libdragon/tools/build-toolchain.sh"
  fi
fi

if [ ! -f "$TOOLCHAIN_BUILDER" ]; then
  echo "ERROR: build-toolchain.sh not found at: $TOOLCHAIN_BUILDER"
  echo "Make sure the libdragon submodule is initialized:"
  echo "  git submodule update --init vendored/libdragon"
  exit 1
fi

if [[ ! -x "$N64_INST/bin/mips64-elf-gcc" || "${FORCE_UPDATE:-}" == "true" ]]; then
  echo "Building MIPS64 cross-compiler toolchain (~15-30 minutes, please wait)..."
  export BUILD_PATH="$workpath/toolchain"
  mkdir -p "$BUILD_PATH"
  bash "$TOOLCHAIN_BUILDER"
else
  echo "MIPS64 toolchain already installed, skipping build."
fi

if [ ! -x "$N64_INST/bin/mips64-elf-gcc" ]; then
  echo "ERROR: mips64-elf-gcc not found after toolchain build."
  exit 1
fi

# --- Libdragon ---
if [ -d "$workpath/libdragon" ]; then
  echo "Libdragon already cloned, updating..."
  cd "$workpath/libdragon"
  git checkout preview
  git pull
else
  echo "Cloning libdragon (preview branch)..."
  git clone -b preview https://github.com/DragonMinded/libdragon.git "$workpath/libdragon"
  cd "$workpath/libdragon"
fi

if [[ ! -f "$N64_INST/bin/n64tool" || "${FORCE_UPDATE:-}" == "true" ]]; then
  echo "Building libdragon..."
  make clean && make -C tools clean
  make -j$(sysctl -n hw.logicalcpu) libdragon && make -j$(sysctl -n hw.logicalcpu) tools
  make install
  make -C tools install
else
  echo "Libdragon already installed, skipping build."
fi

cd "$workpath"

# --- Tiny3D ---
if [ -d "$workpath/tiny3d" ]; then
  echo "Tiny3D already cloned, updating..."
  cd "$workpath/tiny3d"
  git pull
else
  echo "Cloning Tiny3D..."
  git clone https://github.com/HailToDodongo/tiny3d.git "$workpath/tiny3d"
  cd "$workpath/tiny3d"
fi

if [[ ! -f "$N64_INST/bin/gltf_to_t3d" || "${FORCE_UPDATE:-}" == "true" ]]; then
  echo "Building Tiny3D..."
  make clean
  make -j$(sysctl -n hw.logicalcpu)
  make install

  echo "Building Tiny3D tools..."
  make -C tools/gltf_importer -j$(sysctl -n hw.logicalcpu)
  make -C tools/gltf_importer install
else
  echo "Tiny3D already installed, skipping build."
fi

echo ""
echo "=== Installation complete! ==="
echo ""
echo "N64_INST is set to: $N64_INST"
echo "Add the following to your shell profile (~/.zshrc or ~/.bash_profile):"
echo "  export N64_INST=\"$N64_INST\""
echo ""
echo "You can now close this window and return to Pyrite64."
sleep 3
