#!/bin/bash
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

get_abs_filename() {
  # $1 : relative filename
  echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
}

OSX_ARM64_DEST="build/nuget/osx-arm64"
OSX_X86_64_DEST="build/nuget/osx-x64"
cd "$SCRIPT_DIR"
mkdir -p "$OSX_ARM64_DEST"
mkdir -p "$OSX_X86_64_DEST"

DOWNLOADS_DIR=$(get_abs_filename build/nuget/sources)
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -G Ninja -DBT_BUILD_DEMO=OFF -DFT_DOWNLOADS_DIR=$DOWNLOADS_DIR"

cd "$OSX_ARM64_DEST"
cmake ../../.. $CMAKE_ARGS && ninja -v || { echo >&2 "Build failed for osx-arm64"; exit 1; }
