# EMSDK_DIR is expected to be the absolute path of the Emscripten SDK root dir

# Install and activate sdks/tools
& $EMSDK_DIR/emsdk.ps1 install latest
& $EMSDK_DIR/emsdk.ps1 activate latest

# Setup environment variables
. $EMSDK_DIR/emsdk_env.ps1

# Set the CMake toolchain file
$CMAKE_TOOLCHAIN_FILE="$EMSDK_DIR/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"