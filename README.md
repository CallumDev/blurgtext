![blurgtext](dotnet/BlurgText/icon.png)

Text rendering library targeted at integration in game engines, making use of:

- [Freetype](https://freetype.org/)
- [Harfbuzz](https://github.com/harfbuzz/harfbuzz)
- [libraqm](https://github.com/HOST-Oman/libraqm)
- [SheenBidi](https://github.com/Tehreer/SheenBidi)
- [libunibreak](https://github.com/adah1972/libunibreak)
- [plutosvg](https://github.com/sammycage/plutosvg)

For ease of distribution, blurgtext statically links to the Freetype and Harfbuzz libraries on macOS or non-unix platforms, or when the `BT_BUILD_FTHB` option is set to `ON`. The provided freetype is patched to use stb_image for PNG loading instead of libpng.

SheenBidi, libraqm, libunibreak and plutosvg are always built and statically linked into the library.

The minimum CMake required version is **3.15**

Software using this library must include the [Harfbuzz license](https://raw.githubusercontent.com/harfbuzz/harfbuzz/main/COPYING), [SheenBidi license](https://github.com/Tehreer/SheenBidi?tab=readme-ov-file#license), [libraqm license](https://github.com/HOST-Oman/libraqm/blob/master/COPYING) and [blurgtext license](LICENSE) as well as the Freetype credit notice:

```
    Portions of this software are copyright © 2024 The FreeType
    Project (www.freetype.org).  All rights reserved.
```


For the following builds, set -DCMAKE_BUILD_TYPE=Release on the cmake command line to create a release build.

## Compiling (Linux)

```
mkdir -p build/linux
cd build/linux
cmake ../..
make
```

## Compiling (Win64 mingw)

```
mkdir -p build/windows
cd build/windows
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchains/mingw-w64-x86_64.cmake
make
```
