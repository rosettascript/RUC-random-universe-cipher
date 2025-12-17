cmake_minimum_required(VERSION 3.20)
project(ruc_cpp_wasm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emscripten toolchain
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -flto -fno-exceptions")
    # Linker flags (not compiler flags)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s WASM=1 -s EXPORT_ES6=1 -s MODULARIZE=1 -s EXPORT_NAME=createModule")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s MAXIMUM_MEMORY=2GB")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s EXPORTED_FUNCTIONS='[\"_malloc\",\"_free\",\"_ruc_expand_key\",\"_ruc_free_key_material\",\"_ruc_encrypt_block\",\"_ruc_decrypt_block\",\"_ruc_encrypt_blocks_batch\",\"_ruc_decrypt_blocks_batch\",\"_ruc_get_profile_stats\"]'")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\",\"UTF8ToString\",\"stringToUTF8\",\"HEAP8\",\"HEAPU8\",\"HEAP32\",\"HEAPU32\"]'")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --no-entry")
endif()

# Source files
set(SOURCES
    src/ruc_cipher.cpp
    src/gf_math.cpp
    src/shake256.cpp
    src/chacha20.cpp
    src/sbox.cpp
)

add_executable(ruc_wasm ${SOURCES})

