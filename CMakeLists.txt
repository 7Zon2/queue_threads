cmake_minimum_required(VERSION 3.20)

project(proj)

add_library(flags INTERFACE)

set(gcc_like_cxx "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>")
set(msvc_cxx "$<COMPILE_LANG_AND_ID:CXX,MSVC>")


target_compile_options(flags INTERFACE 
                        "$<${gcc_like_cxx}:$<BUILD_INTERFACE:-Wall;-Wextra;>>"
                        "$<${msvc_cxx}:$<BUILD_INTERFACE:-W3>>"
)

target_compile_features(flags INTERFACE cxx_std_20)


add_executable(main main.cpp)


target_include_directories(main PRIVATE include)

target_link_libraries(main flags)