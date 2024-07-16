# blurgtext

Text rendering library targeted at integration in game engines, making use of:

- [SheenBidi](https://github.com/Tehreer/SheenBidi) (or GNU fribidi on Linux)
- [Harfbuzz](https://github.com/harfbuzz/harfbuzz)
- [Freetype](https://freetype.org/)
- [libraqm](https://github.com/HOST-Oman/libraqm)

For ease of distribution, blurgtext statically links to these libraries on Windows.
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
