cmake_minimum_required(VERSION 3.18)
project(mnb_mp_module_filter C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 1. M&BTool NativeMods ET_REL object (mod.arm64.o)
#    Must NOT be linked into a .so; loaded directly by libmodlauncher.so.
add_library(mp_module_filter_etrel OBJECT mod_etrel.c)
target_compile_options(mp_module_filter_etrel PRIVATE
    -O2
    -Wall
    -Wextra
    -Werror
    -fno-PIC
    -fno-common
    -fno-stack-protector
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-optimize-sibling-calls
    $<$<C_COMPILER_ID:Clang>:-fno-sanitize=safe-stack>
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mod.arm64.o
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_OBJECTS:mp_module_filter_etrel> ${CMAKE_CURRENT_BINARY_DIR}/mod.arm64.o
    DEPENDS mp_module_filter_etrel
    COMMENT "Emitting ET_REL mod.arm64.o"
)
add_custom_target(mod_arm64_o ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mod.arm64.o)

# 2. Standalone Android NDK shared library (libmp_module_filter.so)
add_library(mp_module_filter SHARED mod_so.c)
target_compile_options(mp_module_filter PRIVATE -O2 -Wall -Wextra -Werror)
if(ANDROID)
    target_link_libraries(mp_module_filter PRIVATE log dl)
else()
    target_link_libraries(mp_module_filter PRIVATE dl)
endif()
