cmake_minimum_required(VERSION 3.15)
project(blurgtext)

option(BT_BUILD_DEMO "Build demo program" ON)
option(BT_MINGW_BUNDLE_LIBGCC "Statically link libgcc on windows builds" ON)

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

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    #remove lib prefix on windows mingw
    set_target_properties(blurgtext PROPERTIES PREFIX "")
    #enable directwrite (both mingw and MSVC)
    target_compile_definitions(blurgtext PRIVATE -DBT_ENABLE_DIRECTWRITE=1)
    target_link_libraries(blurgtext PRIVATE gdi32 dwrite)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU" AND BT_MINGW_BUNDLE_LIBGCC)
    # link libgcc/libstdc++ into our .dll
    target_link_options(blurgtext PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

set_target_properties(blurgtext PROPERTIES C_VISIBILITY_PRESET hidden)
target_compile_definitions(blurgtext PRIVATE -DBUILDING_BLURG)
target_include_directories(blurgtext PUBLIC "include")

# libunibreak
file(GLOB_RECURSE UNIBREAK_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "deps/libunibreak/src/*.c")
add_library(unibreak STATIC
    ${UNIBREAK_SOURCES}
)
target_include_directories(unibreak PUBLIC "deps/libunibreak/src")
set_property(TARGET unibreak PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(unibreak PROPERTIES C_VISIBILITY_PRESET hidden)
target_link_libraries(blurgtext PRIVATE unibreak)

# raqm, Freetype, Harfbuzz Dependencies
add_library(raqm STATIC
    deps/libraqm/src/raqm.c
)
set_property(TARGET raqm PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(raqm PROPERTIES C_VISIBILITY_PRESET hidden)
set(RAQM_VERSION "0.10.1")
set(RAQM_VERSION_MAJOR "0")
set(RAQM_VERSION_MINOR "10")
set(RAQM_VERSION_MICRO "1")

configure_file(deps/libraqm/src/raqm-version.h.in raqm-version.h)

if(WIN32)
    include(ExternalProject)

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
        BUILD_BYPRODUCTS <INSTALL_DIR>/lib/${freetype_lib_name} <INSTALL_DIR>/lib/${harfbuzz_lib_name}
        CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -G ${CMAKE_GENERATOR}
    )
    ExternalProject_Get_Property(FTHB INSTALL_DIR)
    set(FT_HB_INSTALL_DIR ${INSTALL_DIR})

    

    add_library(FREETYPE_LIBRARY STATIC IMPORTED GLOBAL)
    set_property(TARGET FREETYPE_LIBRARY PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${FT_HB_INSTALL_DIR}/include/freetype2")
    file(MAKE_DIRECTORY ${FT_HB_INSTALL_DIR}/include/freetype2)
    set_property(TARGET FREETYPE_LIBRARY PROPERTY IMPORTED_LOCATION "${FT_HB_INSTALL_DIR}/lib/${freetype_lib_name}")
    add_dependencies(FREETYPE_LIBRARY FTHB)

    add_library(HARFBUZZ_LIBRARY STATIC IMPORTED GLOBAL)
    set_property(TARGET HARFBUZZ_LIBRARY PROPERTY IMPORTED_LOCATION "${FT_HB_INSTALL_DIR}/lib/${harfbuzz_lib_name}")
    file(MAKE_DIRECTORY ${FT_HB_INSTALL_DIR}/include/harfbuzz)
    set_property(TARGET HARFBUZZ_LIBRARY PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${FT_HB_INSTALL_DIR}/include/harfbuzz")
    add_dependencies(HARFBUZZ_LIBRARY FTHB)

    # Use SheenBidi bidirectional library
    add_library(sheenbidi STATIC
        deps/SheenBidi/Source/SheenBidi.c
    )
    set_property(TARGET sheenbidi PROPERTY POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(sheenbidi PRIVATE -DSB_CONFIG_UNITY)
    target_include_directories(sheenbidi PUBLIC deps/SheenBidi/Headers)
    target_compile_definitions(raqm PRIVATE -DRAQM_SHEENBIDI)
    target_include_directories(raqm PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "deps/libraqm/src")
    target_link_libraries(raqm PRIVATE HARFBUZZ_LIBRARY FREETYPE_LIBRARY sheenbidi)
    # link to main project
    target_link_libraries(blurgtext PRIVATE raqm HARFBUZZ_LIBRARY FREETYPE_LIBRARY sheenbidi)
else()
    find_package(Freetype REQUIRED)
    # Use pkgconfig to get harfbuzz and fribidi
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
    pkg_check_modules(FRIBIDI REQUIRED IMPORTED_TARGET fribidi)
    pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
    target_link_libraries(raqm PRIVATE ${FREETYPE_LIBRARIES} PkgConfig::HARFBUZZ PkgConfig::FRIBIDI)
    target_include_directories(raqm PRIVATE ${FREETYPE_INCLUDE_DIRS} ${HARFBUZZ_INCLUDE_DIRS})
    target_include_directories(raqm PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "deps/libraqm/src")
    # link to main project
    target_compile_definitions(blurgtext PRIVATE -DBT_ENABLE_FONTCONFIG=1)
    target_link_libraries(blurgtext PRIVATE raqm ${FREETYPE_LIBRARIES} PkgConfig::HARFBUZZ PkgConfig::FONTCONFIG)
    target_include_directories(blurgtext PRIVATE ${FREETYPE_INCLUDE_DIRS} ${HARFBUZZ_INCLUDE_DIRS})
endif()

if(BT_BUILD_DEMO)
    add_subdirectory(demo)
endif()

