cmake_minimum_required(VERSION 3.15)
project(blurgtext)

option(BT_BUILD_DEMO "Build demo program" ON)
option(BT_MINGW_BUNDLE_LIBGCC "Statically link libgcc on windows builds" ON)
option(BT_ENABLE_SYSTEMFONTS "Enable loading system fonts" ON)
option(FT_DOWNLOADS_DIR "Folder to contain downloaded source files" "${CMAKE_CURRENT_BINARY_DIR}/sources")

if(APPLE OR NOT UNIX)
    set(BT_BUILD_FTHB_DEFAULT ON)
else()
    set(BT_BUILD_FTHB_DEFAULT OFF)
endif()
option(BT_BUILD_FTHB "Build statically linked freetype+harfbuzz" ${BT_BUILD_FTHB_DEFAULT})

add_library(blurgtext SHARED
    src/blurgtext.c
    src/hashmap.c
    src/glyphatlas.c
    src/fontmanager.c
    src/font.c
    src/util.c
    src/sysfonts_fontconfig.c
    src/sysfonts_directwrite.cpp
)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# blurgtext needs lib prefix on all platforms
set_target_properties(blurgtext PROPERTIES PREFIX "lib")

# Apple specific libraries to be linked.
if(APPLE)
  target_link_libraries(blurgtext PRIVATE "-framework CoreFoundation" "-framework CoreText" "-framework CoreGraphics")
endif()

# Platform specific system font enablement
if(BT_ENABLE_SYSTEMFONTS)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        #enable directwrite (both mingw and MSVC)
        target_compile_definitions(blurgtext PRIVATE -DBT_ENABLE_DIRECTWRITE=1)
        target_link_libraries(blurgtext PRIVATE gdi32 dwrite)
    elseif(UNIX AND NOT APPLE)
        if(BT_BUILD_FTHB)
            message(FATAL_ERROR "blurgtext: Fontconfig platforms cannot have BT_ENABLE_SYSTEMFONTS and BT_BUILD_FTHB at the same time")
        endif()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
        target_compile_definitions(blurgtext PRIVATE -DBT_ENABLE_FONTCONFIG=1)
        target_link_libraries(blurgtext PRIVATE PkgConfig::FONTCONFIG)
    elseif(APPLE)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
        target_compile_definitions(blurgtext PRIVATE -DBT_ENABLE_FONTCONFIG=1)
        target_link_libraries(blurgtext PRIVATE PkgConfig::FONTCONFIG)
    endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU" AND BT_MINGW_BUNDLE_LIBGCC)
    # link libgcc/libstdc++ into our .dll
    target_link_options(blurgtext PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

set_target_properties(blurgtext PROPERTIES C_VISIBILITY_PRESET hidden)
target_compile_definitions(blurgtext PRIVATE -DBUILDING_BLURG)
target_include_directories(blurgtext PUBLIC "include")

# plutosvg
set(PLUTOSVG_BUILD_EXAMPLES OFF CACHE BOOL "plutosvg demo" FORCE)
set(PLUTOVG_BUILD_EXAMPLES OFF CACHE BOOL "plutovg demo" FORCE)
add_subdirectory(deps/plutosvg)
target_link_libraries(blurgtext PRIVATE plutosvg)

# libunibreak
file(GLOB_RECURSE UNIBREAK_SOURCES RELATIVE ${CMAKE_CURRENT_LIST_DIR} "deps/libunibreak/src/*.c")
add_library(unibreak STATIC
    ${UNIBREAK_SOURCES}
)
target_include_directories(unibreak PUBLIC "deps/libunibreak/src")
set_target_properties(unibreak PROPERTIES C_VISIBILITY_PRESET hidden)
target_link_libraries(blurgtext PRIVATE unibreak)

# raqm, sheenbidi
add_library(raqm STATIC
    deps/libraqm/src/raqm.c
)
set_target_properties(raqm PROPERTIES C_VISIBILITY_PRESET hidden)
set(RAQM_VERSION "0.10.1")
set(RAQM_VERSION_MAJOR "0")
set(RAQM_VERSION_MINOR "10")
set(RAQM_VERSION_MICRO "1")

configure_file(deps/libraqm/src/raqm-version.h.in raqm-version.h)

# Use SheenBidi bidirectional library
add_library(sheenbidi STATIC
    deps/SheenBidi/Source/SheenBidi.c
)
set_property(TARGET sheenbidi PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(sheenbidi PRIVATE -DSB_CONFIG_UNITY)
target_include_directories(sheenbidi PUBLIC deps/SheenBidi/Headers)
target_compile_definitions(raqm PRIVATE -DRAQM_SHEENBIDI)
target_include_directories(raqm PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "deps/libraqm/src")

# Freetype and Harfbuzz
if(BT_BUILD_FTHB)
    include(ExternalProject)
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
    # Short name to avoid MAX_PATH errors on windows/msvc
    ExternalProject_Add(FTHB
        SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/deps/ft-hb-bundle"
        BUILD_BYPRODUCTS <INSTALL_DIR>/${CMAKE_INSTALL_LIBDIR}/${freetype_lib_name} <INSTALL_DIR>/${CMAKE_INSTALL_LIBDIR}/${harfbuzz_lib_name}
        CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DFT_DOWNLOADS_DIR=${FT_DOWNLOADS_DIR}
        -G ${CMAKE_GENERATOR}
    )
    ExternalProject_Get_Property(FTHB INSTALL_DIR)
    set(FT_HB_INSTALL_DIR ${INSTALL_DIR})

    add_library(FREETYPE_LIBRARY STATIC IMPORTED GLOBAL)
    set_property(TARGET FREETYPE_LIBRARY PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${FT_HB_INSTALL_DIR}/include/freetype2")
    file(MAKE_DIRECTORY ${FT_HB_INSTALL_DIR}/include/freetype2)
    set_property(TARGET FREETYPE_LIBRARY PROPERTY IMPORTED_LOCATION "${FT_HB_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}/${freetype_lib_name}")
    add_dependencies(FREETYPE_LIBRARY FTHB)

    add_library(HARFBUZZ_LIBRARY STATIC IMPORTED GLOBAL)
    set_property(TARGET HARFBUZZ_LIBRARY PROPERTY IMPORTED_LOCATION "${FT_HB_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}/${harfbuzz_lib_name}")
    file(MAKE_DIRECTORY ${FT_HB_INSTALL_DIR}/include/harfbuzz)
    set_property(TARGET HARFBUZZ_LIBRARY PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${FT_HB_INSTALL_DIR}/include/harfbuzz")
    add_dependencies(HARFBUZZ_LIBRARY FTHB)

    message(STATUS "blurgtext: building vendored Freetype and Harfbuzz")
    # link to main project
    target_link_libraries(raqm PRIVATE HARFBUZZ_LIBRARY FREETYPE_LIBRARY sheenbidi)
    target_link_libraries(blurgtext PRIVATE raqm HARFBUZZ_LIBRARY FREETYPE_LIBRARY sheenbidi)
else()
    find_package(Freetype REQUIRED)
    # Use pkgconfig to get harfbuzz
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
    target_link_libraries(raqm PRIVATE ${FREETYPE_LIBRARIES} PkgConfig::HARFBUZZ sheenbidi)
    target_include_directories(raqm PRIVATE ${FREETYPE_INCLUDE_DIRS} ${HARFBUZZ_INCLUDE_DIRS})
    target_include_directories(raqm PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "deps/libraqm/src")
    # link to main project
    target_link_libraries(blurgtext PRIVATE raqm ${FREETYPE_LIBRARIES} PkgConfig::HARFBUZZ sheenbidi)
    target_include_directories(blurgtext PRIVATE ${FREETYPE_INCLUDE_DIRS} ${HARFBUZZ_INCLUDE_DIRS})
endif()


if(BT_BUILD_DEMO)
    add_subdirectory(demo)
endif()
