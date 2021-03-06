cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(ryu_cmake LANGUAGES C)

add_library(ryu STATIC
        ryu/ryu.h ryu/common.h ryu/f2s.c ryu/d2s.c
        ryu/f2s_intrinsics.h ryu/f2s_full_table.h
        ryu/d2s_intrinsics.h ryu/d2s_full_table.h
        ryu/d2fixed.c ryu/d2fixed_full_table.h
        ryu/ryu_parse.h ryu/s2d.c ryu/s2f.c
        ryu/digit_table.h)
add_library(ryu::ryu ALIAS ryu)

target_compile_features(ryu PUBLIC cxx_std_17)

target_include_directories(ryu
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)

# ---- MSVC Specifics ----
if (MSVC)
    # No need to not generate PDB
    target_compile_options(ryu PUBLIC
              /Zi $<$<CONFIG:Release>:/GL>)
endif()
set_target_properties(ryu PROPERTIES FOLDER fp/extern)