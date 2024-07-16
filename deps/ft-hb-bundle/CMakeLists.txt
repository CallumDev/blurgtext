 cmake_minimum_required(VERSION 3.15)
 project(ft-hb-bundle)

 SET(FREETYPE_URL "https://download.savannah.gnu.org/releases/freetype/freetype-2.13.2.tar.gz")
 SET(HARFBUZZ_URL "https://github.com/harfbuzz/harfbuzz/releases/download/9.0.0/harfbuzz-9.0.0.tar.xz")
 if(MSVC)
        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(freetype_lib_name freetyped.lib)
            set(harfbuzz_lib_name harfbuzzd.lib)
        else()
            set(freetype_lib_name freetype.lib)
            set(harfbuzz_lib_name harfbuzz.lib)
        endif()
    else()
        set(freetype_lib_name libfreetype.a)
        set(harfbuzz_lib_name libharfbuzz.a)
    endif()
    list(APPEND freetype_byproduct
        <INSTALL_DIR>/lib/${freetype_lib_name}
    )
    list(APPEND harfbuzz_byproduct
        <INSTALL_DIR>/lib/${harfbuzz_lib_name}
    )
    include(ExternalProject)
    #Stage 1 build Freetype
    ExternalProject_Add(
        FREETYPE_NO_HARFBUZZ
        URL "${FREETYPE_URL}"
        CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DFT_DISABLE_HARFBUZZ=True
        -DFT_DISABLE_BZIP2=True
        -DFT_DISABLE_ZLIB=True
        -DFT_DISABLE_PNG=True
        -DFT_DISABLE_BROTLI=True
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
        BUILD_BYPRODUCTS ${freetype_byproduct}
    )

    ExternalProject_Get_Property(FREETYPE_NO_HARFBUZZ install_dir)
    set(FREETYPE_NO_HARFBUZZ_DIR ${install_dir})

    #Build Harfbuzz
    ExternalProject_Add(
        HARFBUZZ_PROJECT
        URL "${HARFBUZZ_URL}"
        DEPENDS FREETYPE_NO_HARFBUZZ
        CMAKE_ARGS
        -DCMAKE_PREFIX_PATH=${FREETYPE_NO_HARFBUZZ_DIR}
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DHB_HAVE_FREETYPE=On
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
        BUILD_BYPRODUCTS ${harfbuzz_byproduct}
    )

    ExternalProject_Get_Property(HARFBUZZ_PROJECT INSTALL_DIR)
    set(HARFBUZZ_INSTALL_DIR ${INSTALL_DIR})
    #add_library(HARFBUZZ_LIBRARY STATIC IMPORTED GLOBAL)
    #set_property(TARGET HARFBUZZ_LIBRARY PROPERTY IMPORTED_LOCATION "${HARFBUZZ_INSTALL_DIR}/lib/${harfbuzz_lib_name}")
    #file(MAKE_DIRECTORY ${HARFBUZZ_INSTALL_DIR}/include/harfbuzz)
    #set_property(TARGET HARFBUZZ_LIBRARY PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${HARFBUZZ_INSTALL_DIR}/include/harfbuzz")
    #add_dependencies(HARFBUZZ_LIBRARY HARFBUZZ_PROJECT)

    # Create full FT
    ExternalProject_Add(
        FREETYPE_PROJECT
        URL "${FREETYPE_URL}"
        DEPENDS HARFBUZZ_PROJECT
        CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_PREFIX_PATH=${HARFBUZZ_INSTALL_DIR}
        -DCMAKE_MODULE_PATH=${HARFBUZZ_INSTALL_DIR}
        -DFT_REQUIRE_HARFBUZZ=True
        -DFT_DISABLE_BZIP2=True
        -DFT_DISABLE_ZLIB=True
        -DFT_DISABLE_PNG=True
        -DFT_DISABLE_BROTLI=True
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
        BUILD_BYPRODUCTS ${freetype_byproduct}
    )

    ExternalProject_Get_Property(FREETYPE_PROJECT INSTALL_DIR)
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
