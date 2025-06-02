cmake_minimum_required(VERSION 3.15)
project(ft-hb-bundle)

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
 cmake_policy(SET CMP0135 NEW)
endif()

option(FT_DOWNLOADS_DIR "Folder to contain downloaded source files" "${CMAKE_CURRENT_BINARY_DIR}/sources")

SET(FREETYPE_URL "https://download.savannah.gnu.org/releases/freetype/freetype-2.13.2.tar.xz" "https://downloads.sourceforge.net/project/freetype/freetype2/2.13.2/freetype-2.13.2.tar.xz")
SET(FREETYPE_HASH "SHA256=12991c4e55c506dd7f9b765933e62fd2be2e06d421505d7950a132e4f1bb484d")
SET(HARFBUZZ_URL "https://github.com/harfbuzz/harfbuzz/releases/download/9.0.0/harfbuzz-9.0.0.tar.xz")
SET(HARFBUZZ_HASH "SHA256=a41b272ceeb920c57263ec851604542d9ec85ee3030506d94662067c7b6ab89e")

set(FT_PATCH
${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/stb_image.h <SOURCE_DIR>/src/sfnt &&
${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pngshim.c <SOURCE_DIR>/src/sfnt)


include(GNUInstallDirs)

if(MSVC)
    set(harfbuzz_lib_name harfbuzz.lib)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(freetype_lib_name freetyped.lib)
    else()
        set(freetype_lib_name freetype.lib)
    endif()
else()
    set(freetype_lib_name libfreetype.a)
    set(harfbuzz_lib_name libharfbuzz.a)
endif()
    include(ExternalProject)
    #Stage 1 build Freetype
    ExternalProject_Add(
        FT1
        URL "${FREETYPE_URL}"
        URL_HASH "${FREETYPE_HASH}"
        DOWNLOAD_DIR "${FT_DOWNLOADS_DIR}"
        PATCH_COMMAND "${FT_PATCH}"
        BUILD_BYPRODUCTS <INSTALL_DIR>/${CMAKE_INSTALL_LIBDIR}/${freetype_lib_name}
        CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_POLICY_VERSION_MINIMUM=3.15
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_MODULE_PATH=${CMAKE_CURRENT_LIST_DIR}
        -DFT_REQUIRE_PNG=True
        -DFT_DISABLE_HARFBUZZ=True
        -DFT_DISABLE_BZIP2=True
        -DFT_DISABLE_ZLIB=True
        -DFT_DISABLE_BROTLI=True
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
    )

    ExternalProject_Get_Property(FT1 install_dir)
    set(FT1_DIR ${install_dir})

    #Build Harfbuzz
    ExternalProject_Add(
        HB
        URL "${HARFBUZZ_URL}"
        URL_HASH "${HARFBUZZ_HASH}"
        DOWNLOAD_DIR "${FT_DOWNLOADS_DIR}"
        BUILD_BYPRODUCTS <INSTALL_DIR>/${CMAKE_INSTALL_LIBDIR}/${harfbuzz_lib_name}
        DEPENDS FT1
        CMAKE_ARGS
        -DCMAKE_PREFIX_PATH=${FT1_DIR}
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_MODULE_PATH=${CMAKE_CURRENT_LIST_DIR}
        -DHB_HAVE_FREETYPE=ON
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -G ${CMAKE_GENERATOR}
    )

    ExternalProject_Get_Property(HB INSTALL_DIR)
    set(HARFBUZZ_INSTALL_DIR ${INSTALL_DIR})
    set(FT2_MODULES ${CMAKE_CURRENT_LIST_DIR};${HARFBUZZ_INSTALL_DIR})

    # Create full FT
    ExternalProject_Add(
        FT2
        URL "${FREETYPE_URL}"
        URL_HASH "${FREETYPE_HASH}"
        DOWNLOAD_DIR "${FT_DOWNLOADS_DIR}"
        PATCH_COMMAND "${FT_PATCH}"
        DEPENDS HB
        BUILD_BYPRODUCTS <INSTALL_DIR>/${CMAKE_INSTALL_LIBDIR}/${freetype_lib_name}
        CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_POLICY_VERSION_MINIMUM=3.15
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_PREFIX_PATH=${HARFBUZZ_INSTALL_DIR}
        -DCMAKE_MODULE_PATH=${FT2_MODULES}
        -DFT_REQUIRE_HARFBUZZ=True
        -DFT_REQUIRE_PNG=True
        -DFT_DISABLE_BZIP2=True
        -DFT_DISABLE_ZLIB=True
        -DFT_DISABLE_BROTLI=True
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
        -DCMAKE_VERBOSE_BUILD=ON
        -G ${CMAKE_GENERATOR}
    )

    ExternalProject_Get_Property(FT2 INSTALL_DIR)
    set(FREETYPE_INSTALL_DIR ${INSTALL_DIR})
    install(
        DIRECTORY ${FREETYPE_INSTALL_DIR}/
        DESTINATION "."
        USE_SOURCE_PERMISSIONS # Remain permissions (rwx) for installed files
    )
    install(
        DIRECTORY ${HARFBUZZ_INSTALL_DIR}/
        DESTINATION "."
        USE_SOURCE_PERMISSIONS # Remain permissions (rwx) for installed files
    )
