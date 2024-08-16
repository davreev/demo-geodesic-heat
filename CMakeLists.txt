cmake_minimum_required(VERSION 3.14)
project(
    geodesic-heat
    VERSION 0.5.0
    LANGUAGES C CXX
)

#
# Global config
#

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

list(
    PREPEND CMAKE_MODULE_PATH 
    "${CMAKE_CURRENT_LIST_DIR}/cmake"
)

# Show download progress
set(FETCHCONTENT_QUIET FALSE)

#
# Main target
#

set(app_name geodesic-heat)

add_executable(
    ${app_name}
    "src/assets.cpp"
    "src/graphics.c"
    "src/graphics.cpp"
    "src/impl.cpp"
    "src/main.cpp"
    "src/scene.cpp"
    "src/tasks.cpp"
)

include(deps/dr-app)
include(deps/happly)
include(deps/stb-image)
target_link_libraries(
    ${app_name}
    PRIVATE
        dr::app
        happly::happly
        stb::image
)

target_compile_options(
    ${app_name}
    PRIVATE 
        -Wall -Wextra -Wpedantic -Werror
)

if(EMSCRIPTEN)
    # Emscripten compiler options
    target_link_options(
        ${app_name}
        PRIVATE
            "-sUSE_WEBGL2=1"
            "-sALLOW_MEMORY_GROWTH"
            "-sFORCE_FILESYSTEM=1"
            "-sPTHREAD_POOL_SIZE_STRICT=1"
            "-sPTHREAD_POOL_SIZE=1" # Mirrors arg passed to thread_pool_start
            "-sALLOW_BLOCKING_ON_MAIN_THREAD=0"
            "-sSTACK_SIZE=1mb" # https://groups.google.com/g/emscripten-discuss/c/MgHWuq2oq7Q
            "$<$<CONFIG:Debug>:-sASSERTIONS=2>"
            "$<$<CONFIG:Debug>:-sSTACK_OVERFLOW_CHECK=2>"
            "$<$<CONFIG:Debug>:-sPTHREADS_DEBUG=1>"
            "$<$<CONFIG:Debug>:-gsource-map>"
            "$<$<CONFIG:Debug>:--threadprofiler>"
    )
endif()

#
# Post-build commands
#

include(app-utils)
set(src_dir "${CMAKE_CURRENT_SOURCE_DIR}")
set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/gen")
get_default_runtime_output_dir(runtime_output_dir)

set(
    glsl_files
    "${src_dir}/assets/shaders/contour_color.vert.glsl"
    "${src_dir}/assets/shaders/contour_color.frag.glsl"
    "${src_dir}/assets/shaders/contour_line.vert.glsl"
    "${src_dir}/assets/shaders/contour_line.frag.glsl"
    # ...
)

set(gen_asset_files)
convert_shaders(gen_asset_files)
# ...

set(
    asset_files
    "${src_dir}/assets/images/matcap-white-soft.png"
    "${src_dir}/assets/models/torus.ply"
    "${src_dir}/assets/models/double-torus.ply"
    "${src_dir}/assets/models/triple-torus.ply"
    "${src_dir}/assets/models/chen-gackstatter-surface.ply"
    "${src_dir}/assets/models/node-cluster.ply"
    "${src_dir}/assets/models/armadillo-remesh.ply"
    ${gen_asset_files}
    # ...
)

if(EMSCRIPTEN)
    set(
        web_files
        "${src_dir}/web/index.html"
        "${src_dir}/web/coi-serviceworker.js"
    )
    copy_web_files()
    package_assets()
else()
    copy_assets()
endif()
