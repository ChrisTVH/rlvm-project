# write_boost_user_config.cmake
# Called by ExternalProject during boost install step to write user-config.jam.
# Variables passed via -D:
#   FILE    - path to write to (e.g. <boost-source>/user-config.jam)
#   NDK_CC  - absolute path to the NDK clang cross-compiler

if(NOT DEFINED FILE OR NOT DEFINED NDK_CC)
    message(FATAL_ERROR "write_boost_user_config.cmake: FILE and NDK_CC must be defined")
endif()

# Escape backslashes in the path (Windows safety; no-op on Linux)
string(REPLACE "\\" "/" NDK_CC_ESCAPED "${NDK_CC}")

# Write user-config.jam.
# The 'clang-android' version label matches 'toolset=clang-android' in the b2 invocation.
file(WRITE "${FILE}"
"using clang : android : ${NDK_CC_ESCAPED} ;\n"
)

message(STATUS "Wrote boost user-config.jam: ${FILE}")
message(STATUS "  toolset entry: using clang : android : ${NDK_CC_ESCAPED} ;")
