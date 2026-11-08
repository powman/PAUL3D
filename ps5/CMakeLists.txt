# PAUL3D: the text engine (llama.cpp's src/ on ggml with its Vulkan backend on RADV) as static libraries for a PS5 title.
# scripts/build-mg.sh configures this with PS5SD's toolchain.cmake; GGML_SRC is the unified ggml (scripts/ggml-unified.sh),
# LLAMA_SRC llama.cpp (scripts/llama-fetch.sh, never its own ggml).
cmake_minimum_required(VERSION 3.18)
project(paul3d C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_definitions(GGML_MAX_NAME=128)
add_subdirectory(${GGML_SRC} ggml)

file(GLOB LLAMA_FILES ${LLAMA_SRC}/src/*.cpp ${LLAMA_SRC}/src/models/*.cpp)
file(WRITE ${CMAKE_BINARY_DIR}/gen/llama-version.h "#pragma once\n#define LLAMA_VERSION \"paul3d\"\n#define LLAMA_COMMIT \"4ed1c85\"\n")
add_library(paul3d_text STATIC ${LLAMA_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/mg_text.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mg_textjob.cpp ${CMAKE_CURRENT_SOURCE_DIR}/mg_queue.cpp ${CMAKE_CURRENT_SOURCE_DIR}/mg_download.cpp)
target_include_directories(paul3d_text PUBLIC ${LLAMA_SRC}/include ${LLAMA_SRC}/vendor PRIVATE ${LLAMA_SRC}/src ${CMAKE_BINARY_DIR}/gen)
target_compile_options(paul3d_text PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/paul3d_threads.h)
target_link_libraries(paul3d_text PUBLIC ggml ggml-base ggml-cpu ggml-vulkan)
