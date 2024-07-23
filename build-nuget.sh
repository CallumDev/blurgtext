#!/bin/bash
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -G Ninja -DBT_BUILD_DEMO=OFF"
mkdir -p build/nuget/win-x64
mkdir -p build/nuget/win-x86
mkdir -p build/nuget/linux-x64

cd build/nuget/win-x64
cmake ../../.. $CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=../../../toolchains/mingw-w64-x86_64.cmake && ninja
cd ../../..

cd build/nuget/win-x86
cmake ../../.. $CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=../../../toolchains/mingw-w64-i686.cmake && ninja
cd ../../..

cd build/nuget/linux-x64
cmake ../../.. $CMAKE_ARGS && ninja
cd ../../..
