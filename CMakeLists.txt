cmake_minimum_required(VERSION 3.16)
project(curlib LANGUAGES C)

add_library(curlib SHARED cur.c)
target_include_directories(curlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(curlib PRIVATE c_std_99)
target_compile_definitions(curlib PRIVATE CURLIB_BUILD)

if(MSVC)
    target_compile_definitions(curlib PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(curlib PRIVATE /utf-8)
endif()

add_library(curlib_static STATIC cur.c)
target_include_directories(curlib_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(curlib_static PRIVATE c_std_99)
target_compile_definitions(curlib_static PUBLIC CURLIB_STATIC)

if(MSVC)
    target_compile_options(curlib_static PRIVATE /utf-8)
endif()
