#!/bin/bash
check_command() {
    command -v $1 >/dev/null 2>&1 || { echo >&2 "Cannot find $1 on PATH"; exit 1; }
}

get_abs_filename() {
  # $1 : relative filename
  echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
}

ere_quote() {
    sed 's/[][\.|$(){}?+*^]/\\&/g' <<< "$*"
}

SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
DOTNET_VERSION=8.0

check_command dotnet
check_command cmake
check_command ninja

DOTNET_GREP="^`ere_quote $DOTNET_VERSION`"
dotnet --list-sdks | grep -E $DOTNET_GREP > /dev/null

if [ $? -ne 0 ]; then
    echo SDK Version $DOTNET_VERSION was not found
    exit 2
fi

cd "$SCRIPT_DIR"
mkdir -p build/nuget/sources
DOWNLOADS_DIR=$(get_abs_filename build/nuget/sources)
ARTIFACTS_DIR=$(get_abs_filename build/nuget)
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -G Ninja -DBT_BUILD_DEMO=OFF -DFT_DOWNLOADS_DIR=$DOWNLOADS_DIR"

check_linux() {
  OS=`uname -s`
  ARCH=`uname -m`
  if [ "$OS" != "Linux" ]; then
    echo "Unsupported host: $OS. Linux $1 cross-compile not supported"
    exit 1
  fi
  if [ "$ARCH" != "$1" ]; then
    echo "Unsupported host: $ARCH. Linux $1 cross-compile not supported."
    exit 1
  fi
}

check_mac() {
  OS=`uname -s`
  ARCH=`uname -m`
  if [ "$OS" != "Darwin" ]; then
    echo "Unsupported host: $OS. Mac $1 cross-compile not supported"
    exit 1
  fi
  if [ "$ARCH" != "$1" ]; then
    echo "Unsupported host: $ARCH. Mac $1 cross-compile not supported."
    exit 1
  fi
}

pack_native_nuget() {
  dotnet pack -p:VersionSuffix=$PACKAGESUFFIX -p:UseArtifactsOutput=true "-p:ArtifactsPath=$ARTIFACTS_DIR" "./dotnet/BlurgText.Native.${1}/BlurgText.Native.${1}.csproj"
}

win_x86() {
    mkdir -p build/nuget/win-x86
    cd build/nuget/win-x86
    cmake ../../.. $CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=../../../toolchains/mingw-w64-i686.cmake && ninja -v || { echo >&2 "Build failed"; exit 1; }
    cd ../../..
    pack_native_nuget win-x86 || { echo >&2 "Build failed"; exit 1; }
}

win_x64() {
    mkdir -p build/nuget/win-x64
    cd build/nuget/win-x64
    cmake ../../.. $CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=../../../toolchains/mingw-w64-x86_64.cmake && ninja -v || { echo >&2 "Build failed"; exit 1; }
    cd ../../..
    pack_native_nuget win-x64 || { echo >&2 "Build failed"; exit 1; }
}

linux_x64() {
    check_linux x86_64
    mkdir -p build/nuget/linux-x64
    cd build/nuget/linux-x64
    cmake ../../.. $CMAKE_ARGS && ninja -v || { echo >&2 "Build failed"; exit 1; }
    cd ../../..
    pack_native_nuget linux-x64 || { echo >&2 "Build failed"; exit 1; }
}

osx_x64() {
    check_mac x86_64
    mkdir -p build/nuget/osx-x64
    cd build/nuget/osx-x64
    cmake ../../.. $CMAKE_ARGS && ninja -v || { echo >&2 "Build failed"; exit 1; }
    cd ../../..
    pack_native_nuget osx-x64 || { echo >&2 "Build failed"; exit 1; }
}

osx_arm64() {
    check_mac arm64
    mkdir -p build/nuget/osx-arm64
    cd build/nuget/osx-arm64
    cmake ../../.. $CMAKE_ARGS && ninja -v || { echo >&2 "Build failed"; exit 1; }
    cd ../../..
    pack_native_nuget osx-arm64 || { echo >&2 "Build failed"; exit 1; }
}

managed() {
  cd dotnet/BlurgText
  dotnet build -p:NugetBuild=True -p:UseArtifactsOutput=true "-p:ArtifactsPath=$ARTIFACTS_DIR" -p:VersionSuffix=$PACKAGESUFFIX -c Release || { echo >&2 "Build failed"; exit 1; }
  dotnet pack -p:NugetBuild=True -p:UseArtifactsOutput=true "-p:ArtifactsPath=$ARTIFACTS_DIR" -p:VersionSuffix=$PACKAGESUFFIX -c Release || { echo >&2 "Build failed"; exit 1; }
  cd ../..
}

KNOWN_TARGETS=(win_x86 win_x64 linux_x64 managed)

is_known_target() {
  local v="$1"
  for k in "${KNOWN_TARGETS[@]}"; do
    [[ "$k" == "$v" ]] && return 0
  done
  return 1
}

if [[ "$#" -eq 0 ]]; then
  echo "Usage: $0 [target1] [target2] [target3]"
  echo "Targets: ${KNOWN_TARGETS[*]}"
  exit 1
fi

for target in "$@"; do
  if ! is_known_target "$target"; then
    echo "Error: unknown target '$target'"
    exit 1
  fi
done

for target in "$@"; do
  "$target"
done
