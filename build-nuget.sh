#!/bin/bash
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
DOTNET_VERSION=8.0
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -G Ninja -DBT_BUILD_DEMO=OFF"

check_command() {
    command -v $1 >/dev/null 2>&1 || { echo >&2 "Cannot find $1 on PATH"; exit 1; }
}

check_command dotnet
check_command cmake
check_command ninja

ere_quote() {
    sed 's/[][\.|$(){}?+*^]/\\&/g' <<< "$*"
}
DOTNET_GREP="^`ere_quote $DOTNET_VERSION`"
dotnet --list-sdks | grep -E $DOTNET_GREP > /dev/null

if [ $? -ne 0 ]; then
    echo SDK Version $DOTNET_VERSION was not found
    exit 2
fi

cd "$SCRIPT_DIR"

mkdir -p build/nuget/win-x64
mkdir -p build/nuget/win-x86
mkdir -p build/nuget/win-arm64
mkdir -p build/nuget/linux-x64

cd build/nuget/win-x64
cmake ../../.. $CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=../../../toolchains/mingw-w64-x86_64.cmake && ninja || { echo >&2 "Build failed"; exit 1; }
cd ../../..

cd build/nuget/win-x86
cmake ../../.. $CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=../../../toolchains/mingw-w64-i686.cmake && ninja || { echo >&2 "Build failed"; exit 1; }
cd ../../..

cd build/nuget/linux-x64
cmake ../../.. $CMAKE_ARGS && ninja || { echo >&2 "Build failed"; exit 1; }
cd ../../..

cd dotnet/BlurgText
dotnet build -p:VersionSuffix=$PACKAGESUFFIX -c Release || { echo >&2 "Build failed"; exit 1; }
dotnet pack -p:VersionSuffix=$PACKAGESUFFIX -c Release || { echo >&2 "Build failed"; exit 1; }
cd ../..
