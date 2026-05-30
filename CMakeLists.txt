cmake_minimum_required(VERSION 3.15)

# Semantic version for this build. CI injects the tag the release workflow
# computed from the commit history (-DPROJECT_VERSION=X.Y.Z); local builds fall
# back to 0.0.0. Must be plain numeric X.Y.Z for project(VERSION ...).
set(PROJECT_VERSION "0.0.0" CACHE STRING "Semantic version for this build")

# RC enables the Windows version-info resource (compiled by rc.exe on MSVC and
# windres on MinGW) so the .asi carries a File version in its properties.
project(007FirstLightUltrawide VERSION ${PROJECT_VERSION} LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fill the version resource template with the numbers project() parsed out.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    @ONLY)

# ---- The ASI ------------------------------------------------
add_library(007FirstLightUltrawide SHARED
    dllmain.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc")

# Expose the version to the code (logged in the .log header).
target_compile_definitions(007FirstLightUltrawide PRIVATE
    PATCH_VERSION="${PROJECT_VERSION}")

set_target_properties(007FirstLightUltrawide PROPERTIES
    OUTPUT_NAME "007FirstLightUltrawide"
    PREFIX ""                 # no "lib" prefix
    SUFFIX ".asi"             # .asi instead of .dll
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"  # output to bin directory
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"  # output to bin directory
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/implib"  # tuck away mingw import lib
)

# Statically link the C++ runtime so the .asi has no external dependencies.
set_property(TARGET 007FirstLightUltrawide PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# Compiler-specific flags. Do NOT force /O2 on MSVC - the config controls
# optimization there, and /O2 + Debug's /RTC1 are incompatible (D8016).
target_compile_options(007FirstLightUltrawide PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-O2;-Wall;-Wextra>
        $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->
)

target_link_options(007FirstLightUltrawide PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-static;-static-libgcc;-static-libstdc++;-s>
)

target_link_libraries(007FirstLightUltrawide PRIVATE kernel32)

# ---- Generate the ini beside the .asi --------------------------------------
# The content lives here (no external template file to copy or forget). It is
# written into the target's output directory, so it lands next to the .asi for
# single-config (build/) and multi-config (build/Release/) generators alike.
set(INI_CONTENT [=[; ============================================================
;  007 First Light - Ultrawide Cutscene Patch
;  Configuration file
; ============================================================

[General]
; Master switch. Set to 0 (or false/no/off) to make the ASI do nothing
; at all - it stays loaded but applies no patches. Set to 1 to enable.
Enabled = 1

[Aspect]
; Set EITHER your monitor resolution (Width + Height) OR an explicit
; AspectRatio. If AspectRatio is greater than 0 it takes priority and
; Width/Height are ignored.
;
; The patch computes Width / Height as a 32-bit float and writes it over
; the game's stored 16:9 value (1.777778) wherever it appears in memory.

; --- Option A: enter your native resolution -----------------
Width  = 3440
Height = 1440

; --- Option B: force a specific ratio (0 = disabled) --------
; Examples:  2.388889 (21:9 3440x1440)
;            2.370370 (21:9 2560x1080)
;            3.555556 (32:9 3840x1080 / 5120x1440)
AspectRatio = 0

[Advanced]
; The aspect-ratio value the patch searches for in memory. Leave at 0 to use
; the game's built-in 16:9 value (1.777778). Only change this if a game update
; changes the stored value and the log reports "Occurrences patched: 0".
SearchAspect = 0
]=])

file(GENERATE
        OUTPUT  "$<TARGET_FILE_DIR:007FirstLightUltrawide>/007FirstLightUltrawide.ini"
        CONTENT "${INI_CONTENT}")
