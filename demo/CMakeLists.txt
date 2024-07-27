cmake_minimum_required(VERSION 3.15)
project(blurgtest)

add_executable(blurgtest
  main.c
  glad/src/glad.c
)
configure_file(Roboto-Regular.ttf Roboto-Regular.ttf COPYONLY)
configure_file(Roboto-Bold.ttf Roboto-Bold.ttf COPYONLY)
configure_file(Roboto-Italic.ttf Roboto-Italic.ttf COPYONLY)
configure_file(Roboto-BoldItalic.ttf Roboto-BoldItalic.ttf COPYONLY)
configure_file(Roboto-ThinItalic.ttf Roboto-ThinItalic.ttf COPYONLY)
configure_file(Roboto-Black.ttf Roboto-Black.ttf COPYONLY)


if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    set(IS_MINGW_BUILD "Y")
    target_link_options(blurgtest PUBLIC -static-libgcc -static-libstdc++ -static)
else()
    set(IS_MINGW_BUILD "N")
endif()

target_link_libraries(blurgtest PRIVATE blurgtext)
target_include_directories(blurgtest PRIVATE "./glad/include")

 # SDL2 Dependency
if (DEFINED SDL2_INCLUDE_DIRS AND DEFINED SDL2_LIBRARIES)
message(STATUS "blurgtest: using pre-defined SDL2 variables SDL2_INCLUDE_DIRS and SDL2_LIBRARIES")
target_include_directories(blurgtest PRIVATE "$<BUILD_INTERFACE:${SDL2_INCLUDE_DIRS}>")
target_link_libraries(blurgtest PRIVATE ${SDL2_LIBRARIES})
else()
# Only try to autodetect if both SDL2 variables aren't explicitly set
find_package(SDL2 REQUIRED)
if (TARGET SDL2::SDL2)
    message(STATUS "blurgtest: using TARGET SDL2::SDL2")
    if("${IS_MINGW_BUILD}" STREQUAL "Y")
      target_link_libraries(blurgtest PRIVATE SDL2::SDL2main SDL2::SDL2-static)
    else()
      target_link_libraries(blurgtest PRIVATE SDL2::SDL2main SDL2::SDL2)
    endif()
elseif (TARGET SDL2)
    message(STATUS "blurgtest: using TARGET SDL2")
    target_link_libraries(blurgtest PRIVATE SDL2main SDL2)
else()
    message(STATUS "blurgtest: no TARGET SDL2::SDL2, or SDL2, using variables")
    target_include_directories(testapp PRIVATE "$<BUILD_INTERFACE:${SDL2_INCLUDE_DIRS}>")
    target_link_libraries(blurgtest PRIVATE ${SDL2_LIBRARIES})
endif()
endif()

if(WIN32)
add_custom_command(TARGET blurgtest POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:blurgtest> $<TARGET_FILE_DIR:blurgtest>
  COMMAND_EXPAND_LISTS
  )
endif()