# EMSDK_DIR is expected to be the absolute path of the Emscripten SDK root dir

# Install and activate sdks/tools
$EMSDK_DIR/emsdk install latest
$EMSDK_DIR/emsdk activate latest

# Setup environment variables
source $EMSDK_DIR/emsdk_env.sh
